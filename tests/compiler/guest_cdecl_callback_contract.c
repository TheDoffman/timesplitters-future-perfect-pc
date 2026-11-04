#include "guest_callback_call.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"cdecl line%d %s\n",__LINE__,#x);exit(1);}}while(0)
ptrdiff_t g_xbox_mem_offset;
static unsigned char ram[8192],before[8192];
static unsigned argc,callee_pops,called,fail_map,jumping;
static uint32_t arguments[5]={0x89abcdef,0x10203040,0x50607080,0xaabbccdd,0x12345678};
static void seed(void) {
 g_eax=31;g_ebx=32;g_ecx=33;g_edx=34;g_esi=35;g_edi=36;g_esp=0x1007;g_ebp=37;g_seh_ebp=38;g_fs_base=39;
 g_xmm7.q[0]=0x1234;g_xmm7.q[1]=0x5678;g_mm7.q=0x4321;g_fp_stack[7]=1.25;g_df=1;g_fp_top=2;g_fp_cmp=3;g_fp_control_word=0x123;g_fp_cc=0x456;
#ifdef RECOMP_STATUS_FLAGS_SUPPORTED
 g_status_flags.value=0x246802;g_status_flags.known=0x247fd7;
#endif
}
static void preserved(void) {
 CHECK(g_eax==31 && g_ebx==32 && g_ecx==33 && g_edx==34 && g_esi==35 && g_edi==36 && g_esp==0x1007 && g_ebp==37 && g_seh_ebp==38 && g_fs_base==39);
 CHECK(g_xmm7.q[0]==0x1234 && g_xmm7.q[1]==0x5678 && g_mm7.q==0x4321 && g_fp_stack[7]==1.25);
 CHECK(g_df==1 && g_fp_top==2 && g_fp_cmp==3 && g_fp_control_word==0x123 && g_fp_cc==0x456);
#ifdef RECOMP_STATUS_FLAGS_SUPPORTED
 CHECK(g_status_flags.value==0x246802 && g_status_flags.known==0x247fd7);
#endif
}
static void callback(void) {
 uint32_t stack=g_esp;unsigned i;
 CHECK(stack==0x1007-4-4*argc && MEM32(stack)==0 && g_seh_ebp==stack);
 for(i=0;i<argc;i++)CHECK(MEM32(stack+4+4*i)==arguments[i]);++called;
 g_eax=0x87654321;g_ebx=g_ecx=g_edx=g_esi=g_edi=g_ebp=g_seh_ebp=g_fs_base=0xdeadbeef;
 g_xmm7.q[0]=g_xmm7.q[1]=g_mm7.q=0x123456;g_fp_stack[7]=9.;g_df=0;g_fp_top=5;g_fp_cmp=8;g_fp_control_word=g_fp_cc=0;
#ifdef RECOMP_STATUS_FLAGS_SUPPORTED
 g_status_flags.value=g_status_flags.known=0;
#endif
 if(jumping){CHECK(recomp_guest_longjmp(0x100,7));CHECK(0);}
 g_esp=stack+4+(callee_pops?4*argc:0);
}
static tsfp_guest_entry_t lookup(void *c,uint32_t routine){CHECK(c==ram);return routine==0x1234?callback:NULL;}
static void *output(void *c,uint32_t address,uint32_t bytes){CHECK(c==ram);return fail_map || address>sizeof ram || bytes>sizeof ram-address?NULL:ram+address;}
int main(void) {
 tsfp_guest_callback_ops ops={ram,lookup,output};unsigned convention,pops,i;uint32_t result;int ok;
 g_xbox_mem_offset=(ptrdiff_t)ram;
 for(argc=0;argc<=4;argc++)for(convention=0;convention<2;convention++)for(pops=0;pops<2;pops++) {
  memset(ram,0xa7,sizeof ram);memcpy(before,ram,sizeof ram);seed();result=0xdecafbad;called=0;callee_pops=pops;
  ok=convention?tsfp_guest_callback_cdecl(&ops,0x1234,argc?arguments:NULL,argc,&result):tsfp_guest_callback_stdcall(&ops,0x1234,argc?arguments:NULL,argc,&result);
  CHECK(ok==(argc==0 || pops!=convention));CHECK(called==1);preserved();
  CHECK(result==(ok?0x87654321u:0xdecafbadu));
  for(i=0;i<sizeof ram;i++) {
   uint32_t start=0x1007-4-4*argc;unsigned expected=before[i];
   if(i>=start && i<0x1007){unsigned offset=i-start,word=offset/4;expected=word?((arguments[word-1]>>(8*(offset%4)))&255):0;}
   CHECK(ram[i]==expected);
  }
 }
 memset(ram,0xa7,sizeof ram);memcpy(before,ram,sizeof ram);seed();result=0xdecafbad;called=0;argc=2;callee_pops=0;
 CHECK(!tsfp_guest_callback_cdecl(NULL,0x1234,arguments,2,&result));
 CHECK(!tsfp_guest_callback_cdecl(&ops,0,arguments,2,&result));
 CHECK(!tsfp_guest_callback_cdecl(&ops,0x9999,arguments,2,&result));
 CHECK(!tsfp_guest_callback_cdecl(&ops,0x1234,NULL,2,&result));
 CHECK(!tsfp_guest_callback_cdecl(&ops,0x1234,arguments,5,&result));
 fail_map=1;CHECK(!tsfp_guest_callback_cdecl(&ops,0x1234,arguments,2,&result));fail_map=0;
 g_esp=3;CHECK(!tsfp_guest_callback_cdecl(&ops,0x1234,arguments,2,&result));g_esp=0x1007;
 CHECK(result==0xdecafbad && called==0 && !memcmp(ram,before,sizeof ram));preserved();
 CHECK(tsfp_guest_callback_cdecl(&ops,0x1234,arguments,2,NULL));preserved();
 /* A real native transfer abandons this wrapper; target state must survive. */
 MEM32(0x100)=0x1122;MEM32(0x104)=0x3344;MEM32(0x108)=0x5566;MEM32(0x10c)=0x7788;MEM32(0x110)=0x9900;jumping=1;
 switch(setjmp(*recomp_setjmp_slot(0x100))) {
 case 0:(void)tsfp_guest_callback_cdecl(&ops,0x1234,arguments,2,&result);CHECK(0);break;
 case 7:break;
 default:CHECK(0);
 }
 CHECK(g_ebp==0x1122 && g_ebx==0x3344 && g_edi==0x5566 && g_esi==0x7788 && g_esp==0x9904);
 puts("PASS: cdecl/stdcall cleanup distinction, guarded frames, result publication, architecture restoration and nonlocal transfer");return 0;
}
