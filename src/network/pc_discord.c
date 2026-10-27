/* Shared Discord framing, reconnect, activity and configuration policy. */
#include "pc_discord.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef TSFP_DISCORD_INTERVAL
#define TSFP_DISCORD_INTERVAL 15000
#endif
#ifndef TSFP_DISCORD_RETRY
#define TSFP_DISCORD_RETRY 5000
#endif
#define MAX_FRAME 65536u
#ifdef _WIN32
#include "pc_discord_win32.h"
#else
#include "native/discord_posix.h"
#endif

static PcDiscordSnapshot latest={.scene=101};
static PcDiscordConfig config;

static int send_frame(DiscordPipe pipe,uint32_t opcode,const char *data,size_t size)
{
    unsigned char frame[4096];uint32_t length=(uint32_t)size;
    if(size>sizeof frame-8) return 0;
    memcpy(frame,&opcode,4);memcpy(frame+4,&length,4);memcpy(frame+8,data,size);
    return transfer(pipe,frame,(uint32_t)(size+8),1);
}

/* Match only top-level JSON strings; ignore nested user/profile data. */
static int field_is(const char *json,const char *key,const char *value)
{
    int depth=0;
    for(const char *p=json;*p;p++) {
        if(*p=='{' || *p=='[') depth++;
        else if(*p=='}' || *p==']') depth--;
        else if(*p=='"') {
            const char *start=++p;
            while(*p && *p!='"') {if(*p=='\\' && p[1]) p++;p++;}
            if(!*p) return 0;
            if(depth==1 && (size_t)(p-start)==strlen(key) && !memcmp(start,key,strlen(key))) {
                const char *q=p+1;
                while(*q==' ' || *q=='\t' || *q=='\r' || *q=='\n') q++;
                if(*q++!=':') continue;
                while(*q==' ' || *q=='\t' || *q=='\r' || *q=='\n') q++;
                if(*q++=='"' && !strncmp(q,value,strlen(value)) && q[strlen(value)]=='"') return 1;
            }
        }
    }
    return 0;
}

