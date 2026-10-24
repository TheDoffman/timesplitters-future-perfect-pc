/* Darwin host adapter for local reports. The faulting thread only copies its
 * register context into preallocated shared memory, notifies and waits boundedly.
 * Hashing, symbolication, log tails and hang samples happen in a separate process. */
#include "crash_posix.h"
#include "../pc_hang_policy.h"
#include "../pc_breadcrumbs.h"
#include "../pc_capture_health.h"
#include <CommonCrypto/CommonDigest.h>
#ifndef TSFP_CRASH_TEST
#include <CoreGraphics/CoreGraphics.h>
#endif
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct Capture {
    atomic_int entered,done,armed,focused,inhibited,stopping;
    atomic_ullong frames;
    PcBreadcrumbs breadcrumbs; uint64_t started_ms;
    int pid,signal;uint32_t guest[8];uint64_t registers[31],pc,sp,address,base;
    unsigned frame_timeout,startup_timeout;
    char root[PATH_MAX],executable[PATH_MAX],log[PATH_MAX];
} Capture;
static Capture *capture;
static NativeGuestCapture guest_capture;
static int notify_fd=-1;
static pid_t helper;
static char alternate_stack[65536];
static uint64_t ticks(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int foreground(Capture *c){
#ifdef TSFP_CRASH_TEST
    return atomic_load(&c->focused);
#else
    /* Query outside the game: a stalled event thread cannot notice focus loss. */
    CFArrayRef list=CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly|kCGWindowListExcludeDesktopElements,kCGNullWindowID);
    if(!list)return 0;int result=0;
    for(CFIndex i=0;i<CFArrayGetCount(list);i++){
        CFDictionaryRef item=CFArrayGetValueAtIndex(list,i);CFNumberRef owner=CFDictionaryGetValue(item,kCGWindowOwnerPID),layer=CFDictionaryGetValue(item,kCGWindowLayer);int pid=0,level=-1;
        if(owner&&layer&&CFNumberGetValue(owner,kCFNumberIntType,&pid)&&CFNumberGetValue(layer,kCFNumberIntType,&level)&&level==0){result=pid==c->pid;break;}
    }CFRelease(list);return result&&atomic_load(&c->focused);
#endif
}
static int safe_directory(const char *path){
    if(!path || path[0]!='/')return -1;int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    char copy[PATH_MAX];if(strlen(path)>=sizeof copy){close(fd);return -1;}strcpy(copy,path);
    char *state=NULL;for(char *part=strtok_r(copy,"/",&state);part;part=strtok_r(NULL,"/",&state)){
        if(!strcmp(part,".")||!strcmp(part,"..")){close(fd);return -1;}
        if(mkdirat(fd,part,0700) && errno!=EEXIST){close(fd);return -1;}
        int next=openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(fd);fd=next;if(fd<0)return -1;
    }return fd;
}
static FILE *new_file(int dir,const char *name){int fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);return fd<0?NULL:fdopen(fd,"w");}
static void health(Capture *c){
    char session[33];if(!pc_capture_session(session))return;
    int dir=safe_directory(c->root);if(dir<0)return;
    struct stat st;
    if(!fstatat(dir,"capture-health.json",&st,AT_SYMLINK_NOFOLLOW) && !S_ISREG(st.st_mode)){close(dir);return;}
    char temporary[80];snprintf(temporary,sizeof temporary,"health-%d-%llu.tmp",getpid(),(unsigned long long)ticks());
    FILE *out=new_file(dir,temporary);
    if(out){pc_capture_health_json(out,session,(unsigned)c->pid,(unsigned)getpid(),atomic_load(&c->armed));
        int ok=!fflush(out)&&!ferror(out)&&!fsync(fileno(out));if(fclose(out))ok=0;
        if(ok)renameat(dir,temporary,dir,"capture-health.json");unlinkat(dir,temporary,0);
    }close(dir);
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static void executable_hash(const char *path,char hash[65]){
    unsigned char digest[CC_SHA256_DIGEST_LENGTH],bytes[65536];CC_SHA256_CTX ctx;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);hash[0]=0;if(fd<0)return;
    CC_SHA256_Init(&ctx);ssize_t n;while((n=read(fd,bytes,sizeof bytes))>0)CC_SHA256_Update(&ctx,bytes,(CC_LONG)n);
    close(fd);if(n<0)return;CC_SHA256_Final(digest,&ctx);for(unsigned i=0;i<sizeof digest;i++)snprintf(hash+i*2,3,"%02x",digest[i]);
}
#pragma clang diagnostic pop
static void tool(char *const argv[],int output){
    pid_t child=fork();if(child==0){struct rlimit limit={262144,262144};setrlimit(RLIMIT_FSIZE,&limit);dup2(output,STDOUT_FILENO);dup2(output,STDERR_FILENO);execv(argv[0],argv);_exit(127);}
    if(child<0)return;uint64_t end=ticks()+5000;int status;
    while(waitpid(child,&status,WNOHANG)==0){if(ticks()>=end){kill(child,SIGKILL);waitpid(child,&status,0);break;}usleep(20000);}
}
static void report(Capture *c,int hang){
    int root=safe_directory(c->root);if(root<0)return;
    struct timeval now;gettimeofday(&now,NULL);struct tm utc;gmtime_r(&now.tv_sec,&utc);
    char date[32],name[96],hash[65];strftime(date,sizeof date,"%Y%m%d-%H%M%S",&utc);
    snprintf(name,sizeof name,"crash-%s%03d-%010d-%06u",date,(int)(now.tv_usec/1000),c->pid,100000+arc4random_uniform(900000));
    if(mkdirat(root,name,0700)){close(root);return;}
    int dir=openat(root,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(root);if(dir<0)return;
    int locked=openat(dir,"capture.lock",O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);if(locked<0){close(dir);return;}flock(locked,LOCK_EX);
    executable_hash(c->executable,hash);
    FILE *out=new_file(dir,"native-stack.txt");
    if(out){
        if(hang){char pid[20];snprintf(pid,sizeof pid,"%d",c->pid);char *args[]={"/usr/bin/sample",pid,"1","1",NULL};fflush(out);tool(args,fileno(out));}
        else {
            fprintf(out,"Native ARM64 register context\nSignal: %d\nFault address: 0x%llx\nPC: 0x%llx SP: 0x%llx Image base: 0x%llx\n",c->signal,(unsigned long long)c->address,(unsigned long long)c->pc,(unsigned long long)c->sp,(unsigned long long)c->base);
            for(int i=0;i<31;i++)fprintf(out,"x%d = 0x%016llx\n",i,(unsigned long long)c->registers[i]);
            fprintf(out,"Xbox eax ecx edx esp ebx esi edi ebp:");for(int i=0;i<8;i++)fprintf(out," %08x",c->guest[i]);fputs("\nPC and LR symbols (matching executable/dSYM required):\n",out);fflush(out);
            char base[24],pc[24],lr[24];snprintf(base,sizeof base,"0x%llx",(unsigned long long)c->base);snprintf(pc,sizeof pc,"0x%llx",(unsigned long long)c->pc);snprintf(lr,sizeof lr,"0x%llx",(unsigned long long)c->registers[30]);
            char *args[]={"/usr/bin/atos","-arch","arm64","-o",c->executable,"-l",base,pc,lr,NULL};tool(args,fileno(out));
            char pid[20];snprintf(pid,sizeof pid,"%d",c->pid);char *sample[]={"/usr/bin/sample",pid,"1","1",NULL};tool(sample,fileno(out));
        }fclose(out);
    }
    int input=open(c->log,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(input>=0){struct stat st;if(!fstat(input,&st)&&S_ISREG(st.st_mode)){lseek(input,st.st_size>65536?st.st_size-65536:0,SEEK_SET);out=new_file(dir,"game-log-tail.txt");if(out){char bytes[65536];ssize_t n=read(input,bytes,sizeof bytes);if(n>0)fwrite(bytes,1,(size_t)n,out);fclose(out);}}close(input);}
    out=new_file(dir,"report.txt");if(out){fprintf(out,"TimeSplitters native Mac %s\n%s\nSignal: %d\nExecutable SHA-256: %s\nNative context: native-stack.txt\nNothing was uploaded. Review the context and log excerpt before sharing.\n",hang?"suspected hang":"fatal signal",hang?"Frame progress stopped; the game was left running.":"The native game received a fatal signal.",hang?0:c->signal,hash);fclose(out);}
    out=new_file(dir,"breadcrumbs.txt");if(out){pc_crumb_report(out,&c->breadcrumbs);fclose(out);}
    /* Metadata written last: readers never see a partially populated report. */
    out=new_file(dir,"report.json");if(out){fprintf(out,"{\"schema\":1,\"kind\":\"%s\",\"platform\":\"macos-arm64\",\"pid\":%d,\"signal\":%d,\"exception_code\":\"0x%08x\",\"dump_saved\":false,\"native_capture_saved\":true,\"executable_sha256\":\"%s\",\"frame_sequence\":%llu}\n",hang?"suspected_hang":"unhandled_exception",c->pid,hang?0:c->signal,hang?0:c->signal,hash,atomic_load(&c->frames));fclose(out);}
    flock(locked,LOCK_UN);close(locked);close(dir);
}
int tsfp_native_crash_helper(int argc,char **argv){
    if(argc!=4 || strcmp(argv[1],"--native-crash-helper"))return -1;
    char *end;long mapfd=strtol(argv[2],&end,10);if(*end||mapfd<3||mapfd>INT_MAX)return 2;
    long readfd=strtol(argv[3],&end,10);if(*end||readfd<3||readfd>INT_MAX)return 2;
    struct stat st;if(fstat((int)mapfd,&st)||st.st_size!=sizeof(Capture))return 2;
    Capture *c=mmap(NULL,sizeof *c,PROT_READ|PROT_WRITE,MAP_SHARED,(int)mapfd,0);close((int)mapfd);if(c==MAP_FAILED)return 2;
    health(c);uint64_t last_health=ticks();
    PcHangPolicy policy={0};struct pollfd fd={(int)readfd,POLLIN,0};
    while(!atomic_load(&c->stopping)){
        int ready=poll(&fd,1,250);
        if(atomic_load(&c->entered)==2){report(c,0);atomic_store(&c->done,1);break;}
        if(ready>0 && fd.revents){char byte;if(read(fd.fd,&byte,1)<=0)break;}
        if(ticks()-last_health>=2000){health(c);last_health=ticks();}
        if(pc_hang_poll(&policy,ticks(),atomic_load(&c->frames),atomic_load(&c->armed),!foreground(c)||atomic_load(&c->inhibited),c->frame_timeout,c->startup_timeout))report(c,1);
    }
    close((int)readfd);munmap(c,sizeof *c);return 0;
}
int tsfp_native_crash_init(NativeGuestCapture guest){
    const char *root=getenv("TSFP_CRASH_DIR");if(capture||!root||!*root)return 0;
    char temporary[]="/tmp/tsfp-crash-XXXXXX";int mapfd=mkstemp(temporary);if(mapfd<0)return 0;unlink(temporary);
    int pipefd[2];if(ftruncate(mapfd,sizeof(Capture))||pipe(pipefd)){close(mapfd);return 0;}
    Capture *c=mmap(NULL,sizeof *c,PROT_READ|PROT_WRITE,MAP_SHARED,mapfd,0);if(c==MAP_FAILED){close(mapfd);close(pipefd[0]);close(pipefd[1]);return 0;}
    memset(c,0,sizeof *c);uint32_t length=sizeof c->executable;char exe[PATH_MAX];
    if(_NSGetExecutablePath(exe,&length)||!realpath(exe,c->executable)||strlen(root)>=sizeof c->root)goto fail;
    strcpy(c->root,root);const char *log=getenv("RECOMP_LOG_FILE");if(log&&strlen(log)<sizeof c->log)strcpy(c->log,log);
    c->started_ms=ticks();pc_crumb_add(&c->breadcrumbs,0,PC_CRUMB_STARTED,0);
    c->pid=getpid();c->frame_timeout=60000;c->startup_timeout=180000;atomic_store(&c->focused,1);
#ifdef TSFP_CRASH_TEST
    c->frame_timeout=500;c->startup_timeout=1500;
#endif
    Dl_info image;if(dladdr((void *)tsfp_native_crash_init,&image))c->base=(uintptr_t)image.dli_fbase;
    char maparg[20],pipearg[20];snprintf(maparg,sizeof maparg,"%d",mapfd);snprintf(pipearg,sizeof pipearg,"%d",pipefd[0]);
    helper=fork();if(helper==0){close(pipefd[1]);char *args[]={c->executable,"--native-crash-helper",maparg,pipearg,NULL};execv(args[0],args);_exit(127);}
    if(helper<0)goto fail;close(mapfd);close(pipefd[0]);fcntl(pipefd[1],F_SETFL,O_NONBLOCK);fcntl(pipefd[1],F_SETFD,FD_CLOEXEC);
    /* A dead helper must never turn notification into a second fatal signal. */
    signal(SIGPIPE,SIG_IGN);capture=c;guest_capture=guest;notify_fd=pipefd[1];
    stack_t stack={.ss_sp=alternate_stack,.ss_size=sizeof alternate_stack};sigaltstack(&stack,NULL);
    atexit(tsfp_native_crash_stop);return 1;
fail:munmap(c,sizeof *c);close(mapfd);close(pipefd[0]);close(pipefd[1]);return 0;
}
void tsfp_native_crash_signal(int sig,siginfo_t *info,void *context){
    Capture *c=capture;int expected=0;if(c && atomic_compare_exchange_strong(&c->entered,&expected,1)){
        ucontext_t *uc=context;c->signal=sig;c->address=(uintptr_t)info->si_addr;c->pc=uc->uc_mcontext->__ss.__pc;c->sp=uc->uc_mcontext->__ss.__sp;
        for(int i=0;i<29;i++)c->registers[i]=uc->uc_mcontext->__ss.__x[i];c->registers[29]=uc->uc_mcontext->__ss.__fp;c->registers[30]=uc->uc_mcontext->__ss.__lr;
        if(guest_capture)guest_capture(c->guest);atomic_store(&c->entered,2);(void)write(notify_fd,"x",1);
    }
    if(c){struct timespec delay={0,10000000};for(int i=0;i<1200 && !atomic_load(&c->done);i++)nanosleep(&delay,NULL);}
    _Exit(128+sig);
}
void tsfp_native_crash_frame(void){if(capture)atomic_fetch_add(&capture->frames,1);}
void tsfp_native_crash_armed(int armed){if(capture){pc_crumb_add(&capture->breadcrumbs,ticks()-capture->started_ms,armed?PC_CRUMB_GUEST_START:PC_CRUMB_GUEST_STOP,0);atomic_store(&capture->armed,armed);}}
void tsfp_native_crash_focus(int focused){if(capture)atomic_store(&capture->focused,focused);}
void tsfp_native_crash_inhibit(int entering){if(capture){if(entering)atomic_fetch_add(&capture->inhibited,1);else if(atomic_load(&capture->inhibited)>0)atomic_fetch_sub(&capture->inhibited,1);pc_crumb_add(&capture->breadcrumbs,ticks()-capture->started_ms,PC_CRUMB_MODAL,atomic_load(&capture->inhibited));}}
void tsfp_native_crash_stop(void){if(!capture)return;atomic_store(&capture->stopping,1);close(notify_fd);notify_fd=-1;
    uint64_t end=ticks()+6000;int status;while(waitpid(helper,&status,WNOHANG)==0){if(ticks()>=end){kill(helper,SIGKILL);waitpid(helper,&status,0);break;}usleep(10000);}
    /* Keep mapping until teardown: other game threads may still read it. */
}

void pc_capture_gameplay(int32_t scene,uint32_t mode,uint32_t menu,int paused,int network,int cutscene){
    if(!capture)return;
    const int32_t values[PC_CRUMB_FIELDS]={scene,(int32_t)mode,(int32_t)menu,!!paused,network,!!cutscene};
    pc_crumb_game(&capture->breadcrumbs,ticks()-capture->started_ms,values);
}
