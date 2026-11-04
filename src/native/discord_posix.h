/* Thin local IPC adapter. No network requests, SDK, credentials or game RAM. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
typedef int DiscordPipe;
#define BAD_PIPE (-1)
#define THREAD_RESULT void *
#define THREAD_RETURN NULL
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t thread;
static int worker;
static int wakeup[2]={-1,-1};
static atomic_int active;
static uint64_t ticks(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static unsigned process_id(void){return (unsigned)getpid();}
static int wait_stop(unsigned ms){struct pollfd p={wakeup[0],POLLIN,0};return poll(&p,1,(int)ms)>0;}
static void sample_lock(void){pthread_mutex_lock(&lock);}
static void sample_unlock(void){pthread_mutex_unlock(&lock);}
static int submit_lock(void){return atomic_load(&active) && !pthread_mutex_trylock(&lock);}
static void submit_unlock(void){pthread_mutex_unlock(&lock);}
static void pipe_close(DiscordPipe p){close(p);}
static int pipe_available(DiscordPipe p,uint32_t *n){
    struct pollfd fd={p,POLLIN,0};int count=0;
    if(poll(&fd,1,0)<0 || (fd.revents&(POLLHUP|POLLERR|POLLNVAL)))return 0;
    if(ioctl(p,FIONREAD,&count)<0 || (fd.revents&POLLIN && !count))return 0;
    *n=(uint32_t)count;return 1;
}
static int transfer(DiscordPipe p,void *buffer,uint32_t bytes,int writing){
    size_t done=0;uint64_t deadline=ticks()+250;
    while(done<bytes){
        int left=(int)(deadline-ticks());if(left<=0)return 0;
        struct pollfd fds[2]={{p,writing?POLLOUT:POLLIN,0},{wakeup[0],POLLIN,0}};
        int result=poll(fds,2,left);if(result<0 && errno==EINTR)continue;
        if(result<=0 || fds[1].revents || (fds[0].revents&(POLLHUP|POLLERR|POLLNVAL)))return 0;
        ssize_t n=writing?send(p,(char *)buffer+done,bytes-done,
#ifdef MSG_NOSIGNAL
            MSG_NOSIGNAL
#else
            0
#endif
        ):recv(p,(char *)buffer+done,bytes-done,0);
        if(n<0 && (errno==EAGAIN || errno==EINTR))continue;
        if(n<=0)return 0;done+=(size_t)n;
    }return 1;
}
static DiscordPipe open_pipe(void){
#ifdef TSFP_DISCORD_TEST_SOCKET
    const char *dirs[]={getenv("TSFP_DISCORD_TEST_SOCKET")};
#else
    const char *dirs[]={getenv("XDG_RUNTIME_DIR"),getenv("TMPDIR"),getenv("TMP"),getenv("TEMP"),"/tmp"};
#endif
    for(size_t d=0;d<sizeof dirs/sizeof dirs[0];d++)for(unsigned i=0;i<10;i++){
        struct sockaddr_un address={0};struct stat st;
        if(!dirs[d] || !*dirs[d])continue;
        address.sun_family=AF_UNIX;
#ifdef TSFP_DISCORD_TEST_SOCKET
        int n=snprintf(address.sun_path,sizeof address.sun_path,"%s",dirs[d]);
#else
        int n=snprintf(address.sun_path,sizeof address.sun_path,"%s/discord-ipc-%u",dirs[d],i);
#endif
        if(n<0 || (size_t)n>=sizeof address.sun_path || lstat(address.sun_path,&st) || !S_ISSOCK(st.st_mode) || st.st_uid!=geteuid())continue;
        int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)continue;
        #ifdef SO_NOSIGPIPE
        int yes=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof yes);
#endif
        fcntl(fd,F_SETFD,FD_CLOEXEC);fcntl(fd,F_SETFL,O_NONBLOCK);
        if(!connect(fd,(struct sockaddr *)&address,sizeof address))return fd;
        if(errno==EINPROGRESS){struct pollfd p={fd,POLLOUT,0};int error=0;socklen_t size=sizeof error;
            if(poll(&p,1,100)>0 && !getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&size) && !error)return fd;}
        close(fd);
    }return BAD_PIPE;
}
static int start_worker(void *(*run)(void *)){
    if(worker || pipe(wakeup))return 0;
    for(int i=0;i<2;i++){fcntl(wakeup[i],F_SETFD,FD_CLOEXEC);fcntl(wakeup[i],F_SETFL,O_NONBLOCK);}
    if(pthread_create(&thread,NULL,run,NULL)){close(wakeup[0]);close(wakeup[1]);return 0;}
    worker=1;atomic_store(&active,1);return 1;
}
static void stop_worker(void){if(!worker)return;atomic_store(&active,0);(void)write(wakeup[1],"x",1);
    pthread_join(thread,NULL);close(wakeup[0]);close(wakeup[1]);worker=0;}
