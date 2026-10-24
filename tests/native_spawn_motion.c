/* Original model-joint history -> object creation -> inherited motion.
 * Allocation, model orientation extraction, owner transform and wake/bounds
 * are controlled services. Original spawn and velocity/momentum setters run.
 * Each case is one independent spawn event, not repeated per-frame spawning. */
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
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { BODY=0x5000, MODEL=0x6000, OWNER=0x6200, META=0x6600,
       MAP=0x6800, MATRICES=0x7000, HISTORY=0x8000, OBJECT=0x9000,
       OBJECT_MODEL=0x9400, OBJECT_META=0x9600, VERTEX=0x9800, STACK=0x18000 };
static int legacy,allocation_fails,rotated;
static unsigned creations,cleanups,wakes;
static float spawn_position[3];
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) ((void)0)
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((site)==0x172ab5u && legacy ? (value) : pc_native_global_load(site,address,value))
static void identity(uint32_t matrix)
{
    for(unsigned i=0;i<16;i++)MEMF(matrix+i*4)=i%5==0?1.f:0.f;
}
void sub_001528A0(void)
{
    /* Controlled neutral Euler extraction; orientation math is out of scope. */
    MEMF(MEM32(g_esp+8))=MEMF(0x475c90);
    MEMF(MEM32(g_esp+12))=MEMF(0x475ce4);
    MEMF(MEM32(g_esp+16))=0;
    g_esp+=4;
}
void sub_000A14B0(void)
{
    uint32_t m=MEM32(g_esp+4);identity(m);
    if(rotated) {MEMF(m)=MEMF(m+20)=0;MEMF(m+4)=1;MEMF(m+16)=-1;}
    g_esp+=4;
}
void sub_003C8501(void)
{
    g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=floor(MEMD(g_esp+4));g_esp+=4;
}
void sub_000A6F90(void)
{
    ++creations;assert(MEM32(g_esp+4)==0x751);
    for(unsigned i=0;i<3;i++)spawn_position[i]=MEMF(MEM32(g_esp+8)+i*4);
    g_eax=OBJECT;g_esp+=4;
}
void sub_0016C170(void) {assert(MEM32(g_esp+4)==OBJECT);g_eax=allocation_fails?0:1;g_esp+=4;}
void sub_000A9070(void) {assert(MEM32(g_esp+4)==OBJECT);++cleanups;g_esp+=4;}
void sub_00172180(void)
{
    assert(MEM32(g_esp+4)==1 && MEM32(g_esp+8)==OBJECT);
    identity(BODY);identity(BODY+0xc0);
    for(unsigned i=0;i<3;i++)MEMF(BODY+0x140+i*4)=spawn_position[i];
    g_esp+=4;
}
void sub_00165A40(void) {assert(MEM32(g_esp+4)==1);g_esp+=4;}
void sub_00167880(void) {assert(MEM32(g_esp+4)==1);++wakes;g_esp+=4;}
/* UNREACHED */
/* ROUTINES */
static void check(unsigned whole,float delta,float scale,int history,int vertex,int fails)
{
    memset((void *)(g_xbox_mem_offset+BODY),0,0x5000);
    MEM32(0x74c458)=BODY-512;MEM32(0x4e8cec)=60;
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    MEM32(MODEL)=OWNER;MEM32(MODEL+4)=META;MEM32(META+4)=1;
    MEM32(OWNER+0x28)=0x200008;MEM32(MODEL+0x10)=MATRICES;
    MEM32(MODEL+0x8c)=MAP;MEM32(MAP)=MAP+4;MEM32(MAP+8)=2;
    MEM32(MODEL+0x98)=HISTORY;MEM32(HISTORY+0xf0)=2;
    MEM32(OBJECT+4)=OBJECT_MODEL;MEM32(OBJECT_MODEL+4)=OBJECT_META;
    MEM32(OBJECT_META+0x78)=VERTEX;MEMF(VERTEX)=1;MEMF(VERTEX+4)=-2;MEMF(VERTEX+8)=.5f;
    uint32_t joint=MATRICES+128;identity(joint);
    const float velocity[]={3,-2,5};
    for(unsigned i=0;i<3;i++)MEMF(joint+0x30+i*4)=(float)(i+1);
    g_esp=STACK;MEM32(STACK+4)=MODEL;sub_00057040();assert(g_esp==STACK+4);
    for(unsigned i=0;i<3;i++) {
        assert(MEMF(HISTORY+12+i*4)==(float)(i+1));
        MEMF(joint+0x30+i*4)+=velocity[i]*delta/60.f;
    }
    if(!history)MEM32(MODEL+0x98)=0;
    creations=cleanups=wakes=0;allocation_fails=fails;
    g_esp=STACK;MEM32(STACK)=0x12345678;
    const uint32_t args[]={MODEL,1,0x751,vertex?0:0xffffffffu,0,0,0,0x11,0x400000,0};
    for(unsigned i=0;i<10;i++)MEM32(STACK+4+i*4)=args[i];
    MEMF(STACK+40)=scale;g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=g_seh_ebp=0xdef0;g_fp_top=0;
    sub_001727A0();
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef);
    assert(g_eax==(fails?0:OBJECT) && creations==1 && cleanups==(unsigned)fails);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    int inherits=!fails && history && scale!=0 && (g_tsfp_native_timing_active?delta>0:whole>0);
    float v[]={velocity[0]*scale,velocity[1]*scale,velocity[2]*scale};
    if(rotated) {float x=v[0];v[0]=-v[1];v[1]=x;}
    const float lever[]={-1,2,-.5f};
    float momentum[]={lever[1]*v[2]-lever[2]*v[1],lever[2]*v[0]-lever[0]*v[2],lever[0]*v[1]-lever[1]*v[0]};
    for(unsigned i=0;i<3;i++) {
        assert(fabsf(MEMF(BODY+0x14c+i*4)-(inherits?v[i]:0))<.0003f);
        assert(fabsf(MEMF(BODY+0x128+i*4)-(inherits && vertex?momentum[i]:0))<.0006f);
    }
    assert(wakes==(fails?0:inherits?3:1));
}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);legacy=argc>1 && !strcmp(argv[1],"legacy-spawn-motion");
    void *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    for(unsigned fps=60;fps<=144;fps=fps==60?120:144) {
        printf("spawn_motion fps=%u\n",fps);g_tsfp_native_timing_active=fps!=60;
        for(rotated=0;rotated<2;rotated++)for(unsigned frame=0;frame<fps;frame++)
            for(unsigned vertex=0;vertex<2;vertex++)for(int sign=-1;sign<=1;sign++) {
                unsigned whole=(frame+1)*60/fps-frame*60/fps;
                check(whole,60.f/fps,(float)sign,1,vertex,0);
            }
        if(fps==144)break;
    }
    for(rotated=0;rotated<2;rotated++) {
        check(0,.125f,3.66f,1,1,0);check(3,3,3.66f,1,1,0);
        check(0,0,1,1,1,0);check(1,0,1,1,1,0);
        check(0,.5f,1,0,1,0);check(0,.5f,1,1,1,1);
    }
    g_tsfp_native_timing_active=0;check(0,.5f,1,1,1,0);
    free(memory);puts("PASS: original joint history, spawn velocity/momentum, pause, missing history, allocation failure and ABI");
    return 0;
}
