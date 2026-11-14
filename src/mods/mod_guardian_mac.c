/* Trusted Darwin lifetime guardian. Never reads mod bytes or worker replies.
   FD3 is a private parent-liveness pipe; FD4 returns the owned worker PID.
   Only stdio is inherited by the sealed worker. Keeping waitpid ownership until
   termination prevents accidentally signalling a reused PID. */
#define _DARWIN_C_SOURCE
#include <spawn.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <poll.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

static int finish(pid_t worker,int terminate) {
    int status=0;
    if(terminate)kill(worker,SIGKILL);
    while(waitpid(worker,&status,0)<0){if(errno!=EINTR)return 125;}
    return WIFEXITED(status)?WEXITSTATUS(status):128+WTERMSIG(status);
}
int main(int argc,char **argv) {
    if(argc!=2 || argv[1][0]!='/')return 125;
    struct rlimit core={0,0};if(setrlimit(RLIMIT_CORE,&core))return 125;
    if(signal(SIGPIPE,SIG_IGN)==SIG_ERR)return 125;
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    if(posix_spawn_file_actions_init(&actions))return 125;
    if(posix_spawnattr_init(&attributes)){posix_spawn_file_actions_destroy(&actions);return 125;}
    int status=0;
    for(int fd=0;fd<3 && !status;++fd)status=posix_spawn_file_actions_adddup2(&actions,fd,fd);
    if(!status)status=posix_spawnattr_setflags(&attributes,POSIX_SPAWN_CLOEXEC_DEFAULT|POSIX_SPAWN_SETPGROUP);
    if(!status)status=posix_spawnattr_setpgroup(&attributes,0);
    char *environment[]={"LANG=C","LC_ALL=C","TZ=UTC",NULL};
    char *arguments[]={argv[1],NULL};pid_t worker=-1;
    if(!status)status=posix_spawn(&worker,argv[1],&actions,&attributes,arguments,environment);
    posix_spawn_file_actions_destroy(&actions);posix_spawnattr_destroy(&attributes);
    if(status)return 125;
    int64_t identity=worker;
    if(write(4,&identity,sizeof(identity))!=sizeof(identity))return finish(worker,1);
    close(4);
    for(;;) {
        int result=0;pid_t exited=waitpid(worker,&result,WNOHANG);
        if(exited==worker)return WIFEXITED(result)?WEXITSTATUS(result):128+WTERMSIG(result);
        if(exited<0 && errno!=EINTR)return 125;
        struct pollfd parent={3,POLLIN,0};int ready=poll(&parent,1,5);
        // Any input, EOF, invalid FD or failed watch ends this owned session.
        if((ready>0 && parent.revents) || (ready<0 && errno!=EINTR))return finish(worker,1);
    }
}
