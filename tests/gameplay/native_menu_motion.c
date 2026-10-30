/* Execute the original frontend backdrop update and its two motion helpers.
 * Matrix construction and menu services are stubbed; this verifies time,
 * rotation and texture-index transitions, not rendered pixels or world UVs. */
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
RECOMP_TLS int g_fp_top,g_fp_cmp;
enum { BACKDROP=0x1000, VIEW=0x3000, STACK=0x7000 };
void sub_0003C410(void) { g_eax=480; g_esp+=4; }
void sub_0003C400(void) { g_eax=640; g_esp+=4; }
void sub_00151CF0(void) { g_esp+=4; }
void sub_00151C90(void) { g_esp+=4; }
void sub_00153270(void) { g_esp+=4; }
void sub_002D75B0(void) { g_esp+=4; }
void sub_000774B0(void) { g_eax=0; g_esp+=4; }
void sub_000746E0(void) { g_eax=0; g_esp+=4; }
void sub_00074BC0(void)
{ g_fp_top=(g_fp_top+7)&7; g_fp_stack[g_fp_top]=0; g_esp+=4; }
/* GENERATED_ROUTINES */
static const unsigned rings[]={0xc8,0xf0,0x190,0x140,0x168};
static float wrapped(float angle)
{
    if(angle>360) angle-=360*floorf(angle/360);
    if(angle<-360) angle+=360*floorf(-angle/360);
    return angle;
}
int main(void)
{
    void *memory=calloc(1,0x800000);
    assert(memory); g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    MEM32(0x789834)=BACKDROP; MEM32(0x7e71c8)=VIEW;
    MEM32(VIEW+12)=640; MEM32(VIEW+32)=480; MEM32(0x4e8cec)=60;
    MEM32(0x78ae00)=0xffffffff;
    MEMF(0x475c78)=1; MEMF(0x475c88)=360; MEMF(0x475cb8)=-360;
    MEMF(0x475c84)=180; MEMF(0x475cac)=0;
    const unsigned rates[]={60,120,144,180,240};
    for(unsigned r=0;r<sizeof rates/sizeof rates[0];r++) {
        unsigned fps=rates[r],flips=0,last=0;
        memset((void *)((uintptr_t)BACKDROP+g_xbox_mem_offset),0,0x1000);
        for(unsigned i=0;i<5;i++) {
            MEMF(BACKDROP+rings[i]+20)=i%2?-.2f:.2f;
            MEMF(BACKDROP+rings[i]+16)=i%2?-350:350;
        }
        for(unsigned i=0;i<16;i++) MEMF(BACKDROP+0x1fc+i*0x44+0x24)=1.9f;
        for(unsigned f=0;f<fps*6;f++) {
            float ticks=(f+1)*60.f/fps;
            MEM32(0x4e7940)=(f+1)*60/fps-f*60/fps;
            MEMF(0x4e7948)=60.f/fps;
            /* Frontend time must remain independent of paused game time. */
            MEM32(0x4e7944)=0; MEMF(0x4e794c)=0;
            g_tsfp_native_timing_active=r!=0;
            g_esp=STACK; MEM32(STACK)=0x12345678; MEM32(STACK+4)=0;
            sub_002DA0E0();
            assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
            assert(fabsf(MEMF(BACKDROP+0xc4)-(f+1)/(float)fps)<.0001f);
            assert(MEMF(BACKDROP+0x63c)==60.f/fps);
            for(unsigned i=0;i<5;i++)
                assert(fabsf(remainderf(MEMF(BACKDROP+rings[i]+16)-
                    wrapped((i%2?-350:350)+ticks*(i%2?-.2f:.2f)),360))<.02f);
            for(unsigned i=0;i<16;i++)
                assert(fabsf(MEMF(BACKDROP+0x1fc+i*0x44+0x20)-wrapped(ticks*1.9f))<.02f);
            unsigned index=MEM32(BACKDROP+0x1fc+0x38);
            if(index!=last) {
                ++flips; last=index;
                assert(fabsf(ticks*1.9f-(flips==1?180.f:540.f))<1.91f);
            }
        }
        assert(flips==2 && last==0);
        float phase=MEMF(BACKDROP+0x1fc+0x20);
        MEMF(0x4e7948)=0; g_esp=STACK; sub_002DA0E0();
        assert(MEMF(BACKDROP+0x1fc+0x20)==phase && MEM32(BACKDROP+0x1fc+0x38)==last);
    }
    free(memory);
    puts("PASS: original frontend fractional motion, raw-time independence and texture alternation");
    return 0;
}
