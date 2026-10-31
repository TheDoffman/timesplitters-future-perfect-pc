/* Execute the original US scripted mount/dismount routines, then feed the
 * resulting links to the production mouse helper. Camera/weapon services are
 * stubs; the guest's C14/D8 writes and cleanup execute unchanged. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp;
enum { PLAYER=0x10000, ENTITY=0x20000, EXTRA=0x21000, GUN=0x30000,
       DATA=0x31000, FOV=0x40000, SCRIPT=0x50000, STACK=0x70000 };
static uint32_t name;
void sub_00240560(void)
{ assert(MEM32(g_esp+4)==name);g_eax=DATA;g_esp+=4; }
void sub_000D2CE0(void)
{ assert(MEM32(g_esp+4)==PLAYER);MEM32(PLAYER+0x114)=MEM32(g_esp+8);g_esp+=4; }
void sub_001B7D00(void)
{ assert(MEM32(g_esp+4)==PLAYER);g_esp+=4; }
/* UNREACHED_SERVICES */
/* GENERATED_ROUTINES */
/* MOUSE_IMPLEMENTATION */
static void call(void (*routine)(void))
{
    g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=SCRIPT;MEM32(STACK+8)=name;
    g_ebx=0x1234;g_esi=0x5678;g_edi=0x9abc;
    routine();
    assert(g_esp==STACK+4 && g_ebx==0x1234 && g_esi==0x5678 && g_edi==0x9abc);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)memory;
    PcInput input={0};pc_input_focus(&input,1);pc_input_capture(&input,1);
    MEM32(PLAYER+0x14)=ENTITY;MEM32(ENTITY+0x7c)=EXTRA;MEM32(ENTITY+0x150)=PLAYER;
    MEM32(SCRIPT+0x1c)=ENTITY;MEM32(GUN+0x7c)=DATA;MEM16(GUN+0x94)=1;
    MEM32(DATA)=GUN;MEM32(DATA+0x10)=4;MEMF(FOV+0x384)=55;
    MEMF(DATA+0x34)=-90;MEMF(DATA+0x38)=90;
    MEMF(DATA+0x3c)=-180;MEMF(DATA+0x40)=180;
    MEMF(PLAYER+0x100)=34;MEMF(PLAYER+0x104)=-12;
    const uint32_t names[]={0x4147554e,0x54414e4b,0x47554e32,0};
    for(unsigned i=0;i<4;i++)for(int native=0;native<2;native++) {
        name=names[i];MEM32(DATA+4)=name;
        MEM32(DATA+0x68)=i&1?ENTITY:0; /* AI target is independent of mounting. */
        MEM32(0x4e7944)=native?0:1;MEMF(0x4e794c)=native?.4166667f:1;
        MEMF(DATA+0x1c)=MEMF(DATA+0x24)=0;
        call(sub_000B76B0);
        assert(MEM32(PLAYER+0x114)==0x80 && MEM32(EXTRA+0xc14)==GUN);
        assert(MEM32(DATA+0xd8)==ENTITY);
        pc_input_motion(&input,100,20,1000);
        assert(pc_mounted_mouse(&input,1001,.1f,0,PLAYER,FOV,g_xbox_mem_offset,native)==1);
        assert(fabsf(MEMF(DATA+0x24)+10)<.001f && fabsf(MEMF(DATA+0x1c)-2)<.001f);
        assert(MEMF(PLAYER+0x100)==34 && MEMF(PLAYER+0x104)==-12);
        call(sub_000B7830);
        assert(MEM32(PLAYER+0x114)==2 && !MEM32(EXTRA+0xc14) && !MEM32(DATA+0xd8));
        assert(pc_mounted_mouse(&input,1001,.1f,0,PLAYER,FOV,g_xbox_mem_offset,native)==-1);
    }
    free(memory);puts("PASS: original mount/dismount links drive named guns at original/native cadence");
    return 0;
}
