#ifndef TSFP_PC_NATIVE_WALK_TRACE_H
#define TSFP_PC_NATIVE_WALK_TRACE_H
/* Optional synchronous US walking/sweep pairs. All payloads retain exact
 * guest bits; neither collision decisions nor guest state are changed. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PC_WALK_ROWS 8192u
#define PC_WALK_BYTES (16u*1024u*1024u)
typedef int (*PcWalkRead)(uint32_t,void *,size_t);
typedef struct {
    uint32_t frame,scene,tick,raw,game,caller,arguments[10],context[8];
    uint32_t old_position[3],new_position[3],optional[6],optional_valid;
    uint32_t globals[8];
} PcWalkSnapshot;
typedef struct {
    FILE *file;unsigned checked,rows,bytes,next_id;
    uint32_t scene,first_tick,last_tick;
} PcWalkTrace;
static int pc_walk_span(uint32_t a,size_t n)
{ return a>=0x10000u && a<0x84000000u && n<=0x84000000u-a; }
static int pc_walk_u32(const char *s,uint32_t *out)
{
    if(!s || !*s)return 0;
    uint64_t n=0;
    for(;*s;s++) {
        if(*s<'0' || *s>'9')return 0;
        n=n*10u+(unsigned)(*s-'0');if(n>UINT32_MAX)return 0;
    }
    *out=(uint32_t)n;return 1;
}
static int pc_walk_options(PcWalkTrace *t,const char *scene,const char *ticks)
{
    t->scene=46;t->first_tick=2000;t->last_tick=2300;
    if(scene && !pc_walk_u32(scene,&t->scene))return 0;
    if(ticks) {
        char first[16];const char *comma=strchr(ticks,',');
        if(!comma || comma==ticks || (size_t)(comma-ticks)>=sizeof first)return 0;
        memcpy(first,ticks,(size_t)(comma-ticks));first[comma-ticks]=0;
        if(!pc_walk_u32(first,&t->first_tick) ||
           !pc_walk_u32(comma+1,&t->last_tick))return 0;
    }
    return t->first_tick<=t->last_tick;
}
static int pc_walk_collect(PcWalkRead read,uint32_t stack,unsigned count,
                           const PcWalkSnapshot *entry,PcWalkSnapshot *s)
{
    memset(s,0,sizeof *s);
    if((count!=9 && count!=10) || !pc_walk_span(stack,4u*(count+1u)))return 0;
    /* These original routines reuse argument stack slots as scratch. Retain
     * entry pointer anchors after returning, rather than following clobbered
     * slots. The actual memory they address is collected again. */
    if(entry) {s->caller=entry->caller;memcpy(s->arguments,entry->arguments,sizeof s->arguments);}
    else if(!read(stack,&s->caller,4) || !read(stack+4,s->arguments,count*4u))return 0;
    if(!read(0x7497b8u,&s->frame,4) || !read(0x79094cu,&s->scene,4) ||
       !read(0x7de318u,&s->tick,4) || !read(0x4e7948u,&s->raw,4) ||
       !read(0x4e794cu,&s->game,4))return 0;
    if(!pc_walk_span(s->arguments[0],sizeof s->context) ||
       !pc_walk_span(s->arguments[1],12) || !pc_walk_span(s->arguments[2],12) ||
       !read(s->arguments[0],s->context,sizeof s->context) ||
       !read(s->arguments[1],s->old_position,12) ||
       !read(s->arguments[2],s->new_position,12) ||
       !read(0x732f70u,s->globals,sizeof s->globals))return 0;
    /* Sweep argument7 is an optional result block. Root argument7 is retained
     * as raw data only: the two routines have different signatures. */
    if(count==9 && pc_walk_span(s->arguments[6],sizeof s->optional))
        s->optional_valid=(uint32_t)read(s->arguments[6],s->optional,sizeof s->optional);
    return 1;
}
static int pc_walk_enabled(PcWalkTrace *t)
{
    if(!t->checked) {
        t->checked=1;const char *path=getenv("TSFP_WALK_TRACE");
        if(path && *path) {
            if(!pc_walk_options(t,getenv("TSFP_WALK_TRACE_SCENE"),getenv("TSFP_WALK_TRACE_TICKS")))
                fprintf(stderr,"[WALK-TRACE] Invalid scene/tick selection\n");
            else if(!(t->file=fopen(path,"w")))fprintf(stderr,"[WALK-TRACE] Unable to open requested trace\n");
            else {
                const char *header="id,parent,depth,routine,stack_before,stack_after,eax_after,phase,word,bits\n";
                size_t n=strlen(header);
                if(fwrite(header,1,n,t->file)!=n){fclose(t->file);t->file=NULL;}
                else t->bytes=(unsigned)n;
            }
        }
    }
    if(t->file && t->rows>=PC_WALK_ROWS){fclose(t->file);t->file=NULL;}
    return t->file!=NULL;
}
/* One CSV row per pair; the word payload is separated by semicolons to retain
 * exact bits without locale-dependent floats or huge diagnostic headers. */
static void pc_walk_record(PcWalkTrace *t,unsigned id,unsigned parent,unsigned depth,
                           uint32_t routine,uint32_t stack,uint32_t after_stack,uint32_t eax,
                           const PcWalkSnapshot *before,const PcWalkSnapshot *after)
{
    if(!t->file || before->frame!=after->frame || before->scene!=after->scene ||
       before->tick!=after->tick || before->caller!=after->caller ||
       memcmp(before->arguments,after->arguments,sizeof before->arguments) ||
       after_stack!=stack+4u)return;
    char line[4096];int size=snprintf(line,sizeof line,"%u,%u,%u,%08x,%08x,%08x,%08x,pair,layout1,",
        id,parent,depth,routine,stack,after_stack,eax);
    if(size<=0 || (size_t)size>=sizeof line)return;
    const PcWalkSnapshot *samples[2]={before,after};
    for(unsigned phase=0;phase<2;phase++) {
        const PcWalkSnapshot *s=samples[phase];
        /* Explicit arrays avoid dumping padding or aliasing a struct as words. */
        uint32_t words[46];unsigned at=0;
        words[at++]=s->frame;words[at++]=s->scene;words[at++]=s->tick;
        words[at++]=s->raw;words[at++]=s->game;words[at++]=s->caller;
        for(unsigned i=0;i<10;i++)words[at++]=s->arguments[i];
        for(unsigned i=0;i<8;i++)words[at++]=s->context[i];
        for(unsigned i=0;i<3;i++)words[at++]=s->old_position[i];
        for(unsigned i=0;i<3;i++)words[at++]=s->new_position[i];
        words[at++]=s->optional_valid;
        for(unsigned i=0;i<6;i++)words[at++]=s->optional[i];
        for(unsigned i=0;i<8;i++)words[at++]=s->globals[i];
        for(unsigned i=0;i<at;i++) {
            int n=snprintf(line+size,sizeof line-(size_t)size,"%08x;",words[i]);
            if(n<=0 || (size_t)n>=sizeof line-(size_t)size)return;
            size+=n;
        }
    }
    line[size++]='\n';
    if(t->rows>=PC_WALK_ROWS || (unsigned)size>PC_WALK_BYTES-t->bytes ||
       fwrite(line,1,(size_t)size,t->file)!=(size_t)size) {
        fclose(t->file);t->file=NULL;return;
    }
    t->rows++;t->bytes+=(unsigned)size;
    if((t->rows&63u)==0 && fflush(t->file)!=0){fclose(t->file);t->file=NULL;}
}
#endif
