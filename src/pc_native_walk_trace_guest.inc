#include "pc_native_walk_trace.h"
#include "pc_native_walk_guest.h"
#ifndef PC_WALK_READ
#include "pc_guest_memory.h"
static PcGuestRegion pc_walk_region;
static int pc_walk_read(uint32_t a,void *out,size_t n)
{
    if(!pc_walk_span(a,n))return 0;
    uintptr_t host=(uintptr_t)a+g_xbox_mem_offset;
    if(!pc_guest_memory_in_region(&pc_walk_region,host,n,0))return 0;
    memcpy(out,(const void *)host,n);return 1;
}
#define PC_WALK_READ pc_walk_read
#define PC_WALK_RESET_REGION() (pc_walk_region=(PcGuestRegion){0})
#else
#define PC_WALK_RESET_REGION() ((void)0)
#endif
extern void sub_000C54A0(void),sub_000C4700(void);
static PcWalkTrace pc_walk_trace;
#if defined(_MSC_VER)
static __declspec(thread) unsigned pc_walk_parent,pc_walk_depth;
#else
static _Thread_local unsigned pc_walk_parent,pc_walk_depth;
#endif
int pc_native_walk_trace_requested(void) { return pc_walk_enabled(&pc_walk_trace); }
int pc_native_walk_collision_requested(void)
{ return pc_native_walk_fractional() || pc_native_walk_trace_requested(); }
int pc_native_walk_sweep_requested(void)
{ return pc_walk_scope!=NULL || pc_native_walk_trace_requested(); }
static void pc_native_walk_pair(void (*original)(void),uint32_t routine,unsigned count)
{
    uint32_t stack=g_esp;PcWalkSnapshot before,after;
    unsigned parent=pc_walk_parent,depth=pc_walk_depth,id=0;
    PC_WALK_RESET_REGION();
    int valid=pc_walk_depth<8 && pc_walk_collect(PC_WALK_READ,stack,count,NULL,&before) &&
        before.scene==pc_walk_trace.scene && before.tick>=pc_walk_trace.first_tick &&
        before.tick<=pc_walk_trace.last_tick && pc_walk_trace.rows<PC_WALK_ROWS &&
        (routine==0xC54A0u ? before.context[0]==0 : pc_walk_parent!=0);
    PC_WALK_RESET_REGION();
    if(valid) {id=++pc_walk_trace.next_id;pc_walk_parent=id;pc_walk_depth++;}
    original();
    if(valid) {
        PC_WALK_RESET_REGION();
        if(pc_walk_collect(PC_WALK_READ,stack,count,&before,&after))
            pc_walk_record(&pc_walk_trace,id,parent,depth,routine,stack,g_esp,g_eax,&before,&after);
        PC_WALK_RESET_REGION();
        pc_walk_parent=parent;pc_walk_depth=depth;
    }
}
static void pc_walk_collision_observed(void)
{
    if(pc_native_walk_trace_requested())pc_native_walk_pair(sub_000C54A0,0xC54A0u,10);
    else sub_000C54A0();
}
static void pc_walk_sweep_observed(void)
{
    if(pc_native_walk_trace_requested())pc_native_walk_pair(sub_000C4700,0xC4700u,9);
    else sub_000C4700();
}
void pc_native_walk_collision(void) { pc_native_walk_guest(pc_walk_collision_observed); }
void pc_native_walk_sweep(void)
{
    uint32_t caller;memcpy(&caller,(const void *)((uintptr_t)g_esp+g_xbox_mem_offset),4);
    pc_native_walk_sweep_guest(pc_walk_sweep_observed,caller,&g_eax);
}
#undef PC_WALK_READ
#undef PC_WALK_RESET_REGION
