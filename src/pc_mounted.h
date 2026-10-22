#ifndef TSFP_PC_MOUNTED_H
#define TSFP_PC_MOUNTED_H
#include "pc_guest_memory.h"
#include "pc_input.h"
#include <math.h>

static inline uintptr_t pc_mounted_read(uint32_t address,ptrdiff_t offset,size_t size)
{
    uintptr_t p=(uintptr_t)address+offset;
    return address && pc_guest_memory(p,size,0)?p:0;
}

/* US sub_00258A20: equal limits lock an axis, a full revolution wraps it,
 * otherwise the gun's authored minimum/maximum clamp it. */
static inline float pc_mounted_limit(double angle,float minimum,float maximum)
{
    if(minimum==maximum)return minimum;
    if(fabs((double)maximum-minimum)>=360.0)
        return (float)(angle-floor((angle+180.0)/360.0)*360.0);
    return (float)fmax(minimum,fmin(maximum,angle));
}

/* -1 leaves ordinary look/terminal handling alone; 0/1 consumes gun-mode
 * motion without/with movement. Mode 0x80 covers directly mounted AGUNs and
 * CCTV-controlled guns. Their player angles are not their aiming angles.
 * Follow the same C14 gun link and limits as sub_0025D520/sub_00258A20,
 * checking the reverse owner link so stale or AI-controlled guns stay intact. */
static inline int pc_mounted_mouse(PcInput *input,uint64_t now,float sensitivity,
                                   int invert,uint32_t player,uint32_t fov_object,
                                   ptrdiff_t offset,int native_timing)
{
    uintptr_t p=pc_mounted_read(player,offset,0x118),entity,extra,gun,data,ticks,f;
    uint32_t owner,gun_address,state;
    double dx,dy,scale;
    float pitch,yaw,pmin,pmax,ymin,ymax,fov;
    if(!p || *(const uint32_t*)(p+0x114)!=0x80)return -1;
    uint64_t age=pc_input_consume_motion(input,now,&dx,&dy);
    if(!input->focused || !input->captured || age>100 ||
       !isfinite(dx) || !isfinite(dy) || (!dx && !dy) ||
       !isfinite(sensitivity) || sensitivity<=0 || *(const int32_t*)(p+4)!=0)return 0;
    ticks=pc_mounted_read(native_timing?0x004e794c:0x004e7944,offset,4);
    if(!ticks)return 0;
    /* Raw mouse counts are displacement. Every advancing native interval
     * consumes them once, including frames without a carried whole tick.
     * Paused/invalid time still consumes without moving or replaying later. */
    if(native_timing) {
        float delta=*(const float*)ticks;
        if(!isfinite(delta) || delta<=0)return 0;
    } else if(!*(const uint32_t*)ticks)return 0;
    owner=*(const uint32_t*)(p+0x14);
    entity=pc_mounted_read(owner,offset,0x80);
    if(!entity)return 0;
    extra=pc_mounted_read(*(const uint32_t*)(entity+0x7c),offset,0xc18);
    if(!extra)return 0;
    gun_address=*(const uint32_t*)(extra+0xc14);
    gun=pc_mounted_read(gun_address,offset,0x80);
    if(!gun)return 0;
    data=pc_mounted_read(*(const uint32_t*)(gun+0x7c),offset,0x6c);
    if(!data || !pc_guest_memory(data,0x6c,1) ||
       *(const uint32_t*)data!=gun_address || *(const uint32_t*)(data+4)!=0x4147554e ||
       *(const uint32_t*)(data+0x68)!=owner)return 0;
    state=*(const uint32_t*)(data+0x10);
    if(state==1 || state==9)return 0; /* original locked/inactive aim states */
    f=pc_mounted_read(fov_object,offset,0x388);
    if(!f)return 0;
    fov=*(const float*)(f+0x384);
    if(!isfinite(fov) || fov<1 || fov>120)return 0;
    scale=sensitivity*(fov<55?fov/55:1);
    if(*(const uint32_t*)(data+8)&0x1000000) {
        float speed=*(const float*)(data+0x5c);
        if(!isfinite(speed) || speed<0)return 0;
        scale*=speed;
    }
    pitch=*(const float*)(data+0x1c);yaw=*(const float*)(data+0x24);
    pmin=*(const float*)(data+0x34);pmax=*(const float*)(data+0x38);
    ymin=*(const float*)(data+0x3c);ymax=*(const float*)(data+0x40);
    if(!isfinite(pitch) || !isfinite(yaw) || !isfinite(pmin) || !isfinite(pmax) ||
       !isfinite(ymin) || !isfinite(ymax) || pmin>pmax || ymin>ymax ||
       !isfinite(dx*scale) || !isfinite(dy*scale))return 0;
    /* Gun pitch has the opposite sign to player pitch. Raw mouse deltas are
     * already displacement, so do not multiply them by guest frame time. */
    *(float*)(data+0x1c)=pc_mounted_limit(pitch+dy*scale*(invert?-1:1),pmin,pmax);
    *(float*)(data+0x24)=pc_mounted_limit(yaw-dx*scale,ymin,ymax);
    return 1;
}
#endif
