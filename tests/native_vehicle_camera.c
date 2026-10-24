/* Full original vehicle chase-camera update and spring/angle helpers.
 * World collision queries report unobstructed space. CRT floor/asin/acos
 * use host math. History windows and moving player targets are controlled;
 * this is not terrain or physical-input acceptance. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp;

enum { CAMERA=0x1000,PLAYER=0x6000,PROP=0x8000,DATA=0x9000,STACK=0x18000 };
void sub_003C8501(void) {g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=floor(MEMD(g_esp+4));g_esp+=4;}
void sub_003C9DF8(void) {g_fp_stack[g_fp_top]=asin(g_fp_stack[g_fp_top]);g_esp+=4;}
void sub_003C9CC4(void) {g_fp_stack[g_fp_top]=acos(g_fp_stack[g_fp_top]);g_esp+=4;}
void sub_001665C0(void) {g_esp+=4;}
void sub_00088CA0(void) {g_eax=0;g_esp+=4;}
void sub_00088430(void) {g_eax=0;g_esp+=4;}
void sub_000888A0(void) {g_eax=0;g_esp+=4;}
static int legacy_gate,legacy_count;
static unsigned advanced;
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((((site)==0xCA040u && legacy_gate) || ((site)==0xCA424u && legacy_count)) ? \
      (value) : pc_native_global_load((site),(address),(value)))
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) do {if((address)==0x165650u)++advanced;} while(0)
/* UNREACHED */
/* ROUTINES */
static void reset(void) {
    memset((void *)(g_xbox_mem_offset+CAMERA),0,0x10000);
    MEM32(0x7b0cc0)=CAMERA;MEM32(0x7b0c7c)=PLAYER;MEM32(0x4e8cec)=60;
    MEM32(CAMERA+0x218)=24;MEM32(PLAYER+0x14)=PROP;MEM32(PROP+0x7c)=DATA;
    MEMF(CAMERA+0xfec)=5;MEMF(CAMERA+0xff0)=2;MEMF(CAMERA+0xff4)=100;
    MEMF(CAMERA+0x27c)=0;MEMF(CAMERA+0x280)=2;MEMF(CAMERA+0x284)=-5;
}

static void step(float dt,unsigned whole) {
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=dt;MEMF(0x4e7948)=dt;
    /* Original physics publishes whole-step history only when it runs;
     * native physics publishes each fractional interval. Supply steady
     * windows for that original averaging routine, not a second clock. */
    for(unsigned p=0x74c414;p<0x74c450;p+=4)
        MEMF(p)=g_tsfp_native_timing_active?dt:(float)whole;
    /* A missing count correction would underflow the original do/while.
     * Fail before entering that loop, rather than hanging a negative test. */
    if(g_tsfp_native_timing_active && dt>0)
        assert(RECOMP_LOAD_GLOBAL_U32(0xca424u,0x4e7944u,whole)>0);
    g_esp=STACK;MEM32(STACK)=0x12345678;g_fp_top=0;
    g_ebx=0x1234;g_esi=0x5678;g_edi=0x9abc;g_ebp=g_seh_ebp=0xdef0;
    unsigned before=advanced;
    sub_000CA040();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0x1234 && g_esi==0x5678 && g_edi==0x9abc && g_ebp==0xdef0);
    if(g_tsfp_native_timing_active)assert(advanced==before+(dt>0));
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==dt);
    for(unsigned i=0;i<3;i++)assert(isfinite(MEMF(CAMERA+0x27c+4*i)));
}
int main(int argc,char **argv) {
    legacy_gate=argc>1 && !strcmp(argv[1],"legacy-camera-gate");
    legacy_count=argc>1 && !strcmp(argv[1],"legacy-camera-count");
    unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    const unsigned rates[]={60,60,120,144,120};
    const float speeds[]={.25f,1.f,3.f};
    const float jitter[]={.2f,.8f,.4f,.6f};
    float reference[6][3],worst=0;
    for(unsigned speed=0;speed<3;speed++)for(unsigned axis=0;axis<2;axis++)for(unsigned mode=0;mode<5;mode++) {
      unsigned fps=rates[mode];reset();advanced=0;g_tsfp_native_timing_active=mode!=0;
      printf("camera fps=%u mode=%u speed=%.2f axis=%u\n",fps,mode,speeds[speed],axis);fflush(stdout);
      double ticks=0;unsigned count=0;
      for(unsigned stage=0;stage<6;stage++) {
        for(unsigned f=0;f<fps*2;f++) {
          float dt=(mode==4?jitter[count%4]:60.f/fps)*speeds[speed];
          double next=ticks+dt;unsigned whole=(unsigned)floor(next+1e-5)-(unsigned)floor(ticks+1e-5);ticks=next;++count;
          float t=(float)(stage*2.0+(f+1.0)/fps);
          /* Forward then reverse: original target position and orientation
           * machinery runs; only world collision is held unobstructed. */
          MEMF(PLAYER+(axis?0xb4:0xbc))=10*speeds[speed]*(t<=6.f?t:12-t);
          float before=MEMF(CAMERA+0x27c+4*(axis?0:2));
          step(dt,whole);
          if(mode && f>2 && stage<2)assert(MEMF(CAMERA+0x27c+4*(axis?0:2))!=before);
        }
        for(unsigned a=0;a<3;a++) {
          float value=MEMF(CAMERA+0x27c+4*a);
          if(!mode)reference[stage][a]=value;
          else {float error=fabsf(value-reference[stage][a]);if(error>worst)worst=error;
            /* One Original60 target-sample travel plus 0.03 integration margin. */
            float tolerance=(10.f/60.f+.03f)*fmaxf(1.f,speeds[speed]);
            if(error>=tolerance)fprintf(stderr,"stage=%u axis=%u value=%f ref=%f error=%f\n",stage,a,value,reference[stage][a],error);
            assert(error<tolerance);}
        }
      }
      unsigned char paused[0x1000];memcpy(paused,memory+CAMERA,sizeof paused);
      step(0,mode?3:0);assert(!memcmp(paused,memory+CAMERA,sizeof paused));
    }
    g_tsfp_native_timing_active=0;
    assert(pc_native_global_load(0xca040,0x4e7944,3)==3);
    assert(pc_native_global_load(0xca424,0x4e7944,0)==0);
    g_tsfp_native_timing_active=1;MEMF(0x4e794c)=3;
    assert(pc_native_global_load(0xca424,0x4e7944,3)==3);
    assert(pc_native_global_load(0xca425,0x4e7944,0)==0);
    printf("PASS: original vehicle camera, directions/reversal, fresh fractional steps, slow/fast/irregular, pause and ABI; max error %.6f\n",worst);
    free(memory);return 0;
}
