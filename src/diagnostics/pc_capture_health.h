#ifndef TSFP_PC_CAPTURE_HEALTH_H
#define TSFP_PC_CAPTURE_HEALTH_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static inline int pc_capture_session(char session[33]){
    const char *value=getenv("TSFP_CAPTURE_SESSION");
    if(!value || strlen(value)!=32)return 0;
    for(int i=0;i<32;i++)if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return 0;
    memcpy(session,value,33);return 1;
}
static inline void pc_capture_health_json(FILE *out,const char *session,unsigned pid,unsigned helper,int armed){
    fprintf(out,"{\"schema\":1,\"session_id\":\"%s\",\"pid\":%u,\"helper_pid\":%u,\"armed\":%s}\n",session,pid,helper,armed?"true":"false");
}
#endif
