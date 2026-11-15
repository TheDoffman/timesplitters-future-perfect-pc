/* Trusted scanner lifetime guardian. Reads no mod bytes or scanner replies.
   The parent owns the write end of argv[1]; the scanner inherits only stdio.
   Retain waitpid ownership until termination so signalling cannot hit a reused
   PID, even when the scanner changes session/process group or cancels alarms. */
#define _DARWIN_C_SOURCE
#include <spawn.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

extern char **environ;
static int finish(pid_t child,int terminate) {
    int status=0;
    if(terminate && kill(child,SIGKILL)<0 && errno!=ESRCH)return 125;
    while(waitpid(child,&status,0)<0)if(errno!=EINTR)return 125;
    return WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
}
static double now(void) {
    struct timespec stamp;
    if(clock_gettime(CLOCK_MONOTONIC,&stamp))return -1;
    return (double)stamp.tv_sec+(double)stamp.tv_nsec/1000000000.0;
}
int main(int argc,char **argv) {
    if(argc<3 || argv[2][0]!='/')return 125;
    char *end=NULL;errno=0;long value=strtol(argv[1],&end,10);
    if(errno || !end || *end || value<3 || value>4095)return 125;
    int parent=(int)value;
    double started=now();if(started<0)return 125;
    struct rlimit core={0,0};if(setrlimit(RLIMIT_CORE,&core))return 125;
    if(signal(SIGPIPE,SIG_IGN)==SIG_ERR)return 125;
    if(signal(SIGCHLD,SIG_DFL)==SIG_ERR)return 125;
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    if(posix_spawn_file_actions_init(&actions))return 125;
    if(posix_spawnattr_init(&attributes)){posix_spawn_file_actions_destroy(&actions);return 125;}
    int error=0;
    for(int fd=0;fd<3 && !error;++fd)error=posix_spawn_file_actions_adddup2(&actions,fd,fd);
    if(!error)error=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT|POSIX_SPAWN_SETPGROUP);
    if(!error)error=posix_spawnattr_setpgroup(&attributes,0);
    pid_t child=-1;
    if(!error)error=posix_spawn(&child,argv[2],&actions,&attributes,argv+2,environ);
    posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);
    if(error)return 125;
    for(;;) {
        int status=0;pid_t exited=waitpid(child,&status,WNOHANG);
        if(exited==child)return WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
        if(exited<0 && errno!=EINTR)return 125;
        double current=now();
        if(current<0 || current-started>=210)return finish(child,1);
        struct pollfd watch={parent,POLLIN,0};int ready=poll(&watch,1,5);
        if((ready>0 && watch.revents) || (ready<0 && errno!=EINTR))return finish(child,1);
    }
}
