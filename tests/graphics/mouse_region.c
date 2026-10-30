/* Actual host mouse adapter body with controlled OS mappings and input time. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PC_GUEST_MEMORY_H
#define TSFP_PC_GUEST_SPAN_H
typedef struct PcGuestRegion {uintptr_t base;size_t size;int writable;} PcGuestRegion;
static unsigned char guest[0x800000];
static unsigned queries;
static int readable=1,writable=1,split,forward,blocked_low,short_gun;
static uint32_t player_address=0x10000,fov_address=0x30000;
static PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    PcGuestRegion r={0};++queries;
    uintptr_t base=(uintptr_t)guest;
    /* A readable angle prefix must not make an inaccessible operator readable. */
    if(short_gun && address>=base+0x60000 && address<base+0x61000) {
        r.base=base+0x60000;r.size=0xd8;r.writable=1;return r;
    }
    if(blocked_low && address>=base && address-base<0x20000)return r;
    if(readable && address>=base && address-base<sizeof guest) {
        r.base=base;r.size=sizeof guest;r.writable=writable;
        if(short_gun && address<base+0x60000)r.size=0x60000;
        if(short_gun && address>=base+0x61000) {r.base=base+0x61000;r.size=sizeof guest-0x61000;}
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
static PcGuestRegion pc_guest_query_span(uintptr_t address,size_t bytes)
{ (void)bytes;return pc_guest_query_region(address); }
/* PRODUCTION_MEMORY_HELPERS */
#include "pc_camera.h"
#include "pc_reload_guest.h"
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
    CHECK(input.reload.eligible && input.reload.player==player_address);
    /* Updating a second local player must preserve port 1's pending request. */
    pc_input_key_at(&input,'R',1,1001);
    U(0x10004)=1;APPLY();U(0x10004)=0;
    CHECK(input.reload.eligible && (input.reload.events['R']&1));
    queries=1;
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
    /* A second split-screen camera must neither move nor consume player one's
     * pending mouse packet, regardless of its guest player ID. */
    input.dx=input.dy=0;U(0x40000)=2;U(0x40004)=1;U(0x40114)=2;
    player_address=0x40000;MOVE();APPLY();
    CHECK(F(0x40100)==0 && F(0x30100)==-2 && input.dx==10 && input.dy==5);
    U(0x30000)=7;U(0x30004)=0;player_address=0x30000;APPLY();
    CHECK(fabs(F(0x30100)+3)<.001 && F(0x40100)==0 && !input.dx);
    /* Both real adapter bodies must route differently named mounted guns,
     * validate the operator past the angle fields, and recheck after exit. */
    forward=0;U(0x30114)=0x80;U(0x30014)=0x40000;U(0x4007c)=0x42000;
    U(0x42c14)=0x50000;U(0x5007c)=0x60000;U(0x60000)=0x50000;
    U(0x60004)=0x54414e4b;U(0x60010)=4;U(0x600d8)=0x40000;
    F(0x60034)=-90;F(0x60038)=90;F(0x6003c)=-180;F(0x60040)=180;
    F(0x4e794c)=.4166667f;
    float old_pitch=F(0x30104);
    MOVE();APPLY();CHECK(F(0x60024)==-1 && F(0x6001c)==.5f);
    CHECK(F(0x30100)==-3 && F(0x30104)==old_pitch && !input.dx);
    short_gun=1;MOVE();APPLY();
    CHECK(F(0x60024)==-1 && !input.dx);short_gun=0;
    U(0x600d8)=0;U(0x60068)=0x40000;MOVE();APPLY();
    CHECK(F(0x60024)==-1 && !input.dx); /* AI target cannot authorize the mouse. */
    U(0x600d8)=0x40000;U(0x60004)=0;MOVE();APPLY();
    CHECK(F(0x60024)==-2 && F(0x6001c)==1 && !input.dx);
    U(0x30114)=2;APPLY();CHECK(F(0x30100)==-3 && F(0x30104)==old_pitch);
    /* Vehicle gunner mode must take the same route through both host adapters. */
    U(0x30114)=0x400;U(0x4002c)=77;U(0x42c08)=0x400001;
    U(0x72f5e8)=0x70000;U(0x70314)=0x400001;U(0x70364)=0x71000;
    U(0x71140)=1;U(0x71070)=77;U(0x710a0)=3;U(0x7109c)=0;
    U(0x71164)=1;U(0x71144)=0x60000;U(0x42c14)=0;
    MOVE();APPLY();CHECK(F(0x60024)==-3 && F(0x6001c)==1.5f && !input.dx);
    CHECK(F(0x30100)==-3 && F(0x30104)==old_pitch);
    short_gun=1;MOVE();APPLY();CHECK(F(0x60024)==-3 && !input.dx);short_gun=0;
    writable=0;MOVE();APPLY();CHECK(F(0x60024)==-3 && !input.dx);writable=1;
    U(0x30114)=2;
    /* The interval cache never bridges a region boundary or address overflow. */
    PcGuestRegion region={0};uintptr_t end=(uintptr_t)guest+sizeof guest;
    CHECK(pc_guest_memory_in_span(&region,end-4,4,1));
    CHECK(!pc_guest_memory_in_span(&region,end-4,8,1));
    CHECK(!pc_guest_memory_in_span(&region,UINTPTR_MAX-2,8,0));
    puts("PASS: ordered mouse mappings, split regions, permission expiry, terminal routing and bounds");
    return 0;
}
