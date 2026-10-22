/* Original clock -> solver -> body integrator -> original quaternion/matrix
 * helpers and a controlled support contact. Broad phase provides controlled
 * geometry; material, allocation and contact notifications are services.
 * This checks real body state, not live vehicle handling or level geometry. */
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
/* PHYSICS POLICY */
extern void sub_00171560(void),sub_00153B40(void);
void pc_native_physics_solver(void) {pc_native_physics_solver_guest(sub_00171560);}
void pc_native_physics_random(void) {pc_native_physics_random_guest(sub_00153B40);}
enum { BODY=0x5000, TABLE=0x6000, MATERIAL=0x7000, FORCE=0x8000,
       CONTACT=0x9000, PROP=0x9100, STATIC_PROP=0x9300, IMPACTS=0x9500 };
static int legacy,contact_mode,missing_contact,legacy_rng,legacy_secondary;
static unsigned allocations,integrations,changed,contacts,impulses;
static unsigned random_calls;
static double seconds;
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) do { if((address)==0x16ca00u) { \
    ++integrations;seconds+=MEMF(g_esp+8);assert(MEM32(g_esp+4)==BODY); } \
    if((address)==0x153b40u)++random_calls; } while(0)
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    (legacy && ((site)==0x171fb9 || (site)==0x17208e || (site)==0x172119) ? \
     (value) : pc_native_global_load(site,address,value))
#undef RECOMP_F32_MUL_AT
#define RECOMP_F32_MUL_AT(site,left,right) \
    ((site)==0x172099 && legacy ? (left)*(right) : pc_native_multiply(site,left,right))
/* The other ADDSS sites in these lifted routines have identity policy. */
#undef RECOMP_F32_ADD_AT
#define RECOMP_F32_ADD_AT(site,left,right) \
    ((site)==0x17164Fu && !legacy_secondary ? pc_native_physics_elapsed(left,right) : (left)+(right))
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) assert(!"Unexpected callback")
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected tail")
/* Mutations omit contact resolution or route shuffling back to the shared
 * RNG, leaving the clock and remaining generated instructions intact. */
static void drop_contact(void) {g_esp+=4;}
#undef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site,function) \
    ((site)==0x17162du && missing_contact ? drop_contact : \
    (legacy_rng && ((site)==0x1715A0u || (site)==0x170E51u)) ? (function) : \
    PC_NATIVE_PHYSICS_CALLEE(site,function))
