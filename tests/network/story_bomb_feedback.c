/* Compare original falling-bomb camera feedback with the guest replay. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;assert(0);return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t a){(void)a;assert(0);return NULL;}
void recomp_icall_fail_log(uint32_t a){(void)a;assert(0);}

#include <math.h>
#define WORD_AT(a) MEM32(a)
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<0x800000 && n<=0x800000-p;}
static unsigned story_npc_humans(void){return 4;}
static uint32_t guest_call(void (*fn)(void),unsigned n,...);
void sub_000D1EC0(void),sub_000C7880(void);
/* PRODUCTION_FEEDBACK */
static float original_shake[3];
static uint32_t captured_strength;
static float impact[3];
void sub_000EFBD0(void){
    assert(MEM32(g_esp)==0x219f9f && MEM32(g_esp+4)==39);
    captured_strength=MEM32(g_esp+44);
    memcpy(impact,(void *)((uintptr_t)MEM32(g_esp+12)+g_xbox_mem_offset),12);
    g_eax=0;g_esp+=4;
}
void sub_000873C0(void){MEM32(MEM32(g_esp+12))=1;g_eax=1;g_esp+=4;}
void sub_00088C60(void){
    const uint32_t point=MEM32(g_esp+20),normal=MEM32(g_esp+24);
    MEMF(point)=10;MEMF(point+4)=MEMF(point+8)=0;
    MEMF(normal)=MEMF(normal+8)=0;MEMF(normal+4)=1;
    g_eax=1;g_esp+=4;
}
void sub_00153B40(void){g_eax=0;g_esp+=4;}
void sub_000FF490(void){g_esp+=4;}
/* UNREACHED */
/* ORIGINAL_BOMB */
#include <stdarg.h>
static uint32_t guest_call(void (*fn)(void),unsigned n,...){
    assert(fn==sub_000D1EC0 && n==4);
    const uint32_t sp=g_esp;
    g_esp-=32;MEM32(g_esp)=0;
    va_list a;va_start(a,n);
    for(unsigned i=0;i<n;i++) MEM32(g_esp+4+4*i)=va_arg(a,uint32_t);
    va_end(a);fn();g_esp=sp;return 0;
}
int main(void){
    void *ram=calloc(1,0x800000);assert(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* ORIGINAL_CONSTANTS */
    const uint32_t pool=0x60000,local=pool+0x1584,camera=0x70000;
    MEM32(0x7b0c48)=pool;MEM32(local)=1;MEM32(local+4)=0;MEM32(local+8)=camera;
    MEM32(pool+8)=camera;MEM32(0x7b0cc0)=camera;MEM32(camera+0x21c)=4;
    MEMF(camera+0x27c)=20;MEMF(camera+0x280)=MEMF(camera+0x284)=0;
    MEM32(0x78d0e0)=2;MEM32(0x4e8cec)=60;MEMF(0x4e794c)=1;
    MEM32(0x78d108)=0x71000;
    g_esp=0x20000;
    sub_00219CA0();assert(g_esp==0x20004);
    assert(captured_strength==0x3f6aaaab && impact[0]==10);
    memcpy(original_shake,(void *)((uintptr_t)(camera+0x398)+g_xbox_mem_offset),12);
    const uint32_t flash=MEM32(pool+0xfc0);
    assert(fabsf(original_shake[0]-.45833334f)<.00001f);
    memset((void *)((uintptr_t)(camera+0x398)+g_xbox_mem_offset),0,12);
    MEM32(pool+8)=0;MEM32(pool+4)=2;
    StoryEffect s={0};memcpy(s.position,impact,12);
    story_effect_bomb_feedback(&s);
    assert(!memcmp(original_shake,(void *)((uintptr_t)(camera+0x398)+g_xbox_mem_offset),12));
    assert(MEM32(local+0xfbc)==MEM32(pool+0xfbc) && MEM32(local+0xfc0)==flash);
    for(unsigned slot=2;slot<4;slot++) {
        MEM32(pool+(slot-1)*0x1584+4)=2;
        const uint32_t owner=pool+slot*0x1584;
        MEM32(owner)=slot;MEM32(owner+4)=0;MEM32(owner+8)=camera;
        memset((void *)((uintptr_t)(camera+0x398)+g_xbox_mem_offset),0,12);
        story_effect_bomb_feedback(&s);
        assert(!memcmp(original_shake,(void *)((uintptr_t)(camera+0x398)+g_xbox_mem_offset),12));
        assert(MEM32(owner+0xfc0)==flash);
    }
    MEMF(camera+0x27c)=100;
    memset((void *)((uintptr_t)(camera+0x398)+g_xbox_mem_offset),0,12);
    story_effect_bomb_feedback(&s);assert(MEM32(camera+0x398)==0);
    MEMF(camera+0x27c)=NAN;story_effect_bomb_feedback(&s);assert(MEM32(camera+0x398)==0);
    free(ram);puts("PASS: original falling-bomb caller shake arguments, distance falloff and feedback match the guest owned-camera replay");return 0;
}
