/* Original US vehicle gunner input/update with controlled service boundaries.
 * Mouse aim must survive D2470's player-angle reset and 17A440's gun update.
 * This proves routing, not rendered direction in a specific campaign scene. */
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

enum {PLAYER=0x10000,TABLE=0x20000,ENTITY=TABLE+0x2e8,VEHICLE=TABLE+0x5d0,EXTRA=0x30000,VDATA=0x40000,SEAT=VDATA+0x70,GUN=0x50000,DATA=0x51000,FOV=0x60000,DEF=0x62000,SETTINGS=0x63000,MATRIX=0x64000,STACK=0x70000};
static float axis[8];
static void retf(float f){g_fp_top=(g_fp_top-1)&7;g_fp_stack[g_fp_top]=f;g_esp+=4;}
void sub_000B3CD0(void){g_eax=DEF;g_esp+=4;}
void sub_00012F00(void){assert(MEM32(MEM32(g_esp+4))==0x400001);g_eax=ENTITY;g_esp+=4;}
void sub_000CB3B0(void){retf(55);}
void sub_000124F0(void){float x=MEMF(g_esp+4),lo=MEMF(g_esp+8),hi=MEMF(g_esp+12);retf(fminf(hi,fmaxf(lo,x)));}
void sub_00012540(void){float x=MEMF(g_esp+4),lo=MEMF(g_esp+8),hi=MEMF(g_esp+12);while(x<lo)x+=hi-lo;while(x>=hi)x-=hi-lo;retf(x);}
void sub_00165690(void){g_eax=MATRIX;g_esp+=4;}
void sub_001528A0(void){MEMF(MEM32(g_esp+8))=0;MEMF(MEM32(g_esp+12))=0;MEMF(MEM32(g_esp+16))=0;g_esp+=4;}
void sub_00042970(void){retf(axis[MEM32(g_esp+8)]);}
void sub_000427B0(void){g_eax=0;g_esp+=4;}
void sub_00042760(void){g_eax=0;g_esp+=4;}
void sub_0017A000(void){g_eax=0;g_esp+=4;}
void sub_00238510(void){g_esp+=4;}
void sub_0017AA20(void){g_esp+=4;} /* Camera/render boundary; no angle writes here. */
void sub_00177950(void){g_eax=MEM32(VDATA+0x58);g_esp+=4;}
void sub_0017A440(void);
void sub_0017DA00(void){
 uint32_t saved=g_esp, packet=MEM32(g_esp+8);g_eax=SEAT;
 g_esp-=12;MEM32(g_esp)=0;MEM32(g_esp+4)=VEHICLE;MEM32(g_esp+8)=packet;
 sub_0017A440();g_esp=saved+4;
}
void sub_003C848C(void){g_eax=(int32_t)g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;g_esp+=4;}
/* UNREACHED */
void sub_0017A0A0(void);
/* GENERATED_ROUTINES */
/* MOUSE_IMPLEMENTATION */
static void step(void){g_esp=STACK;MEM32(STACK)=0;sub_000D2470();assert(g_esp==STACK+4);}
int main(void){
 setvbuf(stdout,NULL,_IONBF,0);
 unsigned char *memory=calloc(1,0x800000);assert(memory);g_xbox_mem_offset=(ptrdiff_t)memory;
 /* CONSTANTS */
 for(int i=0;i<2048;i++)MEMF(0x7a38a0+4*i)=sinf(i*6.283185307179586/2048);
 MEM32(0x7b0c7c)=PLAYER;MEM32(0x7b0cc0)=FOV;MEM32(0x72f5e8)=TABLE;
 MEM32(PLAYER+0x14)=ENTITY;MEM32(PLAYER+0x114)=0x400;MEM32(PLAYER+0x1500)=SETTINGS;
 MEM32(PLAYER+0x68)=4;MEM32(PLAYER+0x6c)=3;
 MEM32(ENTITY+0x2c)=0x400001;MEM32(ENTITY+0x7c)=EXTRA;MEM32(ENTITY+0x150)=PLAYER;
 MEM32(EXTRA)=ENTITY;MEM32(EXTRA+0xc08)=0x800001;
 MEM32(VEHICLE+0x2c)=0x800001;MEM32(VEHICLE+0x7c)=VDATA;
 MEM32(VDATA+0x140)=1;MEM32(VDATA+0x164)=1;MEM32(VDATA+0x144)=DATA;
 MEM32(SEAT)=0x400001;MEM32(SEAT+0x2c)=0;
 MEM32(GUN+0x7c)=DATA;MEM32(DATA)=GUN;MEM32(DATA+0xd8)=ENTITY;
 MEMF(DATA+0x34)=-90;MEMF(DATA+0x38)=90;MEMF(DATA+0x3c)=-180;MEMF(DATA+0x40)=180;
 MEMF(DATA+0x58)=MEMF(DATA+0x5c)=1;MEMF(FOV+0x384)=55;
 MEM32(0x4e7944)=1;MEMF(0x4e794c)=1;MEM32(0x4e8cec)=60;
 for(int type=3;type<=4;type++){
  MEM32(SEAT+0x30)=type;MEMF(DATA+0x1c)=MEMF(DATA+0x24)=0;MEMF(PLAYER+0x100)=MEMF(PLAYER+0x104)=0;
  PcInput input={0};pc_input_focus(&input,1);pc_input_capture(&input,1);pc_input_motion(&input,100,20,1000);
  int handled=pc_mounted_mouse(&input,1001,.1f,0,PLAYER,FOV,g_xbox_mem_offset,0);assert(handled==1);
  assert(fabsf(MEMF(DATA+0x24)+10)<.001f && fabsf(MEMF(DATA+0x1c)-2)<.001f);
  step();
  assert(fabsf(MEMF(DATA+0x24)+10)<.001f && fabsf(MEMF(DATA+0x1c)-2)<.001f);
  assert(MEMF(PLAYER+0x104)==0);
  /* Original gun-camera rotation (17AA20 -> 151650/152090): mouse down
   * must produce a downward direction, independent of player angle resets. */
  g_esp=STACK;MEM32(STACK+4)=MATRIX;MEMF(STACK+8)=MEMF(DATA+0x1c)*MEMF(0x475d0c);
  MEMF(STACK+12)=0;MEMF(STACK+16)=0;sub_00151650();
  MEMF(DEF)=0;MEMF(DEF+4)=0;MEMF(DEF+8)=1;
  g_esp=STACK;MEM32(STACK+4)=MATRIX;MEM32(STACK+8)=DEF;sub_00152090();
  assert(MEMF(DEF+4)<0);

  MEMF(DATA+0x24)=MEMF(DATA+0x1c)=0;
  axis[3]=.5f;axis[4]=.25f;step();assert(MEMF(DATA+0x24)!=0 && MEMF(DATA+0x1c)!=0);
  axis[3]=axis[4]=0;
 }
 puts("PASS: original vehicle gunner update preserves mouse aim and controller input");free(memory);return 0;
}
