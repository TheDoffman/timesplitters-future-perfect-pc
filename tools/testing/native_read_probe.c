/* Read-only, opt-in observer for the native crossplay acceptance runner.
 * Loaded into that runner's child only; never linked into a game release.
 * A private Unix socket accepts little-endian guest address/size pairs.
 */
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

static intptr_t *offset;
static int listener=-1;
static int transfer(int fd,void *data,size_t size,int writing) {
    char *p=data;
    while(size) {
        ssize_t n=writing?write(fd,p,size):read(fd,p,size);
        if(n<=0)return 0;
        p+=n;size-=(size_t)n;
    }
    return 1;
}
static void *serve(void *unused) {
    (void)unused;
    for(;;) {
        int fd=accept(listener,NULL,NULL);
        if(fd<0)return NULL;
        int no_sigpipe=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&no_sigpipe,sizeof no_sigpipe);
        uint32_t request[2];
        while(transfer(fd,request,sizeof request,0)) {
            char bytes[65536];mach_vm_size_t copied=0;
            uint32_t status=1;
            if(*offset && request[1]>0 && request[1]<=sizeof bytes &&
               (uint64_t)request[0]+request[1]<=UINT64_C(0x100000000)) {
                status=mach_vm_read_overwrite(mach_task_self(),
                    (mach_vm_address_t)*offset+request[0],request[1],
                    (mach_vm_address_t)bytes,&copied);
                if(copied!=request[1])status=1;
            }
            if(!transfer(fd,&status,sizeof status,1) ||
               (!status && !transfer(fd,bytes,request[1],1)))break;
        }
        close(fd);
    }
}
__attribute__((constructor)) static void start(void) {
    const char *path=getenv("TSFP_CROSSPLAY_READ_SOCKET");
    offset=dlsym(RTLD_DEFAULT,"g_xbox_mem_offset");
    if(!path || !offset)return;
    struct sockaddr_un address={0};address.sun_family=AF_UNIX;
    if(strlen(path)>=sizeof address.sun_path)return;
    strcpy(address.sun_path,path);
    listener=socket(AF_UNIX,SOCK_STREAM,0);
    if(listener<0)return;
    /* Never remove an existing path or listen outside the caller-owned folder. */
    mode_t previous=umask(077);
    int result=bind(listener,(struct sockaddr *)&address,sizeof address);
    umask(previous);
    if(result || listen(listener,1)){close(listener);listener=-1;return;}
    pthread_t thread;
    if(!pthread_create(&thread,NULL,serve,NULL))pthread_detach(thread);
}
