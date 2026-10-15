#include "config_files.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int write_temp(const char *path,const char *text,size_t size,mode_t mode) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,mode);
    if(fd<0)return 0;
    size_t at=0;int ok=1;
    while(at<size){ssize_t n=write(fd,text+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=0;break;}at+=n;}
    if(fsync(fd))ok=0;if(close(fd))ok=0;if(!ok)unlink(path);return ok;
}
int tsfp_config_write(const char *path,const char *text,int backup) {
    struct stat st;int exists=lstat(path,&st)==0;
    if((exists&&(!S_ISREG(st.st_mode)||!(st.st_mode&0222)))||(!exists&&backup))return 0;
    char tmp[4096],bak[4096],bak_tmp[4096];
    if(snprintf(tmp,sizeof tmp,"%s.%ld.tmp",path,(long)getpid())>=(int)sizeof tmp ||
       snprintf(bak,sizeof bak,"%s.bak",path)>=(int)sizeof bak ||
       snprintf(bak_tmp,sizeof bak_tmp,"%s.%ld.tmp",bak,(long)getpid())>=(int)sizeof bak_tmp)return 0;
    if(!write_temp(tmp,text,strlen(text),exists?(st.st_mode&0777):0600))return 0;
    int ok=1;
    if(backup){
        int fd=open(path,O_RDONLY|O_NOFOLLOW);char *data=NULL;
        if(fd<0||st.st_size<0||st.st_size>1024*1024)ok=0;
        if(ok){data=malloc((size_t)st.st_size+1);if(!data)ok=0;}
        size_t at=0;
        while(ok&&at<(size_t)st.st_size){ssize_t n=read(fd,data+at,(size_t)st.st_size-at);if(n<0&&errno==EINTR)continue;if(n<=0){ok=0;break;}at+=n;}
        if(fd>=0)close(fd);
        if(ok)ok=write_temp(bak_tmp,data,at,st.st_mode&0777);
        free(data);
        if(ok&&rename(bak_tmp,bak)){unlink(bak_tmp);ok=0;}
    }
    if(ok)ok=rename(tmp,path)==0;
    if(!ok)unlink(tmp);return ok;
}
