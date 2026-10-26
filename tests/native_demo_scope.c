/* Asset-free contract for the shared native demo interval scope. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef _MSC_VER
#define RECOMP_MANUAL_TLS __declspec(thread)
#else
#define RECOMP_MANUAL_TLS _Thread_local
#endif
static unsigned char *memory;
static ptrdiff_t g_xbox_mem_offset;
static int g_tsfp_native_timing_active;
#define PC_NATIVE_DEMO_READ(address,out,bytes) 0
void pc_native_visual_rebase(uint32_t value,double fraction) { (void)value;(void)fraction; }
#include "../src/pc_native_demo_guest.inc"
PcNativeStep g_tsfp_native_step;
#define WORD(a) (*(uint32_t *)(memory+(a)))
#define FLOAT(a) (*(float *)(memory+(a)))
static float observed;
static int nested, eof;
static void inner(void)
{
    assert(pc_native_demo_timer_multiply(0x433E2u,1.f,1.f)==0.25f);
    FLOAT(0x4e7948)=99.f;
}
static void original(void)
{
    observed=pc_native_demo_timer_multiply(0x433E2u,1.f,1.f);
    if(nested) {
        uint32_t mode=WORD(0x6b98a4);
        FLOAT(0x4e7948)=0.25f; WORD(0x6b98a4)=5;
        pc_native_demo_guest(inner);
        assert(pc_native_demo_timer_multiply(0x433E2u,1.f,1.f)==observed);
        WORD(0x6b98a4)=mode;
    }
    WORD(0x4e7940)=1; FLOAT(0x4e7948)=1.f; FLOAT(0x4e794c)=0.f;
    if(eof) WORD(0x6b98a4)=2;
}
int main(void)
{
    memory=calloc(1,0x800000); assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)memory;
    const float intervals[]={0.f,60.f/144,0.5f,1.f,2.f,3.f};
    for(unsigned active=0;active<=1;active++) for(unsigned i=0;i<6;i++)
    for(unsigned mode=0;mode<=5;mode++) {
        float interval=intervals[i];
        g_tsfp_native_timing_active=active;
        WORD(0x6b98a4)=mode; WORD(0x4e7940)=0;
        FLOAT(0x4e7948)=interval;
        nested=active; eof=mode==1;
        pc_native_demo_guest(original);
        assert(observed==(active?interval:1.f));
        int restore=active && (mode==1 || mode==2);
        assert(WORD(0x4e7940)==(restore?0u:1u));
        assert(FLOAT(0x4e7948)==(restore?interval:1.f));
        assert(FLOAT(0x4e794c)==0.f);
        assert(pc_native_demo_timer_multiply(0x43473u,3.f,4.f)==12.f);
    }
    const float invalid[]={-1.f,NAN,INFINITY};
    for(unsigned i=0;i<3;i++) {
        nested=eof=0; WORD(0x6b98a4)=2; FLOAT(0x4e7948)=invalid[i];
        pc_native_demo_guest(original);
        assert(observed==1.f && FLOAT(0x4e7948)==1.f);
    }
    free(memory);
    puts("PASS: original identity, fractional/zero/catch-up scopes, nesting, EOF and invalid-interval fallback");
    return 0;
}
