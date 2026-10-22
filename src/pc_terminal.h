#ifndef TSFP_PC_TERMINAL_H
#define TSFP_PC_TERMINAL_H
#include "pc_guest_memory.h"
#include "pc_input.h"
#include <math.h>

/* US sub_003700D0 routes the active console's stick input to its 256-unit
 * pointer, not player yaw/pitch. The local input index selects the console;
 * the guest player ID can differ (for example on a network client). */
static inline uintptr_t pc_terminal_read(uint32_t address,ptrdiff_t offset,size_t size)
{
    uintptr_t p=(uintptr_t)address+offset;
    return address && pc_guest_memory(p,size,0)?p:0;
}

/* -1: ordinary camera; 0/1: terminal handled without/with pointer movement.
 * Consume terminal motion even when paused, scripted or temporarily invalid,
 * so it cannot turn the hidden player or leak into gameplay on exit. */
static inline int pc_terminal_mouse(PcInput *input,uint64_t now,float sensitivity,
                                    uint32_t player,ptrdiff_t offset,int native_timing)
{
    uintptr_t p=pc_terminal_read(player,offset,0x118),slot,entity,t,descriptor,ticks;
    double dx,dy;
    int32_t index;
    float *x,*y;
    if(!p || *(const uint32_t*)(p+0x114)!=0x2000)return -1;
    uint64_t age=pc_input_consume_motion(input,now,&dx,&dy);
    if(!input->focused || !input->captured || age>100 ||
       !isfinite(dx) || !isfinite(dy) || (!dx && !dy) ||
       !isfinite(sensitivity) || sensitivity<=0)return 0;
    index=*(const int32_t*)(p+4);
    if(index<0 || index>=4)return 0;
    slot=pc_terminal_read(0x0076C27C+(uint32_t)index*4,offset,4);
    if(!slot)return 0;
    entity=pc_terminal_read(*(const uint32_t*)slot,offset,0x80);
    if(!entity)return 0;
    t=pc_terminal_read(*(const uint32_t*)(entity+0x7c),offset,0x60);
    if(!t || !pc_guest_memory(t,0x60,1) ||
       *(const uint32_t*)(t+0x14)!=player || *(const uint32_t*)(t+0x58))return 0;
    descriptor=pc_terminal_read(*(const uint32_t*)(t+0x30),offset,0x18);
    ticks=pc_terminal_read(native_timing?0x004E794C:0x004E7944,offset,4);
    if(!descriptor || (*(const uint32_t*)(descriptor+0x14)&4) ||
       !ticks)return 0;
    /* Native zero-whole intervals still accept raw counts while game time
     * advances. A zero/nonfinite float delta consumes without moving during
     * pause, even if the adjacent whole counter is stale and positive. */
    if(native_timing) {
        float delta=*(const float*)ticks;
        if(!isfinite(delta) || delta<=0)return 0;
    } else if(!*(const uint32_t*)ticks)return 0;
    x=(float*)(t+0x38);y=(float*)(t+0x3c);
    if(!isfinite(*x) || !isfinite(*y))return 0;
    /* Logical coordinates keep pointer travel independent of resolution.
     * Aim inversion and scoped FOV scaling do not apply to a menu pointer. */
    *x=(float)fmin(251.0,fmax(4.0,(double)*x+dx*sensitivity));
    *y=(float)fmin(251.0,fmax(4.0,(double)*y+dy*sensitivity));
    *(uint32_t*)(t+0x20)=0; /* Same inactivity reset as controller movement. */
    return 1;
}
#endif
