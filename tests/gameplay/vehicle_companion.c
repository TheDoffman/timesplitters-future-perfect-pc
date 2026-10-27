/* Execute the original mounted companion AI with isolated RAM and vehicle
 * services. Animation, rendering and unrelated states abort if reached. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#undef RECOMP_ITAIL
#define RECOMP_ITAIL(address) abort()
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3;
static const uint32_t prop=0x1000, character=0x2000, vehicle=0x4000;
static const uint32_t vehicle_data=0x5000, driver=0x6000, stack=0x10000;
static unsigned flags, has_driver, companion, interaction_result;
static unsigned checks;
void sub_0015E120(void) { g_esp+=4; }
void sub_0015D470(void) { g_esp+=4; }
void sub_00012F00(void) { g_eax=vehicle;g_esp+=4; }
void sub_00177920(void) { g_eax=vehicle_data;g_esp+=4; }
void sub_0029FE60(void) { g_esp+=4; }
void sub_0017E010(void) { g_eax=interaction_result;g_esp+=4; }
void sub_0029FE20(void) { g_eax=companion;g_esp+=4; }
void sub_00177950(void) { g_eax=flags;g_esp+=4; }
void sub_00179F70(void) {
    MEM32(MEM32(g_esp+8))=driver;g_eax=has_driver;g_esp+=4;
}
void sub_00277FE0(void) { g_eax=character;g_esp+=4; }
#define UNUSED(name) void name(void) { fprintf(stderr,"Unexpected call: " #name "\n");abort(); }
UNUSED(sub_00017570)
UNUSED(sub_0009A420)
UNUSED(sub_0009BCF0)
UNUSED(sub_001596F0)
UNUSED(sub_0015F0D0)
UNUSED(sub_00172CD0)
UNUSED(sub_00177E90)
UNUSED(sub_00177F20)
UNUSED(sub_00177FB0)
UNUSED(sub_0017A0A0)
UNUSED(sub_0017A200)

/* GENERATED_VEHICLE_ROUTINE */

static void check(unsigned state, unsigned player, unsigned present,
                  unsigned vehicle_flags, unsigned target, unsigned result,
                  unsigned expected) {
    unsigned actual;
    memset((void *)(g_xbox_mem_offset+character),0,0x1000);
    MEM32(prop+0x7c)=character;
    MEM32(character+0x20)=state;MEM32(character+0x9d0)=state;
    MEM32(character+4)=target;MEM32(driver+0x150)=player;
    flags=vehicle_flags;has_driver=present;interaction_result=result;
    g_esp=stack;MEM32(stack)=0xDEADBEEF;MEM32(stack+4)=prop;
    g_ebx=0xABC;g_esi=0xDEF;g_edi=0x123;g_seh_ebp=0x456;
    sub_002A0500();
    actual=MEM32(character+0x20);++checks;
    if(actual!=expected || g_esp!=stack+4 || g_ebx!=0xABC || g_esi!=0xDEF || g_edi!=0x123) {
        fprintf(stderr,"FAIL: mounted AI state %u driver %u player %x flags %x target %x: got %u expected %u; esp %x\n",
                state,present,player,vehicle_flags,target,actual,expected,g_esp);exit(1);
    }
}
int main(void) {
    unsigned char *memory=calloc(1,0x800000);
    if(!memory)return 2;
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_JUMP_TABLES */
    companion=1;
    if(/* LEGACY */) {
        check(9,0x7000,1,0,0,0,13);
        puts("PASS: pre-fix player driver incorrectly triggers dismount");
    } else {
        unsigned i;
        for(i=0;i<600;++i)check(9,0x7000,1,0,0,0,9);
        check(9,0x7000,0,0,0,0,13); /* Driver gone. */
        check(9,0,1,0,0,0,13); /* Non-player driver. */
        check(9,0,0,16,0,0,9); /* Vehicle explicitly keeps NPC mounted. */
        check(9,0x7000,1,0,0x8000,0,10); /* Acquire enemy normally. */
        companion=0;
        check(9,0,0,0,0,0,9); /* Different AI ownership. */
        companion=1;
        /* State 10's interaction status 5 reaches the other incoming edge
         * of the shared JNE, which checks the vehicle's keep-mounted bit. */
        check(10,0x7000,1,16,0,5,10);
        check(10,0x7000,1,0,0,5,13);
        printf("PASS: %u mounted companion decisions and guest ABI checks\n",checks);
    }
    free(memory);return 0;
}
