/* Original TRAP solver and original sine-table initialization. Sound and
 * callback removal are service spies; no rewritten motion model is used. */
#include <assert.h>
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
enum { ENTITY=0x1000, STACK=0x10000, BYTES=0x1a8 };
static unsigned sounds,controls,removals;
static int legacy,naive,retired;
float pc_native_visual_sample(uint32_t value) {
    (void)value;assert(!"unexpected visual-time read");return 0;
}
void sub_00046A00(void) { sounds++;g_eax=0x1234;g_esp+=4; }
void sub_000462B0(void) {
    assert(MEM32(g_esp+4)==4);controls++;g_esp+=4;
}
void sub_000A5CE0(void) {
    assert(MEM32(g_esp+4)==ENTITY && MEM32(g_esp+8)==3 && !MEM32(g_esp+12));
    retired=1;removals++;g_esp+=4;
}
static float trap_policy(unsigned site,unsigned address,int value) {
    return RECOMP_I32_TO_F32_AT(site,address,value);
}
#undef RECOMP_I32_TO_F32_AT
#define RECOMP_I32_TO_F32_AT(site,address,value) \
    ((legacy && ((site)==0x1d8af5 || (site)==0x1d8b6a)) ? \
     RECOMP_I32_TO_F32(address,value) : \
     trap_policy((site),(address),(value)))
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((naive && g_tsfp_native_timing_active && (site)==0x1d8ab0) ? \
     (MEMF(0x4e794c)>0) : pc_native_global_load(site,address,value))
/* GENERATED_ROUTINES */
typedef struct { unsigned char entity[BYTES];unsigned sounds,controls,removals; } State;
static void reset(void) {
    memset((void *)(g_xbox_mem_offset+ENTITY),0,BYTES);
    MEMF(ENTITY+0xc0)=MEMF(0x49dedc);sounds=controls=removals=0;retired=0;
}
static State step(unsigned whole,float delta) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=STACK;MEM32(STACK)=0x12345678;MEM32(STACK+4)=ENTITY;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    if(!retired)sub_001D8AB0();else g_esp+=4;
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    State result={0};memcpy(result.entity,(void *)(g_xbox_mem_offset+ENTITY),BYTES);
    result.sounds=sounds;result.controls=controls;result.removals=removals;return result;
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x4e8cec)=60;g_esp=STACK;MEM32(STACK)=0;sub_00153C20();
    assert(g_esp==STACK+4 && !g_fp_top);setvbuf(stdout,NULL,_IONBF,0);
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned r=0;r<5;r++)for(unsigned slow=1;slow<=4;slow*=4)
    for(unsigned phase=0;phase<4;phase++) {
        unsigned den=rates[r]*slow,count=den*10;
        State *reference=calloc(count,sizeof *reference);assert(reference);
        for(unsigned mode=0;mode<4;mode++) {
            reset();g_tsfp_native_timing_active=mode!=0;
            legacy=mode==2;naive=mode==3;unsigned previous=0,mismatches=0;
            for(unsigned f=1;f<=count;f++) {
                unsigned tick=(f*240+phase*den)/(den*4);
                State result=step(tick-previous,60.f/den);previous=tick;
                if(!mode)reference[f-1]=result;
                else if(memcmp(&result,&reference[f-1],sizeof result))mismatches++;
            }
            if(!mode)assert(sounds==1 && controls==1 && removals==1);
            printf("trap fps=%u slow=%u phase=%u mode=%u mismatches=%u\n",
                   rates[r],slow,phase,mode,mismatches);
            if(mode==1)assert(!mismatches);
            if(mode>=2 && den>60)assert(mismatches);
        }
        free(reference);
    }
    legacy=naive=0;
    reset();g_tsfp_native_timing_active=1;State paused=step(0,0);
    State stale=step(1,0);assert(!memcmp(&paused,&stale,sizeof paused));
    stale=step(0,.25f);assert(!memcmp(&paused,&stale,sizeof paused));
    /* Authored catch-up step sizes retain complete original state/events. */
    State saved={0};const unsigned spans[]={1,3,2,7,30,90,180,300};
    for(unsigned native=0;native<2;native++) {
        reset();g_tsfp_native_timing_active=native;State current={0};
        for(unsigned i=0;i<sizeof spans/sizeof spans[0];i++)current=step(spans[i],(float)spans[i]);
        if(!native)saved=current;else assert(!memcmp(&saved,&current,sizeof saved));
    }
    puts("PASS: original TRAP motion, phase, sound/removal, carried phases and catch-up parity");
    free(memory);return 0;
}
