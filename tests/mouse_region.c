/* Actual host mouse adapter body with controlled OS mappings and input time. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PC_GUEST_MEMORY_H
typedef struct PcGuestRegion {uintptr_t base;size_t size;int writable;} PcGuestRegion;
static unsigned char guest[0x800000];
static unsigned queries;
static int readable=1,writable=1,split,forward,blocked_low;
static uint32_t player_address=0x10000,fov_address=0x30000;
static PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    PcGuestRegion r={0};++queries;
    uintptr_t base=(uintptr_t)guest;
    if(blocked_low && address>=base && address-base<0x20000)return r;
    if(readable && address>=base && address-base<sizeof guest) {
        r.base=base;r.size=sizeof guest;r.writable=writable;
        if(split) {
            r.size=0x20000;
            r.writable=split==2?0:writable;
            if(address-base>=r.size) {r.base+=r.size;r.size=sizeof guest-0x20000;r.writable=split==2?writable:0;}
        }
        if(forward) {
            uintptr_t page=base+((address-base)&~(uintptr_t)0xfff);
            r.size-=page-r.base;r.base=page;
        }
    }
    return r;
}
/* PRODUCTION_MEMORY_HELPERS */
#include "pc_camera.h"
#include "pc_terminal.h"
#include "pc_mounted.h"
static PcInput input;
static double sensitivity=.1;
static int invert_y,rate=144;
static int pc_native_rate(void){return rate;}
#ifdef TEST_SDL
#define camera_input input
#define mouse_sensitivity sensitivity
#define mouse_invert invert_y
static int focused=1,mouse_enabled=1,menu_active,camera_lock;
static unsigned char keys[256];
#define atomic_load(p) (*(p))
#define pthread_mutex_lock(p) ((void)(p))
#define pthread_mutex_unlock(p) ((void)(p))
#define SDL_GetTicks() 1001u
#else
#define VK_LBUTTON 1
static int editing,lock,replaying=1,window_handle;
static unsigned applied;
#define InterlockedCompareExchange(p,a,b) (*(p))
#define AcquireSRWLockExclusive(p) ((void)(p))
#define ReleaseSRWLockExclusive(p) ((void)(p))
#define GetForegroundWindow() 0
#define GetTickCount64() 1001u
static void replay_tick(void){}
#endif
/* PRODUCTION_MOUSE_ADAPTER */
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s (queries=%u)\n",__LINE__,#x,queries);return 1;}}while(0)
#define U(a) (*(uint32_t*)(guest+(a)))
#define F(a) (*(float*)(guest+(a)))
#define MOVE() pc_input_motion(&input,10,5,1000)
#define APPLY() pc_controls_apply_mouse(player_address,fov_address,(ptrdiff_t)guest)
int main(void)
{
    pc_input_focus(&input,1);pc_input_capture(&input,1);
    U(0x10114)=2;F(0x30384)=55;
    MOVE();APPLY();
    CHECK(queries==1 && fabs(F(0x10100)+1)<.001 && fabs(F(0x10104)+.5)<.001);
    /* A different FOV mapping requires another query, and may be read-only. */
    split=1;queries=0;MOVE();APPLY();
    CHECK(queries==2 && fabs(F(0x10100)+2)<.001);
    /* Recheck on every update: neither a read-only nor inaccessible player
     * may inherit the previous update's write permission. */
    split=0;writable=0;queries=0;MOVE();APPLY();
    CHECK(queries>0 && fabs(F(0x10100)+2)<.001);
    readable=0;queries=0;APPLY();
    CHECK(queries>0 && fabs(F(0x10100)+2)<.001);
    readable=writable=1;
    /* Pointer routing and native pause semantics still consume one batch. */
    input.dx=input.dy=0;U(0x10114)=0x2000;U(0x10004)=0;
    U(0x76c27c)=0x20000;U(0x2007c)=0x21000;
    U(0x21014)=0x10000;U(0x21030)=0x22000;F(0x4e794c)=.4166667f;
    F(0x21038)=F(0x2103c)=100;
    queries=0;MOVE();APPLY();
    CHECK(queries==1 && F(0x21038)==101 && F(0x2103c)==100.5f);
    CHECK(F(0x10100)==-2 && !input.dx && input.terminal_active);
    F(0x4e794c)=0;MOVE();APPLY();
    CHECK(F(0x21038)==101 && !input.dx);
    /* Windows returns the queried page and later pages, even in one allocation.
     * A camera below the player must not require a second query of that map. */
    forward=1;player_address=0x30000;fov_address=0x10000;
    U(0x30114)=2;F(0x10384)=55;F(0x30100)=F(0x30104)=0;
    queries=0;MOVE();APPLY();
    CHECK(queries==1 && fabs(F(0x30100)+1)<.001 && fabs(F(0x30104)+.5)<.001);
    /* The lower camera can be a separate read-only mapping. Never extend its
     * permissions over the player; falling back to separate queries is valid. */
    split=2;queries=0;MOVE();APPLY();
    CHECK(queries==3 && fabs(F(0x30100)+2)<.001);
    blocked_low=1;queries=0;MOVE();APPLY();
    CHECK(queries==3 && fabs(F(0x30100)+2)<.001);
    blocked_low=0;split=0;
    /* The interval cache never bridges a region boundary or address overflow. */
    PcGuestRegion region={0};uintptr_t end=(uintptr_t)guest+sizeof guest;
    CHECK(pc_guest_memory_in_region(&region,end-4,4,1));
    CHECK(!pc_guest_memory_in_region(&region,end-4,8,1));
    CHECK(!pc_guest_memory_in_region(&region,UINTPTR_MAX-2,8,0));
    puts("PASS: ordered mouse mappings, split regions, permission expiry, terminal routing and bounds");
    return 0;
}
