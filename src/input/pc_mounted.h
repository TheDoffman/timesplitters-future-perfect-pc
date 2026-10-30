#ifndef TSFP_PC_MOUNTED_H
#define TSFP_PC_MOUNTED_H
#include "pc_guest_span.h"
#include "pc_input.h"
#include <math.h>

static inline uintptr_t pc_mounted_read(uint32_t address,ptrdiff_t offset,size_t size,PcGuestRegion *region)
{
    uintptr_t p=(uintptr_t)address+offset;
    return address && pc_guest_memory_in_span(region,p,size,0)?p:0;
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

/* US 17A120/17A190: vehicle seats have their own gun table, not C14.
 * -1 preserves other driver/passenger look; 0 rejects a binding; 2 is a
 * tracked driver camera and 1 is a
 * gunner. Never cache these links across a seat change or scripted reboarding. */
static inline int pc_vehicle_gun(uintptr_t p,ptrdiff_t offset,PcGuestRegion *region,
                                 uintptr_t *data,uint32_t *gun_address)
{
    uint32_t owner=*(const uint32_t*)(p+0x14),handle,table_address;
    uintptr_t entity=pc_mounted_read(owner,offset,0x80,region),extra,table,vehicle,vdata,seat=0,gun;
    if(!entity)return 0;
    extra=pc_mounted_read(*(const uint32_t*)(entity+0x7c),offset,0xc0c,region);
    table=pc_mounted_read(0x0072f5e8,offset,4,region);
    if(!extra || !table)return 0;
    handle=*(const uint32_t*)(extra+0xc08);table_address=*(const uint32_t*)table;
    uint64_t address=(uint64_t)table_address+(uint64_t)(handle>>22)*0x2e8;
    if(!handle || !table_address || address>UINT32_MAX)return 0;
    vehicle=pc_mounted_read((uint32_t)address,offset,0x80,region);
    if(!vehicle || *(const uint32_t*)(vehicle+0x2c)!=handle)return 0;
    vdata=pc_mounted_read(*(const uint32_t*)(vehicle+0x7c),offset,0x168,region);
    if(!vdata)return 0;
    uint32_t count=*(const uint32_t*)(vdata+0x140),owner_handle=*(const uint32_t*)(entity+0x2c);
    if(!owner_handle || count<1 || count>4)return 0;
    for(uint32_t i=0;i<count;i++) {
        uintptr_t candidate=vdata+0x70+i*0x34;
        if(*(const uint32_t*)candidate==owner_handle) {
            if(seat)return 0; /* Ambiguous ownership must not move either gun. */
            seat=candidate;
        }
    }
    if(!seat)return 0;
    uint32_t type=*(const uint32_t*)(seat+0x30);
    if(type==0 && (*(const uint32_t*)(vdata+0x58)&1))return 2; /* tracked driver camera */
    if(type<=2)return -1;
    if(type!=3 && type!=4)return 0;
    uint32_t index=*(const uint32_t*)(seat+0x2c),guns=*(const uint32_t*)(vdata+0x164);
    if(guns<1 || guns>2 || index>=guns)return 0;
    uint32_t data_address=*(const uint32_t*)(vdata+0x144+index*0x10);
    *data=pc_mounted_read(data_address,offset,0xdc,region);
    if(!*data)return 0;
    *gun_address=*(const uint32_t*)*data;
    gun=pc_mounted_read(*gun_address,offset,0x80,region);
    if(!gun || *(const uint32_t*)(gun+0x7c)!=data_address)return 0;
    /* 17CF30 writes D8; the separate 17CE00 seat-binding path need not.
     * The reciprocal seat handle is authoritative, but reject a rival operator. */
    uint32_t operator_address=*(const uint32_t*)(*data+0xd8);
    return !operator_address || operator_address==owner;
}

/* -1 leaves ordinary look/terminal handling alone; 0/1 consumes gun-mode
 * motion without/with movement. Mode 0x400 uses the vehicle seat gun above.
 * Mode 0x80 covers directly mounted guns and
 * CCTV-controlled guns. Their player angles are not their aiming angles.
 * Follow the same C14 gun link and limits as sub_0025D520/sub_00258A20,
 * checking the reverse operator link so stale or AI-controlled guns stay intact.
 * US B76B0/373E10/3756E0 set the operator at +D8. +68 is an AI target;
 * +4 is an authored instance name (AGUN is only one name), not a type tag. */
static inline int pc_mounted_mouse_in_region(PcInput *input,uint64_t now,float sensitivity,
                                   int invert,uint32_t player,uint32_t fov_object,
                                   ptrdiff_t offset,int native_timing,PcGuestRegion *region)
{
    uintptr_t p=pc_mounted_read(player,offset,0x118,region),entity,extra,gun,data,ticks,f;
    uint32_t owner,gun_address=0,state,mode;
    int vehicle_binding=-1;
    double dx,dy,scale;
    float pitch,yaw,pmin,pmax,ymin,ymax,fov;
    if(!p)return -1;
    mode=*(const uint32_t*)(p+0x114);
    if(mode!=0x80 && mode!=0x400)return -1;
    data=0;
    if(mode==0x400) {
        vehicle_binding=pc_vehicle_gun(p,offset,region,&data,&gun_address);
        if(vehicle_binding<0)return -1;
    }
    uint64_t age=pc_input_consume_motion(input,now,&dx,&dy);
    if(!input->focused || !input->captured || age>100 ||
       !isfinite(dx) || !isfinite(dy) || (!dx && !dy) ||
       !isfinite(sensitivity) || sensitivity<=0 || *(const int32_t*)(p+4)!=0 ||
       (mode==0x400 && !vehicle_binding))return 0;
    ticks=pc_mounted_read(native_timing?0x004e794c:0x004e7944,offset,4,region);
    if(!ticks)return 0;
    /* Raw mouse counts are displacement. Every advancing native interval
     * consumes them once, including frames without a carried whole tick.
     * Paused/invalid time still consumes without moving or replaying later. */
    if(native_timing) {
        float delta=*(const float*)ticks;
        if(!isfinite(delta) || delta<=0)return 0;
    } else if(!*(const uint32_t*)ticks)return 0;
    if(mode==0x80) {
        owner=*(const uint32_t*)(p+0x14);
        entity=pc_mounted_read(owner,offset,0x80,region);
        if(!entity)return 0;
        extra=pc_mounted_read(*(const uint32_t*)(entity+0x7c),offset,0xc18,region);
        if(!extra)return 0;
        gun_address=*(const uint32_t*)(extra+0xc14);
        gun=pc_mounted_read(gun_address,offset,0x80,region);
        if(!gun)return 0;
        data=pc_mounted_read(*(const uint32_t*)(gun+0x7c),offset,0xdc,region);
        if(!data || !pc_guest_memory_in_span(region,data,0x6c,1) ||
           *(const uint32_t*)data!=gun_address ||
           *(const uint32_t*)(data+0xd8)!=owner)return 0;
    }
    if(vehicle_binding==2) {
        f=pc_mounted_read(fov_object,offset,0xfe8,region);
        if(!f || *(uint32_t*)(p+8)!=fov_object ||
           !pc_guest_memory_in_span(region,p+0x100,8,1) ||
           !pc_guest_memory_in_span(region,f+0xfdc,4,1) ||
           !pc_guest_memory_in_span(region,f+0xfe4,4,1))return 0;
        fov=*(const float*)(f+0x384);
        pitch=*(const float*)(p+0x104);yaw=*(const float*)(p+0x100);
        if(!isfinite(fov) || fov<1 || fov>120 || !isfinite(pitch) || !isfinite(yaw))return 0;
        scale=sensitivity*(fov<55?fov/55:1);
        if(!isfinite(dx*scale) || !isfinite(dy*scale))return 0;
        /* D2470 tracked camera uses negative elevation, opposite to on-foot
         * pitch. Keep its original -28..-4 degree orbit limits. */
        *(float*)(p+0x100)=(float)remainder(yaw-dx*scale,360.0);
        *(float*)(p+0x104)=(float)fmax(-28,fmin(-4,pitch+dy*scale*(invert?-1:1)));
        /* Raw mouse displacement updates the camera elevation immediately;
         * the original spring can otherwise retain an old target at rest. */
        *(float*)(f+0xfdc)=*(float*)(f+0xfe4)=*(float*)(p+0x104);
        return 1;
    }
    if(!pc_guest_memory_in_span(region,data,0x6c,1))return 0;
    state=*(const uint32_t*)(data+0x10);
    if(mode==0x400 ? (state==13 || state==14) : (state==1 || state==9))return 0; /* original locked/inactive aim states */
    f=pc_mounted_read(fov_object,offset,0x388,region);
    if(!f)return 0;
    fov=*(const float*)(f+0x384);
    if(!isfinite(fov) || fov<1 || fov>120)return 0;
    scale=sensitivity*(fov<55?fov/55:1);
    if(mode==0x80 && (*(const uint32_t*)(data+8)&0x1000000)) {
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
    if(mode==0x400) {
        /* 17A440 skips equal limits; otherwise it clamps, even for 360 degrees.
         * Bound unlimited angles periodically without changing their direction. */
        double next_pitch=pitch+dy*scale*(invert?-1:1),next_yaw=yaw-dx*scale;
        *(float*)(data+0x1c)=pmin==pmax?(float)remainder(next_pitch,360.0):(float)fmax(pmin,fmin(pmax,next_pitch));
        *(float*)(data+0x24)=ymin==ymax?(float)remainder(next_yaw,360.0):(float)fmax(ymin,fmin(ymax,next_yaw));
        return 1;
    }
    *(float*)(data+0x1c)=pc_mounted_limit(pitch+dy*scale*(invert?-1:1),pmin,pmax);
    *(float*)(data+0x24)=pc_mounted_limit(yaw-dx*scale,ymin,ymax);
    return 1;
}
static inline int pc_mounted_mouse(PcInput *input,uint64_t now,float sensitivity,
                                   int invert,uint32_t player,uint32_t fov_object,
                                   ptrdiff_t offset,int native_timing)
{
    PcGuestRegion region={0};
    return pc_mounted_mouse_in_region(input,now,sensitivity,invert,player,fov_object,offset,native_timing,&region);
}
#endif
