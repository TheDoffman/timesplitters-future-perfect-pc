/* Original supported-US mech input and aim-vector routines. Surrounding
 * scene lookup and angle-wrap services are controlled, not rendered gameplay. */
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
enum {PLAYER=0x10000,SCENE=0x20000,ENTITY=0x30000,EXTRA=0x31000,DATA=0x33000,FOV=0x40000,SETTINGS=0x41000,LOOK=0x42000,STACK=0x70000};
void sub_000C9F50(void){g_eax=MEM32(0x733108);g_esp+=4;}
void sub_00012540(void){float x=MEMF(g_esp+4),lo=MEMF(g_esp+8),hi=MEMF(g_esp+12);while(x<lo)x+=hi-lo;while(x>=hi)x-=hi-lo;g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=x;g_esp+=4;}
/* MOUSE_IMPLEMENTATION */
/* UNREACHED */
/* GENERATED_ROUTINES */
static void step(void){g_esp=STACK;MEMF(STACK+4)=MEMF(STACK+8)=0;MEM32(STACK+12)=LOOK;sub_0037AA70();assert(g_esp==STACK+4);}
int main(void){
#ifdef _WIN32
 _set_abort_behavior(0,_CALL_REPORTFAULT);
#endif
 unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
 /* CONSTANTS */
 for(int i=0;i<2048;i++)MEMF(0x7a38a0+4*i)=sinf(i*6.283185307179586/2048);
 MEM32(0x733108)=SCENE;MEM32(SCENE+0x2ac)=ENTITY;MEM32(ENTITY+0x20)=8;
 MEM32(ENTITY+0x7c)=EXTRA;MEM32(EXTRA+0xaa0)=DATA;MEM32(DATA+0x50)=1;
 MEM32(0x7b0c7c)=PLAYER;MEM32(PLAYER+0x114)=0x8000;MEM32(PLAYER+0x1500)=SETTINGS;
 MEMF(FOV+0x384)=55;
 for(int rate=60;rate<=240;rate+=60){
  MEM32(0x4e7944)=rate==60;MEMF(0x4e794c)=60.f/rate;
  MEMF(DATA+0x2c)=MEMF(DATA+0x30)=0;
  PcInput input={0};pc_input_focus(&input,1);pc_input_capture(&input,1);pc_input_motion(&input,100,20,1000);
  int handled=pc_mounted_mouse(&input,1001,.1f,0,PLAYER,FOV,g_xbox_mem_offset,rate!=60);assert(handled==1);
  step();assert(fabsf(MEMF(DATA+0x30)-350)<.001f && fabsf(MEMF(DATA+0x2c)-358)<.001f);
  /* 37B720's active-control copy then feeds the original gun direction. */
  MEMF(DATA+0x20)=MEMF(DATA+0x2c);MEMF(DATA+0x24)=MEMF(DATA+0x30);
  g_esp=STACK;MEM32(STACK+4)=ENTITY;MEM32(STACK+8)=LOOK+16;sub_003797B0();
  assert(MEMF(LOOK+16)<-.1f && MEMF(LOOK+20)<-.02f && MEMF(LOOK+24)>.9f);
 }
 /* Controller input still changes both target angles via its original path. */
 MEMF(LOOK)=1;MEMF(LOOK+4)=1;step();
 assert(fabsf(MEMF(DATA+0x30)-352)<.001f && MEMF(DATA+0x2c)==0);
 MEMF(LOOK)=MEMF(LOOK+4)=0;MEM32(DATA+0x50)=0;
 float yaw=MEMF(DATA+0x30);step();assert(MEMF(DATA+0x30)==yaw);
 free(memory);puts("PASS: original mech input, mouse targets and controller path");return 0;
}
