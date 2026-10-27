#ifndef TSFP_PC_NATIVE_CHARACTER_TRACE_H
#define TSFP_PC_NATIVE_CHARACTER_TRACE_H
/* Opt-in US status-3 update pairs. Synchronous CPU state, not GPU/scanout. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define PC_CHARACTER_ROWS 8192u
#define PC_CHARACTER_BYTES (8u*1024u*1024u)
typedef int (*PcCharacterRead)(uint32_t,void *,size_t);
typedef struct {
    uint32_t state,flags,model,matrix,frame,scene;
    float raw,game,age,coefficient[2],values[16];
} PcCharacterSnapshot;
typedef struct { FILE *file;unsigned checked,rows,bytes; } PcCharacterTrace;
static int pc_character_span(uint32_t address,size_t bytes)
{ return address>=0x10000u && address<0x84000000u && bytes<=0x84000000u-address; }
static int pc_character_finite(const float *v,unsigned count)
{ for(unsigned i=0;i<count;i++)if(!isfinite(v[i]))return 0;return 1; }
/* 0: invalid/unrelated; 1: active model; 2: inactive/removed. */
static int pc_character_collect(PcCharacterRead read,uint32_t state,PcCharacterSnapshot *out)
{
    uint32_t kind;
    memset(out,0,sizeof *out);out->state=state;
    if(!pc_character_span(state,0x64) || !read(state+4,&out->flags,4) ||
       !read(state+0x54,&kind,4) || kind!=3 || !read(state+0x60,&out->model,4))return 0;
    if(!(out->flags&1) || !out->model)return 2;
    if(!pc_character_span(out->model,0x1c) || !read(out->model+0x18,&out->matrix,4) ||
       !pc_character_span(out->matrix,64) || !read(out->matrix,out->values,64) ||
       !pc_character_finite(out->values,16) || !read(state+0x5c,&out->age,4) ||
       !read(0x4e7948u,&out->raw,4) || !read(0x4e794cu,&out->game,4) ||
       !read(0x478860u,&out->coefficient[0],4) || !read(0x47d6d4u,&out->coefficient[1],4) ||
       !read(0x7497b8u,&out->frame,4) || !read(0x79094cu,&out->scene,4) ||
       !isfinite(out->age) || !isfinite(out->raw) || !isfinite(out->game) ||
       !pc_character_finite(out->coefficient,2))return 0;
    return 1;
}
static int pc_character_enabled(PcCharacterTrace *trace)
{
    if(!trace->checked) {
        trace->checked=1;const char *path=getenv("TSFP_CHARACTER_TRACE");
        if(path && *path) {
            trace->file=fopen(path,"w");
            if(!trace->file)fprintf(stderr,"[CHARACTER-TRACE] Unable to open requested trace\n");
            else {
                char header[1024]="sequence,frame,scene,native,state,model,matrix,raw_delta,game_delta,age,coefficient_xz,coefficient_y,alive_after";
                size_t length=strlen(header);
                for(unsigned phase=0;phase<2;phase++)for(unsigned i=0;i<16;i++)
                    length+=(size_t)snprintf(header+length,sizeof header-length,",%s%u",phase?"after":"before",i);
                header[length++]='\n';
                if(fwrite(header,1,length,trace->file)!=length){fclose(trace->file);trace->file=NULL;}
                else trace->bytes=(unsigned)length;
            }
        }
    }
    return trace->file!=NULL;
}
static void pc_character_record(PcCharacterTrace *trace,const PcCharacterSnapshot *before,
                                const PcCharacterSnapshot *after,int status,int native)
{
    if(!trace->file || (status!=1 && status!=2) || before->state!=after->state ||
       (status==1 && (before->model!=after->model || before->matrix!=after->matrix ||
                      before->frame!=after->frame || before->scene!=after->scene)))return;
    char line[2048];int size=snprintf(line,sizeof line,"%u,%u,%u,%d,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%d",
        trace->rows,before->frame,before->scene,native,before->state,before->model,before->matrix,
        before->raw,before->game,before->age,before->coefficient[0],before->coefficient[1],status==1);
    if(size<=0 || (size_t)size>=sizeof line)return;
    for(unsigned phase=0;phase<2;phase++)for(unsigned i=0;i<16;i++) {
        int n=snprintf(line+size,sizeof line-(size_t)size,",%.9g",phase?after->values[i]:before->values[i]);
        if(n<=0 || (size_t)n>=sizeof line-(size_t)size)return;
        size+=n;
    }
    line[size++]='\n';
    if(trace->rows>=PC_CHARACTER_ROWS || (unsigned)size>PC_CHARACTER_BYTES-trace->bytes ||
       fwrite(line,1,(size_t)size,trace->file)!=(size_t)size) {
        fclose(trace->file);trace->file=NULL;return;
    }
    trace->rows++;trace->bytes+=(unsigned)size;
    if(fflush(trace->file)!=0){fclose(trace->file);trace->file=NULL;}
}
#endif
