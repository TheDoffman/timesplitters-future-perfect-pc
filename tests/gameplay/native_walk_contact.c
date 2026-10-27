/* Full original C54A0; recorded Venice geometry-service boundaries. The
 * production fractional scope is compared with the unscoped original branch.
 * Data: original60 pair453/native240 pair2045, STATUS trace comparison. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { STACK=0x18000,CONTEXT=0x20000,OLD=0x21000,NEW=0x22000,SCRATCH=0x23000 };
typedef struct { uint32_t game,old[3],wanted[3],hit[3],normal[2],fraction,slide[3],scratch[2],context[8]; } Case;
static const Case cases[]={
    {0x3f800000u,{0xc27af950u,0x3eaa8006u,0xc2036122u},{0xc27b7eddu,0x3ea04295u,0xc2037d85u},{0xc27af95au,0x3ea04295u,0xc2036124u},{0x3f74ddfeu,0x3e9558a8u},0x399677bau,{0xc27afcc8u,0x3ea04295u,0xc20355dau},{0x3eaa8006u,0xc2059ab4u},{0x00000000u,0x0000001au,0x3eaa8006u,0x81019f28u,0x00000000u,0xffffffffu,0x00000000u,0x00000000u}},
    {0x3e7633ceu,{0xc27af6afu,0x3eaa8006u,0xc2036a27u},{0xc27b16cbu,0x3ea809bfu,0xc20370fau},{0xc27af6b0u,0x3ea809bfu,0xc2036a27u},{0x3f76207au,0x3e8cd1edu},0x39087995u,{0xc27af74fu,0x3ea809bfu,0xc20367f8u},{0x3eaa8006u,0xc2058e29u},{0x00000000u,0x0000001au,0x3eaa8006u,0x81019f28u,0x00000000u,0xffffffffu,0x00000000u,0x00000000u}},
};
static unsigned scenario,sweeps;
static int candidate,blocked;
static float value(uint32_t bits) { float f;memcpy(&f,&bits,4);return f; }
/* WALK_POLICY */
void sub_000C54A0(void),sub_000C4700(void);
int pc_native_walk_trace_requested(void) { return 0; }
int pc_native_walk_collision_requested(void) { return pc_native_walk_fractional(); }
int pc_native_walk_sweep_requested(void) { return pc_walk_scope!=NULL; }
void pc_native_walk_collision(void) { pc_native_walk_guest(sub_000C54A0); }
void pc_native_walk_sweep(void) {
    pc_native_walk_sweep_guest(sub_000C4700,MEM32(g_esp),&g_eax);
}
void sub_000C3E60(void) { assert(!"unexpected initial ground query"); }
void sub_000C4EC0(void) { g_esp+=4; }
void sub_000C4FB0(void) { g_esp+=4; }
void sub_000C4700(void)
{
    const Case *c=&cases[scenario];uint32_t ctx=MEM32(g_esp+4),target=MEM32(g_esp+12);
    unsigned n=sweeps++;assert(n<2 && ctx==CONTEXT);
    if(!n) {
        assert(!memcmp((void *)((uintptr_t)target+g_xbox_mem_offset),c->wanted,12));
        memcpy((void *)((uintptr_t)target+g_xbox_mem_offset),c->hit,12);
        uint32_t out=MEM32(g_esp+28);assert(out);
        MEM32(out)=0x26000;MEM32(out+4)=c->normal[0];MEM32(out+8)=c->normal[1];
        MEM32(out+12)=c->fraction;MEM32(out+16)=0x27000;
        MEM32(0x732f78)=c->normal[0];MEM32(0x732f7c)=c->normal[1];
        uint32_t position_out=MEM32(g_esp+24);
        if(position_out)memcpy((void *)((uintptr_t)position_out+g_xbox_mem_offset),c->slide,12);
        MEM32(ctx+24)=1;g_eax=6;
    } else {
        /* The second sweep receives the candidate supplied by the first
         * geometry service. A clear result leaves it unchanged; only the
         * original C54A0 body decides whether the player keeps that result. */
        for(unsigned i=0;i<3;i++)assert(fabsf(MEMF(target+4*i)-value(c->slide[i]))<.00002f);
        uint32_t position_out=MEM32(g_esp+24);
        if(position_out)memcpy((void *)((uintptr_t)position_out+g_xbox_mem_offset),(const void *)((uintptr_t)target+g_xbox_mem_offset),12);
        MEM32(ctx+24)=0;g_eax=blocked?2u:0u;
    }
    g_esp+=4;
}
void sub_000C4350(void)
{
    MEM32(NEW+4)=cases[scenario].old[1];MEM32(CONTEXT+24)=0;
    MEM32(0x732f78)=cases[scenario].scratch[0];MEM32(0x732f7c)=cases[scenario].scratch[1];
    g_eax=1;g_esp+=4;
}
/* GENERATED_ROUTINES */
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    for(scenario=0;scenario<2;scenario++)for(int native=0;native<2;native++)
    for(blocked=0;blocked<2;blocked++)for(candidate=0;candidate<2;candidate++) {
        memset(memory,0,0x800000);/* ORIGINAL_CONSTANTS */
        const Case *c=&cases[scenario];g_tsfp_native_timing_active=native;sweeps=0;
        MEM32(0x790950)=1;MEM32(0x4e794c)=c->game;
        memcpy(memory+CONTEXT,c->context,32);memcpy(memory+OLD,c->old,12);memcpy(memory+NEW,c->wanted,12);
        g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=CONTEXT;
        MEM32(STACK+8)=OLD;MEM32(STACK+12)=NEW;
        MEM32(STACK+16)=1072924774u;MEM32(STACK+20)=1057442759u;
        MEM32(STACK+24)=SCRATCH;MEM32(STACK+28)=1;
        g_ebx=0x123;g_esi=0x789;g_edi=0xabc;g_ebp=g_seh_ebp=0x456;
        if(candidate)RECOMP_DIRECT_CALLEE_AT(0xc5ad6u,sub_000C54A0)();
        else sub_000C54A0();
        assert(!pc_walk_scope);
        assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
        assert(g_ebx==0x123 && g_esi==0x789 && g_edi==0xabc && g_ebp==0x456);
        assert(g_eax==7 && sweeps==2 && MEM32(NEW+4)==c->old[1]);
        float distance=hypotf(MEMF(NEW)-value(c->old[0]),MEMF(NEW+8)-value(c->old[2]));
        printf("walk contact case=%u native=%d blocked=%d policy=%d distance=%.9g\n",scenario,native,blocked,candidate,distance);
        if(scenario && !(candidate && native && !blocked))assert(distance==0.f);
        else {
            assert(distance>.001f);
            assert(MEM32(NEW)==c->slide[0] && MEM32(NEW+8)==c->slide[2]);
        }
    }
    free(memory);puts("PASS: original contact acceptance reproduces rewind and isolates final branch");return 0;
}