static THREAD_RESULT run(void *unused)
{
    char *incoming=(char *)malloc(MAX_FRAME+9u),payload[2048];
    unsigned nonce=0;int64_t started=(int64_t)time(NULL);
    int reported_ready=0,reported_error=0,reported_activity=0;
    (void)unused;
    if(!incoming) return THREAD_RETURN;
    while(!wait_stop(0)) {
        DiscordPipe pipe=open_pipe();
        if(pipe!=BAD_PIPE) {
            int n=snprintf(payload,sizeof payload,"{\"v\":1,\"client_id\":\"%s\"}",config.application_id);
            int connected=send_frame(pipe,0,payload,(size_t)n),ready=0,have_sent=0;
            size_t used=0;uint64_t opened=ticks(),sent_at=0;
            PcDiscordActivity previous={0};
            while(connected && !wait_stop(100)) {
                uint32_t available=0;
                if(!pipe_available(pipe,&available)) break;
                if(available) {
                    uint32_t take=available;
                    if(take>MAX_FRAME+8u-used) take=(uint32_t)(MAX_FRAME+8u-used);
                    if(!take || !transfer(pipe,incoming+used,take,0)) break;
                    used+=take;
                    incoming[used]=0;
                    while(used>=8) {
                        uint32_t opcode,length;size_t frame;
                        memcpy(&opcode,incoming,4);memcpy(&length,incoming+4,4);
                        if(length>MAX_FRAME) {connected=0;break;}
                        frame=(size_t)length+8;
                        if(used<frame) break;
                        /* Preserve the next frame's first byte while parsing. */
                        char saved=incoming[frame];incoming[frame]=0;
                        if(opcode==2) connected=0;
                        else if(opcode==3) {
                            uint32_t pong=4;
                            memcpy(incoming,&pong,4);
                            connected=transfer(pipe,incoming,(uint32_t)frame,1);
                        } else if(opcode==1) {
                            if(field_is(incoming+8,"evt","ERROR")) {
                                if(!reported_error) fprintf(stderr,"[PC-DISCORD] Discord rejected presence; check the Application ID.\n");
                                reported_error=1;
                                connected=0;
                            } else if(field_is(incoming+8,"cmd","DISPATCH") && field_is(incoming+8,"evt","READY")) {
                                ready=1;
                                if(!reported_ready) fprintf(stderr,"[PC-DISCORD] Connected to the Discord desktop client.\n");
                                reported_ready=1;
                            } else if(field_is(incoming+8,"cmd","SET_ACTIVITY") && !reported_activity) {
                                fprintf(stderr,"[PC-DISCORD] Discord accepted Rich Presence.\n");reported_activity=1;
                            }
                        } else if(opcode!=4) connected=0;
                        incoming[frame]=saved;
                        used-=frame;memmove(incoming,incoming+frame,used);
                        if(!connected) break;
                    }
                }
                if(!ready && ticks()-opened>10000) break;
                if(connected && ready && (!have_sent || ticks()-sent_at>=TSFP_DISCORD_INTERVAL)) {
                    PcDiscordSnapshot sample;PcDiscordActivity activity;
                    sample_lock();sample=latest;sample_unlock();
                    pc_discord_activity(&sample,&activity);
                    if(!have_sent || strcmp(previous.details,activity.details) || strcmp(previous.state,activity.state)) {
                        size_t bytes=pc_discord_payload(payload,sizeof payload,&activity,process_id(),started,++nonce);
                        connected=bytes && send_frame(pipe,1,payload,bytes);
                        if(connected) {
                            previous=activity;sent_at=ticks();have_sent=1;
                            fprintf(stderr,"[PC-DISCORD] Activity: %s / %s\n",activity.details,activity.state);
                        }
                    }
                }
            }
            /* Pipe disconnect also clears presence. Attempt an explicit clear
             * on normal shutdown without allowing a slow peer to hold exit. */
            if(ready && wait_stop(0)) {
                size_t bytes=pc_discord_payload(payload,sizeof payload,NULL,process_id(),started,++nonce);
                if(bytes) send_frame(pipe,1,payload,bytes);
            }
            pipe_close(pipe);
        }
        if(wait_stop(TSFP_DISCORD_RETRY)) break;
    }
    free(incoming);return THREAD_RETURN;
}

void pc_discord_start(void)
{
    char path[32768],text[256];const char *p;FILE *file;size_t bytes;
    if(worker) return;
    p=getenv("TSFP_DISCORD_CONFIG");
    if(p && *p) snprintf(path,sizeof path,"%s",p);
    else {
        char *a,*b,*slash;
        p=getenv("RECOMP_INPUT_CONFIG");snprintf(path,sizeof path,"%s",p&&*p?p:"pc-controls.json");
        a=strrchr(path,'/');b=strrchr(path,'\\');slash=a&&b?(a>b?a:b):a?a:b;
        if(slash) snprintf(slash+1,sizeof path-(size_t)(slash+1-path),"pc-discord.ini");
        else snprintf(path,sizeof path,"pc-discord.ini");
    }
    file=fopen(path,"rb");if(!file) return;
    bytes=fread(text,1,sizeof text-1,file);text[bytes]=0;
    int valid=!ferror(file) && feof(file) && !memchr(text,0,bytes);
    fclose(file);
    if(!valid || !pc_discord_parse_config(text,&config)) {
        fprintf(stderr,"[PC-DISCORD] Invalid settings; presence disabled.\n");return;
    }
    if(!config.enabled) return;
    if(!start_worker(run)) return;
    atexit(pc_discord_stop);
    fprintf(stderr,"[PC-DISCORD] Optional Rich Presence enabled.\n");
}

void pc_discord_submit(const PcDiscordSnapshot *sample)
{
    if(submit_lock()) {
        latest=*sample;submit_unlock();
    }
}

void pc_discord_stop(void)
{
    stop_worker();
}
