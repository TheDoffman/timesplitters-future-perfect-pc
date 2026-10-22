#ifndef PC_NATIVE_POSE_TRACE_H
#define PC_NATIVE_POSE_TRACE_H
/* Optional completed-update diagnostics, shared by both native hosts.
 * The US gameplay loop calls the clock at 0x3D3E6, after sub_0003C440.
 * This is an update boundary, not proof of GPU completion or scanout. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define PC_NATIVE_POSE_LIMIT 20000u
typedef uint32_t (*PcNativePoseRead)(uint32_t);
typedef struct {
    uint32_t frame,scene,tick,entity[2];
    float delta,position[2][3];
} PcNativePose;
typedef struct { FILE *file;unsigned checked,rows; } PcNativePoseTrace;
static int pc_native_pose_address(uint32_t address)
{ return address>=0x80000000u && address<=0x83fff000u; }
static float pc_native_pose_float(PcNativePoseRead read,uint32_t address)
{ uint32_t bits=read(address);float value;memcpy(&value,&bits,sizeof value);return value; }
static int pc_native_pose_collect(PcNativePoseRead read,uint32_t caller,PcNativePose *pose)
{
    if(caller!=0x0003D3EBu || read(0x76b1a4u)!=15u || read(0x7de30cu)!=7u ||
       (read(0x7de400u)&255u)!=2u)return 0;
    uint32_t pool=read(0x7356d8u);
    if(!pc_native_pose_address(pool))return 0;
    pose->frame=read(0x7497b8u);pose->scene=read(0x79094cu);pose->tick=read(0x7de318u);
    pose->delta=pc_native_pose_float(read,0x4e794cu);
    for(unsigned slot=0;slot<2;slot++) {
        pose->entity[slot]=read(pool+slot*0xea0u);
        if(!pc_native_pose_address(pose->entity[slot]))return 0;
        for(unsigned axis=0;axis<3;axis++) {
            float value=pc_native_pose_float(read,pose->entity[slot]+0x64u+axis*4u);
            if(!isfinite(value))return 0;
            pose->position[slot][axis]=value;
        }
    }
    return 1;
}
static int pc_native_pose_enabled(PcNativePoseTrace *trace)
{
    if(!trace->checked) {
        const char *path=getenv("TSFP_NATIVE_POSE_TRACE");trace->checked=1;
        if(path && *path) {
            trace->file=fopen(path,"w");
            if(trace->file)fputs("frame,seconds,scene,level_ticks,game_delta,entity0,x0,y0,z0,entity1,x1,y1,z1\n",trace->file);
            else fprintf(stderr,"[POSE-TRACE] Unable to open requested trace\n");
        }
    }
    return trace->file!=NULL;
}
static void pc_native_pose_record(PcNativePoseTrace *trace,const PcNativePose *pose,double seconds)
{
    if(!trace->file)return;
    fprintf(trace->file,"%u,%.9f,%u,%u,%.9g,%u,%.9g,%.9g,%.9g,%u,%.9g,%.9g,%.9g\n",
        pose->frame,seconds,pose->scene,pose->tick,pose->delta,
        pose->entity[0],pose->position[0][0],pose->position[0][1],pose->position[0][2],
        pose->entity[1],pose->position[1][0],pose->position[1][1],pose->position[1][2]);
    if(++trace->rows%120u==0)fflush(trace->file);
    if(trace->rows>=PC_NATIVE_POSE_LIMIT){fclose(trace->file);trace->file=NULL;}
}
#endif
