static const unsigned test_rates[]={60,120,144,180,240};
/* Original clock -> solver -> body integrator -> quaternion/matrix helpers,
 * vehicle component update, entity publication and sleep/wake, with a
 * controlled support contact. Broadphase, material/allocation, damage,
 * room membership and model notification remain controlled services.
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
       CONTACT=0x9000, PROP=0x9100, STATIC_PROP=0x9300, IMPACTS=0x9500,
       VEHICLE=0xa000, PARAMETERS=0xb000, CAPS=0xc000, CHARACTER=0xd000 };
static int legacy,contact_mode,missing_contact,legacy_rng,legacy_secondary,legacy_sleep;
static unsigned allocations,integrations,changed,contacts,impulses;
static unsigned random_calls;
static unsigned publications,sleeps;
static unsigned vehicle_updates;
static int character_contact,legacy_contact;
static int wall_contact,missing_wall;
static int penetration_contact,missing_penetration;
static unsigned contact_kind;
static float contact_depth,contact_restitution;
static float contact_normal[3],contact_offset[3];
static void impact_callback(void);
static double seconds;
#undef RECOMP_TRACE_ENTER
#define RECOMP_TRACE_ENTER(name,address) do { if((address)==0x16ca00u) { \
    ++integrations;seconds+=MEMF(g_esp+8);assert(MEM32(g_esp+4)==BODY); } \
    if((address)==0x153b40u)++random_calls; \
    if((address)==0x1753a0u)++vehicle_updates; \
    if((address)==0x1677e0u)++sleeps; \
    if((address)==0x16e780u) { ++contacts; \
        if(fabsf(MEMF(g_esp+16))>1e-6f)++impulses; } } while(0)
#undef RECOMP_LOAD_GLOBAL_U32
#define RECOMP_LOAD_GLOBAL_U32(site,address,value) \
    ((legacy && ((site)==0x171fb9 || (site)==0x17208e || (site)==0x172119)) || \
     (legacy_contact && (site)==0x166899) ? \
     (value) : pc_native_global_load(site,address,value))
#undef RECOMP_F32_MUL_AT
#define RECOMP_F32_MUL_AT(site,left,right) \
    ((site)==0x1689ACu && penetration_contact && missing_penetration ? 0.f : \
    (site)==0x172099 && legacy ? (left)*(right) : pc_native_multiply(site,left,right))
/* The other ADDSS sites in these lifted routines have identity policy. */
#undef RECOMP_F32_ADD_AT
#define RECOMP_F32_ADD_AT(site,left,right) \
    ((site)==0x17164Fu && !legacy_secondary ? pc_native_physics_elapsed(left,right) : (left)+(right))
#undef RECOMP_LOAD_U8_AT
#define RECOMP_LOAD_U8_AT(site,value) \
    ((site)==0x171f83u && legacy_sleep ? (value) : pc_native_load_u8(site,value))
#undef RECOMP_ICALL_SAFE_AT
#define RECOMP_ICALL_SAFE_AT(target,stack,site) do { \
    assert((target)==0x7f1234u && (site)==0x16dacau);impact_callback(); } while(0)
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(target) assert(!"Unexpected tail")
/* Mutations omit contact resolution or route shuffling back to the shared
 * RNG, leaving the clock and remaining generated instructions intact. */
static void drop_contact(void) {g_esp+=4;}
#undef RECOMP_DIRECT_CALLEE_AT
#define RECOMP_DIRECT_CALLEE_AT(site,function) \
    ((site)==0x17162du && (missing_contact || (missing_wall && wall_contact)) ? drop_contact : \
    (legacy_rng && ((site)==0x1715A0u || (site)==0x170E51u)) ? (function) : \
    PC_NATIVE_PHYSICS_CALLEE(site,function))
