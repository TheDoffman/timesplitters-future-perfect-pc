#ifndef TSFP_PC_NATIVE_CONTACT_TRACE_H
#define TSFP_PC_NATIVE_CONTACT_TRACE_H
/* Optional US 172114 -> 171560 solver-entry evidence. No guest writes.
 * Generated contacts are transient: 171FB0 resets their arena after solving.
 * These records describe CPU physics, never presentation or frame pacing. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#define PC_CONTACT_MAX 500u
#define PC_CONTACT_ROWS 16000u
#define PC_CONTACT_BYTES (8u*1024u*1024u)
typedef int (*PcContactRead)(uint32_t,void *,size_t);
typedef struct {
    float point[3],normal[3],depth,coefficient[2];
    uint32_t owner,other;
    uint16_t material,kind;
    uint32_t flags,next;
} PcContactRecord;
typedef struct {
    uint32_t address,index,selected;
    PcContactRecord record;
} PcContactEntry;
typedef struct {
    uint32_t frame,body,prop,raw_count,selected_count,count;
    float delta,primary,secondary,position[3],velocity[3],orientation[4];
    PcContactEntry contacts[2*PC_CONTACT_MAX];
} PcContactSnapshot;
typedef struct {
    FILE *file;
    unsigned checked,rows,bytes,sequence,slot;
    float min_z,max_z;
} PcContactTrace;

static int pc_contact_options(PcContactTrace *trace,const char *slot,const char *window)
{
    char *end;
    if(!slot || !*slot || !window || !*window || *slot<'0' || *slot>'9')return 0;
    errno=0;unsigned long value=strtoul(slot,&end,10);
    if(errno || *end || value>=1024u)return 0;
    trace->slot=(unsigned)value;
    errno=0;trace->min_z=strtof(window,&end);
    if(errno || end==window || *end!=',' || !isfinite(trace->min_z))return 0;
    window=end+1;errno=0;trace->max_z=strtof(window,&end);
    return !errno && end!=window && !*end && isfinite(trace->max_z) && trace->min_z<=trace->max_z;
}
static int pc_contact_enabled(PcContactTrace *trace)
{
    if(!trace->checked) {
        trace->checked=1;
        const char *path=getenv("TSFP_CONTACT_TRACE");
        if(path && *path) {
            if(pc_contact_options(trace,getenv("TSFP_CONTACT_TRACE_SLOT"),getenv("TSFP_CONTACT_TRACE_Z")))
                trace->file=fopen(path,"w");
            if(!trace->file)fprintf(stderr,"[CONTACT-TRACE] Invalid filter or unable to open requested trace\n");
            else {
                const char *header="sequence,frame,native,slot,body,prop,game_delta,primary_seconds,secondary_seconds,x,y,z,vx,vy,vz,qx,qy,qz,qw,raw_count,selected_count,source,index,address,owner,other,kind,flags,px,py,pz,nx,ny,nz,depth,coefficient0,coefficient1\n";
                size_t size=strlen(header);
                if(fwrite(header,1,size,trace->file)!=size){fclose(trace->file);trace->file=NULL;}
                else trace->bytes=(unsigned)size;
            }
        }
    }
    return trace->file!=NULL;
}
static int pc_contact_finite(const float *values,unsigned count)
{ for(unsigned i=0;i<count;i++)if(!isfinite(values[i]))return 0;return 1; }
static int pc_contact_heap(uint32_t address,size_t size)
{ return address>=0x80000000u && address<0x84000000u && size<=0x84000000u-address; }
static int pc_contact_collect(PcContactRead read,uint32_t stack,const PcContactTrace *trace,PcContactSnapshot *out)
{
    uint32_t pool,table,handle,prop,other_handle,kind,arena,remaining,args[5];
    out->count=0;
    if(trace->slot>=1024u || !read(0x74c458u,&pool,4) ||
       !pc_contact_heap(pool,trace->slot*0x200u+0x200u) ||
       !read(0x74c46cu,&table,4) || !pc_contact_heap(table,trace->slot*4u+4u))return 0;
    out->body=pool+trace->slot*0x200u;
    if(!read(out->body+0x1e4u,&handle,4) || (handle&0xffffu)!=trace->slot ||
       !read(table+trace->slot*4u,&prop,4) || !pc_contact_heap(prop,0x84u) ||
       !read(prop+0x80u,&other_handle,4) || other_handle!=handle ||
       !read(prop+0x20u,&kind,4) || kind!=0x400000u)return 0;
    out->prop=prop;
    if(!read(out->body+0x140u,out->position,sizeof out->position) ||
       !pc_contact_finite(out->position,3) || out->position[2]<trace->min_z || out->position[2]>trace->max_z)return 0;
    if(!read(out->body+0x14cu,out->velocity,sizeof out->velocity) ||
       !read(out->body+0x100u,out->orientation,sizeof out->orientation) ||
       !pc_contact_finite(out->velocity,3) || !pc_contact_finite(out->orientation,4) ||
       !read(0x7497b8u,&out->frame,4) || !read(0x4e794cu,&out->delta,4) || !isfinite(out->delta) ||
       !read(stack,args,sizeof args) || args[0]!=0x172119u || args[2]>PC_CONTACT_MAX ||
       !read(0x74c468u,&arena,4) || !read(0x4e826cu,&remaining,4) || remaining>PC_CONTACT_MAX ||
       !pc_contact_heap(arena,PC_CONTACT_MAX*sizeof(PcContactRecord)))return 0;
    memcpy(&out->primary,&args[3],4);memcpy(&out->secondary,&args[4],4);
    if(!isfinite(out->primary) || !isfinite(out->secondary) || out->primary<=0 || out->secondary<=0)return 0;
    out->raw_count=PC_CONTACT_MAX-remaining;out->selected_count=args[2];
    uint32_t selected[PC_CONTACT_MAX];
    if(args[2] && !read(args[1],selected,args[2]*4u))return 0;
    /* Retain both lists, including contacts filtered out before the solver.
     * An explicit summary row survives even when no target contact exists. */
    for(unsigned pass=0;pass<2;pass++) {
        unsigned count=pass?args[2]:out->raw_count;
        for(unsigned i=0;i<count;i++) {
            uint32_t address=pass?selected[i]:arena+(remaining+i)*(uint32_t)sizeof(PcContactRecord);
            PcContactRecord record;
            if(address<arena+remaining*sizeof(PcContactRecord) || address>=arena+PC_CONTACT_MAX*sizeof(PcContactRecord) ||
               (address-arena)%sizeof(PcContactRecord) || !read(address,&record,sizeof record))return 0;
            if(record.owner!=prop && record.other!=prop)continue;
            if(!pc_contact_finite(record.point,3) || !pc_contact_finite(record.normal,3) ||
               !isfinite(record.depth) || !pc_contact_finite(record.coefficient,2))return 0;
            PcContactEntry *entry=&out->contacts[out->count++];
            entry->address=address;entry->index=i;entry->selected=pass;entry->record=record;
        }
    }
    return 1;
}
static void pc_contact_record(PcContactTrace *trace,const PcContactSnapshot *s,int native)
{
    if(!trace->file)return;
    unsigned sequence=trace->sequence++;
    for(unsigned i=0;i<=s->count;i++) {
        PcContactEntry empty={0};
        const PcContactEntry *e=i?&s->contacts[i-1]:&empty;
        const PcContactRecord *c=&e->record;
        char line[1536];
        int size=snprintf(line,sizeof line,
            "%u,%u,%d,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%u,%u,%s,%u,%u,%u,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
            sequence,s->frame,native,trace->slot,s->body,s->prop,s->delta,s->primary,s->secondary,
            s->position[0],s->position[1],s->position[2],s->velocity[0],s->velocity[1],s->velocity[2],
            s->orientation[0],s->orientation[1],s->orientation[2],s->orientation[3],s->raw_count,s->selected_count,
            i?(e->selected?"selected":"generated"):"summary",e->index,e->address,c->owner,c->other,(unsigned)c->kind,c->flags,
            c->point[0],c->point[1],c->point[2],c->normal[0],c->normal[1],c->normal[2],c->depth,c->coefficient[0],c->coefficient[1]);
        if(size<=0 || (size_t)size>=sizeof line || trace->rows>=PC_CONTACT_ROWS ||
           (unsigned)size>PC_CONTACT_BYTES-trace->bytes || fwrite(line,1,(size_t)size,trace->file)!=(size_t)size) {
            fclose(trace->file);trace->file=NULL;return;
        }
        trace->rows++;trace->bytes+=(unsigned)size;
    }
    if(fflush(trace->file)!=0){fclose(trace->file);trace->file=NULL;}
}
#endif
