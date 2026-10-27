#ifndef PC_NATIVE_LOCAL_TRACE_H
#define PC_NATIVE_LOCAL_TRACE_H
/* Opt-in CPU observations after US sub_0003C440 returns, before the next
 * timing interval is published. No input, guest memory or animation is changed.
 * This is not a rendered-pose, GPU-completion or physical-latency trace. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define PC_NATIVE_LOCAL_LIMIT 20000u
#define PC_NATIVE_LOCAL_BYTES (8u*1024u*1024u)
typedef uint32_t (*PcNativeLocalRead)(uint32_t);
typedef struct {
    uint32_t entity,model,clip,cycle,next_clip;
    float time,previous_time,speed,next_time;
} PcNativeLocalAnimation;
typedef struct {
    uint32_t frame,scene,tick,raw_ticks,game_ticks,suppression,player,body,camera,valid;
    float delta,position[3],look[2],camera_position[3],camera_direction[3];
    PcNativeLocalAnimation animation[3];
} PcNativeLocalPose;
typedef struct { FILE *file;unsigned checked,rows,bytes; } PcNativeLocalTrace;
static int pc_native_local_address(uint32_t address,uint32_t size)
{ return address>=0x80000000u && size<=0x04000000u && address<=0x84000000u-size; }
static float pc_native_local_float(PcNativeLocalRead read,uint32_t address)
{ uint32_t bits=read(address);float value;memcpy(&value,&bits,4);return value; }
static int pc_native_local_animation(PcNativeLocalRead read,uint32_t entity,PcNativeLocalAnimation *out)
{
    PcNativeLocalAnimation a={0};
    if(!pc_native_local_address(entity,8))return 0;
    a.entity=entity;a.model=read(entity+4);
    if(!pc_native_local_address(a.model,0x13cu))return 0;
    a.clip=read(a.model+0xd4u);a.cycle=read(a.model+0x124u);
    a.time=pc_native_local_float(read,a.model+0xdcu);
    a.previous_time=pc_native_local_float(read,a.model+0xe0u);
    a.speed=pc_native_local_float(read,a.model+0x108u);
    a.next_clip=read(a.model+0x12cu);a.next_time=pc_native_local_float(read,a.model+0x134u);
    if(!isfinite(a.time)||!isfinite(a.previous_time)||!isfinite(a.speed)||!isfinite(a.next_time))return 0;
    *out=a;return 1;
}
static int pc_native_local_collect(PcNativeLocalRead read,uint32_t caller,PcNativeLocalPose *out)
{
    if(caller!=0x0003d3ebu)return 0;
    uint32_t scene=read(0x79094cu),match=read(0x7de30cu);
    /* A local player in Story or an active Arcade match; menus can retain
     * old player pointers. Split-screen/LAN require their own observations. */
    if(scene>=100u || (match!=1u && match!=7u) || (read(0x7de400u)&255u)!=1u)return 0;
    PcNativeLocalPose p={0};
    p.player=read(0x7b0c7cu);
    if(!pc_native_local_address(p.player,0xe88u))return 0;
    p.body=read(p.player+0x14u);
    if(!pc_native_local_address(p.body,0x1d0u))return 0;
    p.frame=read(0x7497b8u);p.scene=scene;p.tick=read(0x7de318u);
    p.raw_ticks=read(0x4e7940u);p.game_ticks=read(0x4e7944u);
    p.delta=pc_native_local_float(read,0x4e794cu);p.suppression=read(0x6b9408u);
    if(!isfinite(p.delta)||p.delta<0)return 0;
    for(unsigned i=0;i<3;i++) {
        p.position[i]=pc_native_local_float(read,p.player+0xb4u+i*4u);
        if(!isfinite(p.position[i]))return 0;
    }
    for(unsigned i=0;i<2;i++) {
        p.look[i]=pc_native_local_float(read,p.player+0x100u+i*4u);
        if(!isfinite(p.look[i]))return 0;
    }
    uint32_t entities[3]={p.body,read(p.player+0x5b0u),read(p.player+0xe84u)};
    for(unsigned i=0;i<3;i++)if(pc_native_local_animation(read,entities[i],&p.animation[i]))p.valid|=1u<<i;
    p.camera=read(0x7b0cc0u);
    if(pc_native_local_address(p.camera,0x340u)) {
        unsigned finite=1;
        for(unsigned i=0;i<3;i++) {
            p.camera_position[i]=pc_native_local_float(read,p.camera+0x27cu+i*4u);
            p.camera_direction[i]=pc_native_local_float(read,p.camera+0x334u+i*4u);
            finite&=isfinite(p.camera_position[i])&&isfinite(p.camera_direction[i]);
        }
        if(finite)p.valid|=8u;
        else { memset(p.camera_position,0,sizeof p.camera_position);memset(p.camera_direction,0,sizeof p.camera_direction); }
    }
    *out=p;return 1;
}
static int pc_native_local_enabled(PcNativeLocalTrace *trace)
{
    if(!trace->checked) {
        trace->checked=1;const char *path=getenv("TSFP_NATIVE_LOCAL_TRACE");
        if(path&&*path) {
            trace->file=fopen(path,"wb");
            if(trace->file) {
                int n=fputs("frame,seconds,scene,level_ticks,raw_ticks,game_ticks,game_delta,suppression,player,body,camera,x,y,z,yaw,pitch,cam_x,cam_y,cam_z,dir_x,dir_y,dir_z,valid",trace->file);
                const char *names[]={"body","hand","weapon"};
                for(unsigned i=0;i<3;i++)fprintf(trace->file,",%s_entity,%s_model,%s_clip,%s_cycle,%s_time,%s_previous_time,%s_speed,%s_next_clip,%s_next_time",names[i],names[i],names[i],names[i],names[i],names[i],names[i],names[i],names[i]);
                if(n<0||fputc('\n',trace->file)==EOF||ftell(trace->file)<0) {fclose(trace->file);trace->file=NULL;}
                else trace->bytes=(unsigned)ftell(trace->file);
            } else fprintf(stderr,"[LOCAL-TRACE] Unable to open requested trace\n");
        }
    }
    return trace->file!=NULL;
}
static void pc_native_local_record(PcNativeLocalTrace *trace,const PcNativeLocalPose *p,double seconds)
{
    if(!trace->file)return;
    char line[2048];size_t used=0;
#define PC_LOCAL_APPEND(...) do { int n=snprintf(line+used,sizeof line-used,__VA_ARGS__); if(n<0||(size_t)n>=sizeof line-used)goto close_trace; used+=(size_t)n; } while(0)
    PC_LOCAL_APPEND("%u,%.9f,%u,%u,%u,%u,%.9g,%u,%u,%u,%u,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%u",
        p->frame,seconds,p->scene,p->tick,p->raw_ticks,p->game_ticks,p->delta,p->suppression,p->player,p->body,p->camera,
        p->position[0],p->position[1],p->position[2],p->look[0],p->look[1],p->camera_position[0],p->camera_position[1],p->camera_position[2],p->camera_direction[0],p->camera_direction[1],p->camera_direction[2],p->valid);
    for(unsigned i=0;i<3;i++) {
        const PcNativeLocalAnimation *a=&p->animation[i];
        PC_LOCAL_APPEND(",%u,%u,%u,%u,%.9g,%.9g,%.9g,%u,%.9g",a->entity,a->model,a->clip,a->cycle,a->time,a->previous_time,a->speed,a->next_clip,a->next_time);
    }
    PC_LOCAL_APPEND("\n");
#undef PC_LOCAL_APPEND
    if(used>PC_NATIVE_LOCAL_BYTES-trace->bytes || fwrite(line,1,used,trace->file)!=used)goto close_trace;
    trace->bytes+=(unsigned)used;
    if(++trace->rows%120u==0 && fflush(trace->file)!=0)goto close_trace;
    if(trace->rows<PC_NATIVE_LOCAL_LIMIT)return;
close_trace:
    fclose(trace->file);trace->file=NULL;
}
#endif