void sub_0003DFA0(void) {g_eax=allocations++%2?0x3000:0x2000;g_esp+=4;}
void sub_0003D6E0(void) {g_eax=0x4000;g_esp+=4;}
void sub_0003D6B0(void) {g_esp+=4;}
void sub_00170500(void) {
    MEM32(g_edi)=contact_mode;
    if(contact_mode) {
        for(unsigned i=0;i<3;i++)MEMF(CONTACT+i*4)=MEMF(BODY+0x140+i*4);
        MEMF(CONTACT+0x10)=1; /* zero-depth, upward normal at body centre */
        MEM32(CONTACT+0x24)=PROP;MEM32(CONTACT+0x28)=STATIC_PROP;
        MEM16(CONTACT+0x2e)=2;MEM32(STATIC_PROP+0x80)=0xffffffff;
        for(int i=1;i<contact_mode;i++)
            memcpy((void *)(g_xbox_mem_offset+CONTACT+i*0x40),
                   (void *)(g_xbox_mem_offset+CONTACT),0x40);
    }
    g_esp+=4;
}
void sub_0016CCD0(void) {g_esp+=4;}
void sub_00170690(void) {
    for(int i=0;i<contact_mode;i++)MEM32(0x4000+i*4)=CONTACT+i*0x40;
    g_eax=contact_mode;g_esp+=4;
}
void sub_0016B6B0(void) {g_esp+=4;}
void sub_0016B050(void) {g_esp+=4;}
void sub_0016C0D0(void) {g_esp+=4;}
void sub_001719E0(void) {g_esp+=4;}
void sub_00167880(void) {assert(MEM32(g_esp+4)==0);g_esp+=4;}
void sub_002561B0(void) {g_eax=MATERIAL;g_esp+=4;}
void sub_0016E780(void) {
    uint32_t contact=MEM32(g_esp+4);
    assert(contact>=CONTACT && contact<CONTACT+contact_mode*0x40 &&
           (contact-CONTACT)%0x40==0);assert(isfinite(MEMF(g_esp+16)));
    contacts++;if(fabsf(MEMF(g_esp+16))>1e-6f)impulses++;
    g_esp+=4;
}
/* UNREACHED */
/* ROUTINES */
static void reset(int forced)
{
    memset((void *)(g_xbox_mem_offset+BODY),0,0x5000);
    contact_mode=forced>=2 ? (forced==3?3:1) : 0;
    MEM32(0x7497c8)=0;MEM32(0x4e7950)=1;MEM32(0x4e7954)=0;
    MEM32(0x74c458)=BODY;MEM32(0x74c46c)=TABLE;MEM32(0x7a20c8)=BODY;
    MEM32(0x74c48c)=IMPACTS;
    MEM32(0x74c450)=0;MEM16(0x74c454)=0;MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<15;i++)MEMF(0x74c414+4*i)=1;
    for(unsigned matrix=0;matrix<4;matrix++)
        for(unsigned i=0;i<16;i++)MEMF(BODY+matrix*64+i*4)=i%5==0?1:0;
    MEMF(BODY+0x10c)=1; /* normalized quaternion */
    MEMF(BODY+0x1a8)=.5f;MEM32(BODY+0x1c0)=0x40000; /* external gravity disabled */
    MEM32(BODY+0x1b8)=FORCE;
    MEMF(BODY+0x14c)=6;MEMF(BODY+0x150)=-4;MEMF(BODY+0x154)=2;
    MEMF(BODY+0x12c)=MEMF(BODY+0x138)=.5f;
    if(forced==1)MEMF(FORCE)=12;
    if(contact_mode) {
        MEMF(FORCE+4)=-20;
        MEMF(BODY+0x14c)=MEMF(BODY+0x154)=0;
        MEMF(BODY+0x12c)=MEMF(BODY+0x138)=0;
    }
    for(unsigned i=0;i<6;i++)MEMF(BODY+0x110+i*4)=MEMF(FORCE+i*4);
    integrations=changed=contacts=impulses=random_calls=0;seconds=0;
}
static void step(unsigned whole,float delta)
{
    unsigned position=BODY+(contact_mode?0x144:0x140);
    float before=MEMF(position);
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    g_esp=0x18000;MEM32(g_esp)=0x12345678;
    g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=0xdef0;
    pc_native_physics_guest(sub_00171FB0);
    assert(g_esp==0x18004 && MEM32(0x18000)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    if(MEMF(position)!=before)changed++;
}
int main(int argc,char **argv)
{
    legacy=argc>1 && !strcmp(argv[1],"legacy-clock");
    legacy_rng=argc>1 && !strcmp(argv[1],"legacy-rng");
    legacy_secondary=argc>1 && !strcmp(argv[1],"legacy-secondary");
    missing_contact=argc>1 && !strcmp(argv[1],"missing-contact");setbuf(stdout,NULL);
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    for(unsigned forced=0;forced<4;forced++) {
        float reference_position[3]={0},reference_velocity[3]={0},reference_quaternion[4]={0};
        uint32_t reference_seed[2]={0};
        for(unsigned rate=0;rate<3;rate++) {
            unsigned fps=rate==0?60:rate==1?120:144;
            g_tsfp_native_timing_active=rate!=0;reset(forced);
            for(unsigned frame=0;frame<fps;frame++)step((frame+1)*60/fps-frame*60/fps,60.f/fps);
            printf("rigid_motion fps=%u forced=%u fresh=%u integrations=%u seconds=%.9f x=%.9f vx=%.9f qy=%.9f qw=%.9f y=%.9f vy=%.9f contacts=%u impulses=%u\n",
                fps,forced,changed,integrations,seconds,MEMF(BODY+0x140),MEMF(BODY+0x14c),MEMF(BODY+0x104),MEMF(BODY+0x10c),MEMF(BODY+0x144),MEMF(BODY+0x150),contacts,impulses);
            printf("shared_rng fps=%u draws=%u seed=%08x:%08x\n",fps,
                MEM32(0x7497c8),MEM32(0x4e7954),MEM32(0x4e7950));
            assert(MEM32(0x7497c8)==(contact_mode>1 ? 60*8*(contact_mode-1) : 0));
            assert(random_calls==(contact_mode>1 ? fps*8*(contact_mode-1) : 0));
            for(unsigned i=0;i<2;i++) {
                uint32_t seed=MEM32(0x4e7950+i*4);
                if(rate==0)reference_seed[i]=seed;
                assert(seed==reference_seed[i]);
            }
            /* The original contact path uses eight secondary integrations.
             * A body held at rest need not change position after impact. */
            assert((contact_mode ? changed>=1 : changed==fps) &&
                   integrations==fps*(contact_mode?8:1) && fabs(seconds-1.)<1e-6);
            if(contact_mode) {
                assert(contacts>=fps && impulses>=fps);
                assert(fabsf(MEMF(BODY+0x144))<.04f && fabsf(MEMF(BODY+0x150))<.001f);
            } else {
                /* Constant half-radian/second rotation for one second. */
                assert(fabs(MEMF(BODY+0x104)-sin(.25))<2e-6);
                assert(fabs(MEMF(BODY+0x10c)-cos(.25))<2e-6);
            }
            double norm=0,dot=0;
            for(unsigned i=0;i<4;i++) {
                float q=MEMF(BODY+0x100+4*i);assert(isfinite(q));norm+=(double)q*q;
                if(rate==0)reference_quaternion[i]=q;
                dot+=(double)q*reference_quaternion[i];
            }
            assert(fabs(norm-1)<1e-5 && fabs(dot)>0.99999);
            for(unsigned axis=0;axis<3;axis++) {
                float position=MEMF(BODY+0x140+axis*4),velocity=MEMF(BODY+0x14c+axis*4);
                assert(isfinite(position) && isfinite(velocity));
                if(rate==0) {reference_position[axis]=position;reference_velocity[axis]=velocity;}
                /* Forward-Euler motion may differ by less than one original
                 * frame's travel; a lost half-second cannot pass this bound. */
                assert(fabsf(position-reference_position[axis])<.2f);
                assert(fabsf(velocity-reference_velocity[axis])<.02f);
            }
            uint32_t count=MEM32(0x7497c8);
            unsigned char before[0x200];memcpy(before,memory+BODY,sizeof before);
            step(0,0);assert(!memcmp(before,memory+BODY,sizeof before));
            assert(MEM32(0x7497c8)==count && pc_physics_scope==NULL);
        }
    }
    /* Slow/fast game time and variable frame durations still commit only
     * carried authored ticks, even when ceil(delta) adds primary solves. */
    for(unsigned mode=0;mode<3;mode++) {
        g_tsfp_native_timing_active=1;reset(3);
        const float irregular[]={.25f,.5f,.75f,1.25f,2.5f,.05f,3.f};
        double ticks=0;unsigned whole_total=0,primary=0;
        unsigned frames=mode==0?576:mode==1?144:70;
        for(unsigned frame=0;frame<frames;frame++) {
            float delta=mode==0?60.f/(144.f*4.f):mode==1?180.f/144.f:irregular[frame%7];
            ticks+=delta;
            unsigned total=mode==0?(frame+1)*60/576:mode==1?(frame+1)*180/144:(unsigned)ticks;
            step(total-whole_total,delta);whole_total=total;
            primary+=(unsigned)ceilf(delta);
        }
        uint64_t expected_seed=1;
        for(unsigned i=0;i<whole_total*16;i++)expected_seed=expected_seed*214013u+2531011u;
        printf("rigid_mixed mode=%u seconds=%.9f whole=%u primary=%u actual_rng=%u shared_rng=%u\n",
            mode,seconds,whole_total,primary,random_calls,MEM32(0x7497c8));
        assert(fabs(seconds-ticks/60)<1e-5 && integrations==primary*8);
        assert(random_calls==primary*16 && MEM32(0x7497c8)==whole_total*16);
        assert(MEM32(0x4e7950)==(uint32_t)expected_seed && MEM32(0x4e7954)==(uint32_t)(expected_seed>>32));
        assert(fabsf(MEMF(BODY+0x150))<.001f && pc_physics_scope==NULL);
    }
    free(memory);puts("PASS: original rigid motion, force, drag, quaternion/matrix pipeline, support contact, fresh steps and pause");return 0;
}
