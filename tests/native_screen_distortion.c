/* Execute the complete US screen-distortion mesh update and draw dispatch.
 * The grid/active viewport are controlled fixture inputs; graphics services
 * are spies. Lookup-table generation and its FSIN helper remain original.
 * Compare original whole time, an exact test-only fractional probe and the
 * production clock-store policy. Probe time is not used by production. */
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
/* VISUAL_TIME_POLICY */
enum { STACK=0x10000,VIEW=0x4000,PLAYER=0x5000,COLS=11,ROWS=9,CELLS=80 };
static int probe;
static float probe_time;
static unsigned clock_reads,draws,copies,state_calls;
static uint32_t draw_words[CELLS][21];
static float production_numeric(uint32_t site,uint32_t address,int32_t value) {
    return RECOMP_I32_TO_F32_AT(site,address,value);
}
static float numeric_probe(uint32_t site,uint32_t address,int32_t value) {
    if(address==0x7de318u && site==0xf9726u) {
        clock_reads++;
        if(probe==1)return probe_time;
        if(probe==2)return production_numeric(site,address,value);
    }
    return (float)pc_native_numeric(address,value);
}
#undef RECOMP_I32_TO_F32_AT
#define RECOMP_I32_TO_F32_AT(site,address,value) numeric_probe(site,address,(int32_t)(value))
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected distortion tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected distortion callback")
void sub_00019650(void) { state_calls++;g_esp+=4; }
void sub_0001ED50(void) { state_calls++;g_esp+=4; }
void sub_00019D70(void) { state_calls++;g_esp+=4; }
void sub_0001EDE0(void) { state_calls++;g_esp+=4; }
void sub_0001F790(void) { state_calls++;g_esp+=4; }
void sub_0001F090(void) {
    assert(MEM32(g_esp+4)==0 && MEM32(g_esp+8)==0);
    assert(MEM32(g_esp+12)==640 && MEM32(g_esp+16)==448);
    copies++;g_esp+=4;
}
void sub_0001C140(void) {
    assert(draws<CELLS && MEM32(g_esp+4)==0);
    memcpy(draw_words[draws++],(void *)(g_xbox_mem_offset+g_esp+4),84);
    g_esp+=4;
}
/* GENERATED_ROUTINES */
static uint32_t vertex(unsigned row,unsigned col) {
    return 0x7ad2a8+row*0x39c+col*0x54;
}
static void call(void (*function)(void)) {
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    function();
    assert(g_esp==STACK+4 && g_fp_top==0);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
}
static void setup(float amplitude) {
    MEM32(0x7de318)=pc_native_visual_store(0x670e8,0);
    MEM32(0x4e8cec)=60;MEM32(0x4e8cf0)=640;MEM32(0x4e8cf4)=448;
    MEM32(0x74cf64)=0;call(sub_000F8AA0);
    assert(MEM32(0x7ad29c)==64 && MEM32(0x7af31c)==56);
    MEM32(0x7e71c8)=VIEW;MEM32(VIEW)=0;MEM32(VIEW+4)=639;
    MEM32(VIEW+0x14)=0;MEM32(VIEW+0x18)=447;
    MEM32(0x7b0cc0)=PLAYER;MEM32(PLAYER+0x214)=0;
    for(unsigned r=0;r<ROWS;r++)for(unsigned c=0;c<COLS;c++) {
        uint32_t p=vertex(r,c);
        MEM32(p-8)=(r==0 || r==ROWS-1 || c==0 || c==COLS-1);
        MEM32(p)=MEM32(p+0x10)=c*64;
        MEM32(p+4)=MEM32(p+0x14)=r*56;
        MEMF(p+0x28)=MEMF(p+0x34)=amplitude;
    }
    for(unsigned r=0;r<ROWS-1;r++)for(unsigned c=0;c<COLS-1;c++) {
        uint32_t p=0x739dc8+r*0x208+c*0x34;
        MEM32(p)=0;MEM32(p+0x24)=vertex(r,c);
        MEM32(p+0x28)=vertex(r,c+1);MEM32(p+0x2c)=vertex(r+1,c);
        MEM32(p+0x30)=vertex(r+1,c+1);
    }
}
typedef struct Snapshot {
    uint32_t positions[ROWS*COLS][2],draws[CELLS][21];
} Snapshot;
static Snapshot step(unsigned tick,unsigned whole,float delta,float exact) {
    Snapshot result;
    MEM32(0x7de318)=tick;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    probe_time=exact;clock_reads=draws=copies=state_calls=0;
    call(sub_000F9700);
    assert(clock_reads==1 && draws==CELLS && copies==1 && state_calls==7);
    assert(MEM32(0x7de318)==tick && MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    for(unsigned r=0;r<ROWS;r++)for(unsigned c=0;c<COLS;c++) {
        uint32_t p=vertex(r,c);
        result.positions[r*COLS+c][0]=MEM32(p+8);
        result.positions[r*COLS+c][1]=MEM32(p+12);
        assert(MEM32(p+12)==r*56);
        if(MEM32(p-8))assert(MEM32(p+8)==c*64);
    }
    memcpy(result.draws,draw_words,sizeof draw_words);
    return result;
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);
    call(sub_00153C20);
    assert(fabsf(MEMF(0x7a38a0))<.00001f && fabsf(MEMF(0x7a38a0+512*4)-1)<.00001f);
    Snapshot *reference=calloc(481,sizeof *reference);assert(reference);
    setup(.75f);probe=0;g_tsfp_native_timing_active=0;
    for(unsigned t=0;t<=480;t++)reference[t]=step(t,1,1,(float)t);
    const unsigned rates[]={60,120,144};
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned k=0;k<3;k++) {
        unsigned n=rates[k]*slow*8,repeats[3]={0},prev=0;
        for(probe=0;probe<=2;probe++) {
            g_tsfp_native_timing_active=1;setup(.75f);
            Snapshot last=step(0,0,0,0);
            for(unsigned f=1;f<=n;f++) {
                unsigned tick=f*60/(rates[k]*slow);
                float exact=(float)((double)f*60/(rates[k]*slow));
                if(probe==2) {
                    MEM32(0x7de318)=prev;MEM32(0x4e7944)=tick-prev;
                    MEMF(0x4e794c)=60.f/(rates[k]*slow);
                    MEM32(0x7de318)=pc_native_visual_store(0x69265,tick);
                }
                Snapshot current=step(tick,tick-prev,60.f/(rates[k]*slow),exact);
                if(!memcmp(&last,&current,sizeof current))repeats[probe]++;
                if(!probe || f*60%(rates[k]*slow)==0)
                    assert(!memcmp(&current,&reference[tick],sizeof current));
                Snapshot paused=step(tick,0,0,exact);
                assert(!memcmp(&current,&paused,sizeof current));
                last=current;prev=tick;
            }
            prev=0;
        }
        assert(repeats[0]==n-480);
        if(rates[k]*slow>60)assert(repeats[1]<repeats[0]);
        assert(repeats[2]==repeats[1]);
        printf("distortion fps=%u slow=%u frames=%u repeated_original=%u repeated_probe=%u repeated_fixed=%u\n",
               rates[k],slow,n,repeats[0],repeats[1],repeats[2]);
    }
    /* A reset, replay seek or network rebase can write the same time as an
     * earlier frame. The consumer has no history: re-evaluating that clock
     * must reproduce the same mesh and draw packet, even after a jump. */
    const unsigned rebases[]={333,12,0,480,12,12,0};
    for(probe=0;probe<=2;probe++) {
        setup(.75f);
        for(unsigned i=0;i<sizeof rebases/sizeof *rebases;i++) {
            unsigned t=rebases[i];
            MEM32(0x7de318)=pc_native_visual_store(0x357939,t);
            Snapshot current=step(t,0,0,(float)t);
            assert(!memcmp(&current,&reference[t],sizeof current));
        }
    }
    /* Drawing another player's view must sample, not advance, effect time.
     * Feed identical alpha to player 1, then use its original cell filter. */
    for(probe=1;probe<=2;probe++) {
        setup(.75f);
        MEM32(0x7de318)=pc_native_visual_store(0x670e8,12);
        MEM32(0x4e7944)=0;MEMF(0x4e794c)=.25f;
        MEM32(0x7de318)=pc_native_visual_store(0x688bb,12);
        Snapshot first=step(12,0,.25f,12.25f);
        MEM32(PLAYER+0x214)=1;
        for(unsigned r=0;r<ROWS;r++)for(unsigned c=0;c<COLS;c++)
            MEMF(vertex(r,c)+0x38)=.75f;
        for(unsigned r=0;r<ROWS-1;r++)for(unsigned c=0;c<COLS-1;c++)
            MEM32(0x739dc8+r*0x208+c*0x34)=1;
        Snapshot second=step(12,0,.25f,12.25f);
        assert(!memcmp(&first,&second,sizeof first));
    }
    /* No active distortion must avoid all render work in either clock path. */
    for(probe=0;probe<=2;probe++) {
        setup(0);clock_reads=draws=copies=state_calls=0;call(sub_000F9700);
        assert(clock_reads==1 && !draws && !copies && !state_calls);
    }
    puts("PASS: fractional distortion clock matches probe cadence and original tick-boundary meshes/draw arguments");
    free(reference);free(memory);return 0;
}