void sub_0003DFA0(void) {g_eax=allocations++%2?0x3000:0x2000;g_esp+=4;}
void sub_0003D6E0(void) {g_eax=0x4000;g_esp+=4;}
void sub_0003D6B0(void) {g_esp+=4;}
void sub_00170500(void) {
    MEM32(g_edi)=contact_mode;
    if(contact_mode) {
        for(unsigned i=0;i<3;i++) {
            MEMF(CONTACT+i*4)=MEMF(BODY+0x140+i*4)+contact_offset[i];
            MEMF(CONTACT+0xc+i*4)=contact_normal[i];
        }
        MEM32(CONTACT+0x24)=PROP;MEM32(CONTACT+0x28)=STATIC_PROP;
        MEM16(CONTACT+0x2e)=contact_kind;MEM32(STATIC_PROP+0x80)=0xffffffff;
        MEMF(CONTACT+0x18)=contact_depth;MEMF(CONTACT+0x20)=contact_restitution;
        if(character_contact) {
            MEM16(CONTACT+0x2e)=3;MEM32(STATIC_PROP+0x7c)=CHARACTER;
            MEMF(STATIC_PROP+0xfc)=2.f*MEMF(0x4e794c)/60.f;
        }
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
void sub_0026A500(void) {assert(MEM32(g_esp+4)==BODY);g_esp+=4;}
void sub_00168180(void) {assert(g_eax==0);g_esp+=4;}
void sub_002572A0(void) {assert(MEM32(g_esp+4)==PROP);publications++;g_esp+=4;}
void sub_000872F0(void) {g_eax=1;g_esp+=4;}
void sub_002561B0(void) {g_eax=MATERIAL;g_esp+=4;}
static void impact_callback(void) {
    uint32_t contact=MEM32(g_esp+8);
    assert(contact>=CONTACT && contact<CONTACT+contact_mode*0x40 &&
           (contact-CONTACT)%0x40==0);assert(isfinite(MEMF(g_esp+24)));
    if(character_contact) {
        /* The original solver and 16E780/16DA90 forward the pre-impulse
         * relative normal speed and vector to the body callback. Impulse math
         * also reads live displacement independently, so body position alone
         * cannot catch a stale notification velocity. */
        unsigned vector=MEM32(g_esp+20);
        assert(fabsf(MEMF(g_esp+16)-(MEMF(BODY+0x150)-2.f))<.00001f);
        for(unsigned i=0;i<3;i++)
            assert(fabsf(MEMF(vector+4*i)-(MEMF(BODY+0x14c+4*i)-(i==1?2.f:0)))<.00001f);
    }
    g_eax=0;g_esp+=4;
}
/* UNREACHED */
/* ROUTINES */
static void reset(int forced)
{
    memset((void *)(g_xbox_mem_offset+BODY),0,0x9000);
    character_contact=wall_contact=penetration_contact=0;
    contact_kind=2;contact_depth=contact_restitution=0;
    for(unsigned i=0;i<3;i++) {contact_normal[i]=i==1?1.f:0.f;contact_offset[i]=0;}
    contact_mode=forced==2?1:forced==3?3:0;
    MEM32(0x7497c8)=0;MEM32(0x4e7950)=1;MEM32(0x4e7954)=0;
    MEM32(0x74c458)=BODY;MEM32(0x74c46c)=TABLE;MEM32(0x7a20c8)=BODY;
    MEM32(TABLE)=PROP;MEM32(PROP+0x34)=1;MEM8(PROP+0x30)=1;
    MEM32(PROP+0x28)=0x40000000; /* room membership supplied by the fixture */
    MEM32(0x74c48c)=IMPACTS;
    MEM32(0x74c450)=0;MEM16(0x74c454)=0;MEM32(0x4e8cec)=60;
    for(unsigned i=0;i<15;i++)MEMF(0x74c414+4*i)=1;
    for(unsigned matrix=0;matrix<4;matrix++)
        for(unsigned i=0;i<16;i++)MEMF(BODY+matrix*64+i*4)=i%5==0?1:0;
    MEMF(BODY+0x10c)=1; /* normalized quaternion */
    MEMF(BODY+0x1a8)=.5f;MEM32(BODY+0x1c0)=0x40004; /* no extra gravity or sleep */
    MEM32(BODY+0x1e8)=0x7f1234; /* observed body impact callback */
    MEM8(BODY+0x1c8)=10;
    MEM32(BODY+0x1b8)=FORCE;
    MEMF(BODY+0x14c)=6;MEMF(BODY+0x150)=-4;MEMF(BODY+0x154)=2;
    MEMF(BODY+0x12c)=MEMF(BODY+0x138)=.5f;
    if(forced>=4) {
        MEM32(PROP+0x20)=0x400000;MEM32(PROP+0x7c)=VEHICLE;
        MEM32(VEHICLE+0x50)=PARAMETERS;MEM32(VEHICLE+0x5c)=1;
        MEMF(PARAMETERS+0x18)=100;MEMF(PARAMETERS+0x1c)=2;
        MEMF(PARAMETERS+0x54)=2;MEM32(PARAMETERS+0x9c)=2;
        for(unsigned i=0;i<2;i++) {
            unsigned state=VEHICLE+0x1c8+i*0x64;
            MEM32(state-0x54)=CAPS+i*4;MEMF(CAPS+i*4)=1;
            MEMF(state)=forced==5?(i?-100:100):(i?-4:5);
            MEMF(state-0x34)=i?.5f:2.f;
            MEMF(state-8)=MEMF(state-4)=i?-2.f:3.f;
        }
        /* Nonzero local centre exercises the original bounds transform and
         * entity-position offset publication while the body rotates. */
        MEMF(BODY+0x170)=.25f;MEMF(BODY+0x174)=-.5f;MEMF(BODY+0x178)=.75f;
    }
    if(forced==1)MEMF(FORCE)=12;
    if(contact_mode) {
        MEMF(FORCE+4)=-20;
        MEMF(BODY+0x14c)=MEMF(BODY+0x154)=0;
        MEMF(BODY+0x12c)=MEMF(BODY+0x138)=0;
    }
    for(unsigned i=0;i<6;i++)MEMF(BODY+0x110+i*4)=MEMF(FORCE+i*4);
    integrations=changed=contacts=impulses=random_calls=publications=sleeps=vehicle_updates=0;seconds=0;
}
static void step_clocks(unsigned raw_whole,unsigned whole,float raw,float delta)
{
    unsigned position=BODY+(contact_mode?0x144:0x140);
    float before=MEMF(position);
    MEM32(0x4e7944)=whole;MEMF(0x4e794c)=delta;
    MEM32(0x4e7940)=raw_whole;MEMF(0x4e7948)=raw;
    /* The controlled force producer precedes the physics clock each frame.
     * The original publisher now clears those accumulators after use. */
    if(delta>0)for(unsigned i=0;i<6;i++)MEMF(BODY+0x110+i*4)=MEMF(FORCE+i*4);
    g_esp=0x18000;MEM32(g_esp)=0x12345678;
    g_ebx=0xabcd;g_esi=0xbcde;g_edi=0xcdef;g_ebp=0xdef0;
    pc_native_physics_guest(sub_00171FB0);
    assert(g_esp==0x18004 && MEM32(0x18000)==0x12345678 && g_fp_top==0);
    assert(g_ebx==0xabcd && g_esi==0xbcde && g_edi==0xcdef);
    assert(MEM32(0x4e7944)==whole && MEMF(0x4e794c)==delta);
    if(delta>0)for(unsigned i=0;i<3;i++) {
        assert(MEMF(PROP+0x64+i*4)==MEMF(BODY+0x140+i*4)-MEMF(PROP+0x1a8+i*4));
        assert(MEMF(PROP+0xb4+i*4)==MEMF(BODY+0x14c+i*4));
        assert(MEMF(BODY+0x110+i*4)==0 && MEMF(BODY+0x11c+i*4)==0);
    }
    if(MEMF(position)!=before)changed++;
}
static void step(unsigned whole,float delta) {step_clocks(whole,whole,delta,delta);}
int main(int argc,char **argv)
{
    legacy=argc>1 && !strcmp(argv[1],"legacy-clock");
    legacy_rng=argc>1 && !strcmp(argv[1],"legacy-rng");
    legacy_secondary=argc>1 && !strcmp(argv[1],"legacy-secondary");
    legacy_sleep=argc>1 && !strcmp(argv[1],"legacy-body-sleep");
    legacy_contact=argc>1 && !strcmp(argv[1],"legacy-character-contact");
    missing_contact=argc>1 && !strcmp(argv[1],"missing-contact");
    missing_wall=argc>1 && !strcmp(argv[1],"missing-wall");
    missing_penetration=argc>1 && !strcmp(argv[1],"missing-penetration");setbuf(stdout,NULL);
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* CONSTANTS */
    for(unsigned forced=0;forced<6;forced++) {
        float reference_position[3]={0},reference_velocity[3]={0},reference_quaternion[4]={0};
        uint32_t reference_seed[2]={0};
        for(unsigned rate=0;rate<sizeof test_rates/sizeof test_rates[0];rate++) {
            unsigned fps=test_rates[rate];
            g_tsfp_native_timing_active=rate!=0;reset(forced);
            printf("rigid_begin fps=%u forced=%u\n",fps,forced);
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
            assert(publications==fps);
            if(forced>=4) {
                assert(vehicle_updates==fps);
                for(unsigned i=0;i<2;i++) {
                    unsigned state=VEHICLE+0x1c8+i*0x64;
                    float speed=MEMF(state),angle=MEMF(state-0x3c);
                    float expected_speed=forced==5?(i?-50.f-2.f/fps:50.f+3.f/fps):(i?-6.f:8.f);
                    float expected_angle=forced==5?(i?-25.f:100.f):(i?-2.5f:13.f);
                    printf("vehicle_component fps=%u case=%u index=%u speed=%.9f angle=%.9f\n",fps,forced,i,speed,angle);
                    assert(fabsf(speed-expected_speed)<.001f && fabsf(angle-expected_angle)<.06f);
                }
            } else assert(vehicle_updates==0);
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
    /* A character moving upward supplies a moving support contact. Use the
     * original relative-velocity, impulse and notification pipeline with
     * gravity. Character displacement itself is controlled. */
    for(unsigned rate=0;rate<sizeof test_rates/sizeof test_rates[0];rate++) {
        unsigned fps=test_rates[rate];
        g_tsfp_native_timing_active=rate!=0;reset(2);character_contact=1;
        MEMF(BODY+0x150)=2;
        printf("character_contact fps=%u\n",fps);
        for(unsigned frame=0;frame<fps;frame++) {
            step((frame+1)*60/fps-frame*60/fps,60.f/fps);
            assert(fabsf(MEMF(BODY+0x150)-2.f)<.001f);
        }
        printf("character_support fps=%u y=%.9f vy=%.9f impulses=%u\n",fps,
               MEMF(BODY+0x144),MEMF(BODY+0x150),impulses);
        assert(fabsf(MEMF(BODY+0x144)-2.f)<.04f);
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
    for(unsigned mode=0;mode<3;mode++)for(unsigned rate=0;rate<sizeof test_rates/sizeof test_rates[0];rate++) {
        unsigned fps=test_rates[rate];
        g_tsfp_native_timing_active=rate!=0;reset(0);
        MEM32(BODY+0x1c0)=0x40000;
        for(unsigned i=0;i<3;i++) {
            MEMF(BODY+0x14c+i*4)=0;MEMF(BODY+0x128+i*4)=0;MEMF(BODY+0x134+i*4)=0;
        }
        unsigned frame=0;
        while(!sleeps && frame<fps) {
            unsigned whole=(frame+1)*60/fps-frame*60/fps;
            unsigned divisor=mode==1?4:1,multiplier=mode==2?3:1;
            unsigned game_whole=(frame+1)*60*multiplier/(fps*divisor)-frame*60*multiplier/(fps*divisor);
            step_clocks(whole,game_whole,60.f/fps,60.f*multiplier/(fps*divisor));frame++;
        }
        printf("rigid_sleep fps=%u mode=%u frame=%u seconds=%.9f count=%u calls=%u\n",fps,mode,frame,(double)frame/fps,MEM8(BODY+0x1c8),sleeps);
        assert(sleeps==1 && frame*6==fps*(mode==1?4:1));
        assert(MEM8(BODY+0x1c8)==255 && MEM32(0x7a20c8)==0);
        unsigned before=integrations;step(1,1);assert(integrations==before);
        /* Original wake reinserts the body once, resets its countdown, and
         * allows motion on the very next fractional update. */
        for(unsigned wake=0;wake<2;wake++) {
            g_esp=0x18000;MEM32(g_esp+4)=0;sub_00167880();
            assert(g_esp==0x18004 && MEM32(0x7a20c8)==BODY &&
                   MEM32(BODY+0x1f8)==0 && MEM32(BODY+0x1fc)==0 && MEM8(BODY+0x1c8)==10);
        }
        MEMF(BODY+0x14c)=6;step(rate?0:1,60.f/fps);
        assert(integrations==before+1 && MEMF(PROP+0x64)>0 && MEM8(BODY+0x1c8)==10);
    }
    /* Identical zero-depth static contact at each preset, independent of live
     * broadphase/approach. A frictionless inelastic impulse has analytical
     * magnitude -v.n / (inverse_mass + |r cross n|^2) for unit inverse inertia.
     * Cover all normal axes/signs and centre/eccentric points. The contact
     * oracle supplies fixed world-space offsets; this is not level geometry. */
    const float inverse_masses[]={.25f,.5f,2.f},closing_speeds[]={4.f,12.f,-4.f,0.f};
    double max_linear_error=0,max_angular_error=0;unsigned wall_cases=0;
    for(unsigned axis=0;axis<3;axis++)for(int sign=-1;sign<=1;sign+=2)
    for(int offset=-1;offset<=1;offset++)for(unsigned mass=0;mass<3;mass++)
    for(unsigned speed=0;speed<4;speed++)for(unsigned rate=0;rate<6;rate++) {
        unsigned fps=rate<5?test_rates[rate]:144;
        g_tsfp_native_timing_active=rate!=0;reset(2);wall_contact=1;
        unsigned tangent=(axis+1)%3,angular=(axis+2)%3;
        float arm=offset*.25f,closing=closing_speeds[speed],inverse_mass=inverse_masses[mass];
        MEMF(BODY+0x1a8)=inverse_mass;
        for(unsigned i=0;i<3;i++) {
            contact_normal[i]=i==axis?(float)sign:0;
            contact_offset[i]=i==tangent?arm:0;
            MEMF(FORCE+i*4)=MEMF(FORCE+12+i*4)=0;
            MEMF(BODY+0x14c+i*4)=i==axis?-sign*closing:0;
            MEMF(BODY+0x128+i*4)=MEMF(BODY+0x134+i*4)=0;
        }
        float ticks=0;unsigned frame=0;
        while(ticks<1.f) {
            float delta=rate==5?(frame%2?.8f:.2f):60.f/fps;
            if(delta>1.f-ticks)delta=1.f-ticks;
            unsigned whole=(unsigned)(ticks+delta)-(unsigned)ticks;
            step(whole,delta);ticks+=delta;frame++;
        }
        float impulse=closing>0 ? closing/(inverse_mass+arm*arm) : 0;
        float expected_linear=sign*(-closing+inverse_mass*impulse);
        float expected_angular=-sign*arm*impulse;
        printf("wall_contact fps=%u mode=%u axis=%u sign=%d offset=%d mass=%u speed=%u v=%.9f expected=%.9f w=%.9f expected_w=%.9f\n",
               fps,rate,axis,sign,offset,mass,speed,MEMF(BODY+0x14c+4*axis),expected_linear,
               MEMF(BODY+0x134+4*angular),expected_angular);
        assert(integrations && (closing<=0 || contacts));
        /* Original16CA00 also applies linear drag 0.001*v, verified at
         * US constant475DBC. Bound its one-tick velocity contribution plus
         * float roundoff; do not mistake it for a contact-impulse error. */
        double linear_tolerance=.001*fabs(closing)*inverse_mass/60.+.00002;
        double energy=0;
        for(unsigned i=0;i<3;i++) {
            float v=MEMF(BODY+0x14c+4*i),w=MEMF(BODY+0x134+4*i);
            double linear_error=fabs(v-(i==axis?expected_linear:0));
            double angular_error=fabs(w-(i==angular?expected_angular:0));
            if(linear_error>max_linear_error)max_linear_error=linear_error;
            if(angular_error>max_angular_error)max_angular_error=angular_error;
            assert(isfinite(v) && isfinite(w) && linear_error<linear_tolerance && angular_error<.00002);
            energy+=.5*((double)v*v/inverse_mass+(double)w*w);
        }
        assert(energy<=.5*(double)closing*closing/inverse_mass+.0001);
        wall_cases++;
    }
    printf("wall_summary cases=%u max_linear_error=%.9f max_angular_error=%.9f\n",wall_cases,max_linear_error,max_angular_error);
    /* Real pillar contacts include quantized normals, shallow overlap and
     * rebound. Original168720 specifies an escape speed in world units/sec:
     * max(12*clamp(-depth,0,.1), restitution*closing). It is not per-frame
     * displacement. Test the full solver against an independent generalized
     * impulse, retaining authored drag and unit inverse inertia. Geometry is
     * still controlled; this is not a replay of the live corner encounter. */
    const float depths[]={-.006f,-.05f,-.15f,.02f},rebounds[]={0,.004f,.4f};
    unsigned overlap_cases=0;double overlap_linear_error=0,overlap_angular_error=0;
    for(unsigned depth=0;depth<4;depth++)for(unsigned rebound=0;rebound<3;rebound++)
    for(unsigned normal=0;normal<2;normal++)for(int arm=-1;arm<=1;arm++)
    for(unsigned kind=0;kind<2;kind++)for(unsigned rate=0;rate<6;rate++) {
        unsigned fps=rate<5?test_rates[rate]:144;
        g_tsfp_native_timing_active=rate!=0;reset(2);penetration_contact=1;
        contact_depth=depths[depth];contact_restitution=rebounds[rebound];contact_kind=kind?2:0;
        float n[3]={normal?-7.f/127.f:0,0,normal?-126.f/127.f:-1.f};
        double norm2=(double)n[0]*n[0]+(double)n[2]*n[2],offset=arm*.25;
        double cross[3]={offset*n[2],0,-offset*n[0]},cross2=cross[0]*cross[0]+cross[2]*cross[2];
        double initial[3],target=fmax(12.*fmin(fmax(-contact_depth,0),.1),12.*contact_restitution);
        double impulse=(12.+target)/(.5*norm2+cross2),initial_energy=0;
        for(unsigned i=0;i<3;i++) {
            contact_normal[i]=n[i];contact_offset[i]=i==1?(float)offset:0;
            MEMF(FORCE+i*4)=MEMF(FORCE+12+i*4)=0;
            initial[i]=-12.*n[i]/norm2;MEMF(BODY+0x14c+4*i)=(float)initial[i];
            MEMF(BODY+0x128+4*i)=MEMF(BODY+0x134+4*i)=0;
            initial_energy+=initial[i]*initial[i];
        }
        float ticks=0;unsigned frame=0;
        while(ticks<1.f) {
            float delta=rate==5?(frame%2?.8f:.2f):60.f/fps;
            if(delta>1.f-ticks)delta=1.f-ticks;
            unsigned whole=(unsigned)(ticks+delta)-(unsigned)ticks;
            step(whole,delta);ticks+=delta;frame++;
        }
        printf("penetration fps=%u mode=%u depth=%u rebound=%u normal=%u arm=%d kind=%u target=%.9f\n",
               fps,rate,depth,rebound,normal,arm,contact_kind,target);
        /* Linear speed cannot exceed the initial kinetic-energy bound here.
         * Allow authored drag .001*v * inverse_mass over one tick, plus
         * roundoff. Angular response has an independent roundoff bound. */
        double linear_tolerance=.001*sqrt(initial_energy)*.5/60.+.00002;
        double energy=0;
        for(unsigned i=0;i<3;i++) {
            double expected_v=initial[i]+.5*impulse*n[i],expected_w=impulse*cross[i];
            float v=MEMF(BODY+0x14c+4*i),w=MEMF(BODY+0x134+4*i);
            double ve=fabs(v-expected_v),we=fabs(w-expected_w);
            if(ve>overlap_linear_error)overlap_linear_error=ve;
            if(we>overlap_angular_error)overlap_angular_error=we;
            assert(isfinite(v) && isfinite(w) && ve<linear_tolerance && we<.00002);
            energy+=(double)v*v+.5*(double)w*w;
        }
        assert(energy<=initial_energy+.0001);overlap_cases++;
    }
    printf("penetration_summary cases=%u max_linear_error=%.9f max_angular_error=%.9f\n",
           overlap_cases,overlap_linear_error,overlap_angular_error);
    free(memory);puts("PASS: original rigid motion, force, drag, quaternion/matrix pipeline, support/contact impulse, fresh steps and pause");return 0;
}
