/* Bounded binary stdin/stdout protocol. No source paths or file commands exist.
   This executable is NOT an OS sandbox; launcher activation stays unavailable
   until the game supervisor and all platform confinement adapters are verified. */
#include "mod_script.h"
#include "mod_watchdog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#define INPUT_MAX (TSFP_SCRIPT_SOURCE_MAX+8u)
#define OUTPUT_MAX (32u+TSFP_SCRIPT_COMMAND_MAX*(20u+TSFP_SCRIPT_TEXT_MAX))
static unsigned char input[INPUT_MAX],output[OUTPUT_MAX];
typedef struct reader { const unsigned char *data;size_t size,at;int valid; } reader;
static uint32_t read32(reader *r) {
    uint32_t n;if(r->size-r->at<4){r->valid=0;return 0;}
    n=(uint32_t)r->data[r->at]|(uint32_t)r->data[r->at+1]<<8|(uint32_t)r->data[r->at+2]<<16|(uint32_t)r->data[r->at+3]<<24;r->at+=4;return n;
}
static uint64_t read64(reader *r){uint64_t low=read32(r),high=read32(r);return low|(high<<32);}
static float read_float(reader *r){uint32_t bits=read32(r);float value;memcpy(&value,&bits,4);return value;}
static int32_t read_signed(reader *r){uint32_t bits=read32(r);int32_t value;memcpy(&value,&bits,4);return value;}
static void write32(unsigned char *data,uint32_t value){unsigned i;for(i=0;i<4;i++)data[i]=(unsigned char)(value>>(8*i));}
static size_t put32(size_t at,uint32_t value){write32(output+at,value);return at+4;}
static int reply(uint32_t status,uint64_t sequence,tsfp_script_vm *vm,const tsfp_script_result *result,const char *error) {
    size_t at=0,length;uint32_t i,count=status?0:result->count;
    at=put32(at,1);at=put32(at,status);at=put32(at,(uint32_t)sequence);at=put32(at,(uint32_t)(sequence>>32));
    at=put32(at,(uint32_t)tsfp_script_memory(vm));at=put32(at,count);
    for(i=0;i<count;i++) {
        const tsfp_script_command *c=&result->commands[i];at=put32(at,c->kind);at=put32(at,c->player);at=put32(at,(uint32_t)c->value);at=put32(at,c->text_size);
        memcpy(output+at,c->text,c->text_size);at+=c->text_size;
    }
    length=error?strlen(error):0;if(length>255)length=255;
    at=put32(at,(uint32_t)length);if(length){memcpy(output+at,error,length);at+=length;}
    {unsigned char header[4];write32(header,(uint32_t)at);
     if(fwrite(header,1,4,stdout)!=4 || fwrite(output,1,at,stdout)!=at || fflush(stdout))return 0;}
    return 1;
}
static int decode_event(reader *r,tsfp_script_event *event) {
    uint32_t i;memset(event,0,sizeof(*event));
    event->sequence=read64(r);event->kind=read32(r);event->tick=read64(r);event->tick_rate=read32(r);
    event->actor=read32(r);event->target=read32(r);event->player_count=read32(r);
    if(event->player_count>TSFP_SCRIPT_PLAYER_MAX)return 0;
    for(i=0;i<event->player_count;i++) {
        tsfp_script_player *p=&event->players[i];p->id=read32(r);p->team=read32(r);p->score=read_signed(r);p->health=read_signed(r);
        p->x=read_float(r);p->y=read_float(r);p->z=read_float(r);
    }
    return r->valid && r->at==r->size;
}
int main(int argc,char **argv) {
    tsfp_script_vm *vm=NULL;tsfp_script_result result;int exit_code=0;(void)argv;
    if(argc!=1)return 2;
#ifdef _WIN32
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)return 2;
#endif
    if(!tsfp_mod_watchdog_start())return 2;
    for(;;) {
        unsigned char header[4];size_t got=fread(header,1,4,stdin);uint32_t size,type;reader r;uint64_t sequence=0;int success=0;
        if(!got)break;
        if(got!=4){exit_code=2;break;}
        r.data=header;r.size=4;r.at=0;r.valid=1;size=read32(&r);
        if(size<4 || size>INPUT_MAX){reply(2,0,vm,NULL,"invalid worker frame length");exit_code=2;break;}
        if(fread(input,1,size,stdin)!=size){exit_code=2;break;}
        r.data=input;r.size=size;r.at=0;r.valid=1;type=read32(&r);result.count=0;
        tsfp_mod_watchdog_arm();
        if(type==1 && !vm && r.size-r.at>4) {
            uint32_t seed=read32(&r);vm=tsfp_script_create(seed);
            success=vm && tsfp_script_load(vm,(const char *)r.data+r.at,r.size-r.at);
        }else if(type==2 && vm) {
            tsfp_script_event event;
            if(decode_event(&r,&event)){sequence=event.sequence;success=tsfp_script_dispatch(vm,&event,&result);}
        }else if(type==3 && r.at==r.size && vm) {
            tsfp_script_destroy(vm);vm=NULL;tsfp_mod_watchdog_disarm();return 0;
        }
        tsfp_mod_watchdog_disarm();
        if(!success) {
            const char *error=tsfp_script_error(vm);if(!error[0])error="invalid worker command or event";
            reply(1,sequence,vm,NULL,error);exit_code=2;break;
        }
        if(!reply(0,sequence,vm,&result,NULL)){exit_code=2;break;}
    }
    tsfp_mod_watchdog_arm();tsfp_script_destroy(vm);tsfp_mod_watchdog_disarm();return exit_code;
}
