/* Original contact relative velocity, including rotating rigid bodies and a
 * character whose displacement is supplied by the movement service. */
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
enum { BODY=0x5000, OTHER_BODY=0x5200, PROP=0x6000, OTHER=0x6200,
       CHARACTER=0x6400, CONTACT=0x7000, OUTPUT=0x8000, STACK=0x18000 };
static int legacy_contact;
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) ((void)0)
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((site)==0x166899u && legacy_contact ? (value) : pc_native_global_load(site,address,value))
/* UNREACHED */
/* ROUTINES */
static void reset(void)
{
    memset((void *)(g_xbox_mem_offset+BODY),0,0x4000);
    MEM32(0x74c458)=BODY;MEM32(0x4e8cec)=60;
    MEM32(CONTACT+0x24)=PROP;MEM32(PROP+0x80)=0;
    MEM32(CONTACT+0x28)=OTHER;MEM32(OTHER+0x7c)=CHARACTER;
    MEM32(OTHER+0x80)=1;
    const float p[]={10,20,30},lever[]={2,3,-1},v[]={6,-4,2},w[]={.5f,-.25f,.75f};
    for(unsigned i=0;i<3;i++) {
        MEMF(BODY+0x140+i*4)=MEMF(OTHER_BODY+0x140+i*4)=p[i];
        MEMF(CONTACT+i*4)=p[i]+lever[i];
        MEMF(BODY+0x14c+i*4)=v[i];MEMF(BODY+0x134+i*4)=w[i];
    }
    MEMF(OTHER_BODY+0x14c)=1;MEMF(OTHER_BODY+0x154)=-3;
    MEMF(CONTACT+0xc)=.6f;MEMF(CONTACT+0x14)=.8f;
}
static void check(unsigned type,unsigned whole,float delta,int direction,int character_moves)
{
    MEM16(CONTACT+0x2e)=type;
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    const float actor[]={3.f*direction,-2.f*direction,1.f*direction};
    MEMF(CHARACTER+0x3c)=actor[0]*delta/60;
    MEMF(OTHER+0xfc)=actor[1]*delta/60;
    MEMF(CHARACTER+0x40)=actor[2]*delta/60;
    float expected[]={4,-2,4}; /* v + angular_velocity cross contact_offset */
    if(type==3 && character_moves)for(unsigned i=0;i<3;i++)expected[i]-=actor[i];
    if(type==1) {expected[0]-=1;expected[2]+=3;}
    for(unsigned null_output=0;null_output<2;null_output++) {
        for(unsigned i=0;i<3;i++)MEMF(OUTPUT+i*4)=12345;
        g_esp=STACK;MEM32(STACK)=0x12345678;
        g_esi=null_output?0:OUTPUT;g_edx=CONTACT;
        g_edi=0xcdef;g_ebx=0xabcd;g_ebp=0xdef0;
        sub_001667C0();
        assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678);
        assert(g_esi==(null_output?0:OUTPUT) && g_edi==0xcdef && g_ebx==0xabcd && g_ebp==0xdef0);
        assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
        assert(fabsf(g_xmm0.f[0]-(.6f*expected[0]+.8f*expected[2]))<.00001f);
        for(unsigned i=0;i<3;i++)assert(fabsf(MEMF(OUTPUT+i*4)-(null_output?12345:expected[i]))<.00001f);
    }
}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);
    legacy_contact=argc>1 && !strcmp(argv[1],"legacy-contact-velocity");
    void *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    for(unsigned rate=0;rate<3;rate++)for(unsigned mode=0;mode<3;mode++) {
        unsigned fps=rate==0?60:rate==1?120:144;
        unsigned frames=fps*(mode==1?4:1),multiplier=mode==2?3:1;
        g_tsfp_native_timing_active=rate!=0;reset();
        printf("contact_velocity fps=%u mode=%u\n",fps,mode);
        for(unsigned frame=0;frame<frames;frame++) {
            unsigned whole=(frame+1)*60*multiplier/frames-frame*60*multiplier/frames;
            float delta=g_tsfp_native_timing_active?60.f*multiplier/frames:(float)whole;
            for(unsigned type=1;type<=3;type++)for(int direction=-1;direction<=1;direction++)
                check(type,whole,delta,direction,delta>0);
        }
        for(unsigned type=1;type<=3;type++) {
            check(type,0,0,1,0);
            if(g_tsfp_native_timing_active)check(type,1,0,1,0);
        }
    }
    reset();g_tsfp_native_timing_active=0;check(3,0,.5f,1,0);
    g_tsfp_native_timing_active=1;
    const float irregular[]={.125f,0,.5f,1.5f,3.f,.375f};
    float elapsed=0;
    for(unsigned i=0;i<6;i++) {
        float delta=irregular[i];unsigned whole=(unsigned)(elapsed+delta)-(unsigned)elapsed;
        check(3,whole,delta,1,delta>0);elapsed+=delta;
    }
    free(memory);puts("PASS: original contact relative velocity, rotating bodies, character movement, pause and ABI");
    return 0;
}
