/* Complete original strip-distortion routine and sine-table generator.
 * Render services are spies: compare their arguments, not GPU output. */
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
enum { STACK=0x10000,VIEW=0x4000,PLAYER=0x5000,STRIPS=224 };
typedef struct Snapshot { uint32_t words[STRIPS][9]; } Snapshot;
static Snapshot output;
static unsigned draws,states,reads;
static int probe;
static float exact_time;
static float production_numeric(uint32_t site,uint32_t address,int32_t value) {
    return RECOMP_I32_TO_F32_AT(site,address,value);
}
static float sample(uint32_t site,uint32_t address,int32_t value) {
    if(site==0xf9b4b && address==0x7de318) {
        reads++;
        if(probe==1)return exact_time;
        if(probe==2)return production_numeric(site,address,value);
    }
    return (float)pc_native_numeric(address,value);
}
#undef RECOMP_I32_TO_F32_AT
#define RECOMP_I32_TO_F32_AT(site,address,value) sample(site,address,(int32_t)(value))
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected strip tail")
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected strip callback")
void sub_00019650(void) { states++;g_esp+=4; }
void sub_00019D70(void) { states++;g_esp+=4; }
void sub_0001ED50(void) { states++;g_esp+=4; }
void sub_0001F090(void) {
    assert(draws<STRIPS);
    memcpy(output.words[draws++],(void *)(g_xbox_mem_offset+g_esp+4),36);
    g_esp+=4;
}
/* GENERATED_ROUTINES */
static void call(void (*function)(void)) {
    g_esp=STACK;MEM32(STACK)=0x12345678;
    g_esi=0xdead;g_edi=0xbeef;g_ebx=0xfeed;g_ebp=0xabcd;
    function();
    assert(g_esp==STACK+4 && !g_fp_top);
    assert(g_esi==0xdead && g_edi==0xbeef && g_ebx==0xfeed && g_ebp==0xabcd);
}
static uint32_t region(unsigned view) { return 0x73ae08+view*0x1d8; }
static void setup(void) {
    MEM32(0x7de318)=pc_native_visual_store(0x670e8,0);
    MEM32(0x4e8cec)=60;MEM32(0x7b0cc0)=PLAYER;MEM32(PLAYER+0x214)=0;
    MEM32(0x7e71c8)=VIEW;MEM32(VIEW+0x20)=448;
    for(unsigned view=0;view<4;view++) {
        uint32_t p=region(view);
        MEM32(p+0x64)=0;MEM32(p+0x68)=640;
        MEM32(p+0x6c)=0;MEM32(p+0x70)=448;
    }
}
static Snapshot step(unsigned tick,unsigned whole,float delta,float exact) {
    MEM32(0x7de318)=tick;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    exact_time=exact;draws=states=reads=0;memset(&output,0,sizeof output);
    call(sub_000F9B10);
    uint32_t p=region(MEM32(PLAYER+0x214));
    unsigned left=MEM32(p+0x64),right=MEM32(p+0x68),top=MEM32(p+0x6c),bottom=MEM32(p+0x70);
    assert(reads==1 && draws==(bottom-top+1)/2 && states==5);
    assert(MEM32(0x7de318)==tick && MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    for(unsigned i=0;i<draws;i++) {
        const uint32_t *w=output.words[i];
        unsigned y=top+i*2;
        assert(w[1]==y && w[3]==y+2 && w[2]-w[0]==right-left);
        assert(w[4]==left*16 && w[5]==(y+1)*16 && w[6]==right*16);
        assert(w[7]==(y+3)*16 && w[8]==0x7f7f7f7f);
    }
    return output;
}
int main(void) {
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
    /* ORIGINAL_CONSTANTS */
    setvbuf(stdout,NULL,_IONBF,0);call(sub_00153C20);
    assert(fabsf(MEMF(0x7a38a0+512*4)-1)<.00001f);
    Snapshot *reference=calloc(481,sizeof *reference);assert(reference);
    probe=0;g_tsfp_native_timing_active=0;setup();
    for(unsigned t=0;t<=480;t++)reference[t]=step(t,1,1,(float)t);
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned slow=1;slow<=4;slow*=4)for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) {
        unsigned n=rates[r]*slow*8,repeats[3]={0},rounding_frames=0;
        for(probe=0;probe<=2;probe++) {
            g_tsfp_native_timing_active=1;setup();unsigned previous=0;
            Snapshot last=step(0,0,0,0);
            for(unsigned f=1;f<=n;f++) {
                unsigned tick=f*60/(rates[r]*slow),whole=tick-previous;
                float exact=(float)((double)f*60/(rates[r]*slow));
                float delta=60.f/(rates[r]*slow);
                if(probe==2) {
                    MEM32(0x7de318)=previous;MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
                    MEM32(0x7de318)=pc_native_visual_store(0x69265,tick);
                }
                Snapshot current=step(tick,whole,delta,exact);
                if(probe==2) {
                    probe=1;Snapshot expected=step(tick,whole,delta,exact);probe=2;
                    if(memcmp(&current,&expected,sizeof current)) {
                        float actual=pc_native_visual_sample(tick);
                        printf("strip rounding fps=%u slow=%u frame=%u exact=%.9g actual=%.9g\n",
                               rates[r],slow,f,exact,actual);
                        assert(fabsf(actual-exact)<.0001f);rounding_frames++;
                        /* Accumulated float deltas can cross a sine-table or
                         * integer-pixel boundary before exact rational time. */
                        for(unsigned row=0;row<STRIPS;row++) {
                            int shift=(int32_t)current.words[row][0]-(int32_t)expected.words[row][0];
                            assert(abs(shift)<=1);
                            assert((int32_t)current.words[row][2]-(int32_t)expected.words[row][2]==shift);
                            for(unsigned j=1;j<9;j++)if(j!=2)
                                assert(current.words[row][j]==expected.words[row][j]);
                        }
                    }
                }
                if(!memcmp(&current,&last,sizeof current))repeats[probe]++;
                if(!probe || f*60%(rates[r]*slow)==0)
                    assert(!memcmp(&current,&reference[tick],sizeof current));
                Snapshot paused=step(tick,0,0,exact);
                assert(!memcmp(&current,&paused,sizeof current));
                last=current;previous=tick;
            }
        }
        printf("strip fps=%u slow=%u frames=%u repeated_original=%u repeated_probe=%u repeated_fixed=%u rounding_frames=%u\n",
               rates[r],slow,n,repeats[0],repeats[1],repeats[2],rounding_frames);
        assert(repeats[0]>=n-480);
        if(rates[r]*slow>60)assert(repeats[1]<repeats[0]);
        assert(repeats[2]==repeats[1]);
    }
    /* Render views sample the same clock without advancing it. */
    probe=2;setup();MEM32(0x7de318)=pc_native_visual_store(0x670e8,12);
    MEM32(0x4e7944)=0;MEMF(0x4e794c)=.25f;
    MEM32(0x7de318)=pc_native_visual_store(0x688bb,12);
    Snapshot first=step(12,0,.25f,12.25f);
    for(unsigned view=1;view<4;view++) {
        MEM32(PLAYER+0x214)=view;
        Snapshot next=step(12,0,.25f,12.25f);
        assert(!memcmp(&first,&next,sizeof first));
    }
    const unsigned rebases[]={333,12,0,480,12,12,0};
    for(unsigned i=0;i<sizeof rebases/sizeof *rebases;i++) {
        unsigned t=rebases[i];MEM32(0x7de318)=pc_native_visual_store(0x357939,t);
        Snapshot next=step(t,0,0,(float)t);
        assert(!memcmp(&next,&reference[t],sizeof next));
    }
    /* A clipped player viewport retains its own rows and screen coordinates.
     * Check odd and even heights and preserve original integer output. */
    for(unsigned view=0;view<4;view++)for(unsigned odd=0;odd<2;odd++) {
        setup();MEM32(PLAYER+0x214)=view;uint32_t p=region(view);
        MEM32(p+0x64)=32;MEM32(p+0x68)=320;
        MEM32(p+0x6c)=40;MEM32(p+0x70)=280+odd;MEM32(VIEW+0x20)=224;
        MEM32(0x7de318)=pc_native_visual_store(0x670e8,12);
        probe=0;Snapshot original=step(12,1,1,12);
        probe=2;Snapshot native=step(12,1,1,12);
        assert(!memcmp(&original,&native,sizeof native));
        MEM32(0x4e7944)=0;MEMF(0x4e794c)=.25f;
        MEM32(0x7de318)=pc_native_visual_store(0x688bb,12);
        native=step(12,0,.25f,12.25f);
        probe=1;Snapshot expected=step(12,0,.25f,12.25f);
        assert(!memcmp(&expected,&native,sizeof native));
        g_tsfp_native_timing_active=0;probe=2;
        Snapshot disabled=step(12,0,.25f,12.25f);
        assert(!memcmp(&disabled,&original,sizeof disabled));
        g_tsfp_native_timing_active=1;
    }
    assert(production_numeric(0xf9b4c,0x7de318,12)==12);
    assert(production_numeric(0xf9b4b,0x7de31c,12)==12);
    /* Empty X and Y ranges retain original early-outs. */
    for(probe=0;probe<=2;probe++) {
        setup();MEM32(region(0)+0x68)=0;reads=draws=states=0;
        call(sub_000F9B10);assert(!reads && !draws && !states);
        setup();MEM32(region(0)+0x70)=0;reads=draws=states=0;
        call(sub_000F9B10);assert(reads==1 && !draws && states==5);
    }
    puts("PASS: strip distortion fractional cadence, original tick-boundary packets, pause, rebases and four-view sampling");
    free(reference);free(memory);return 0;
}
