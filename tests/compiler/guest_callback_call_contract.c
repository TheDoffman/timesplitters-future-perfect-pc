#include "guest_callback_call.h"
#include "guest_lifetime.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"call line%d %s\n",__LINE__,#x);exit(1);}}while(0)
ptrdiff_t g_xbox_mem_offset;
static unsigned char ram[8192];static int called,resolve_fail,malformed,jumping;
static void seed(unsigned n) {
    unsigned i;
    g_eax=n+1;g_ebx=n+2;g_ecx=n+3;g_edx=n+4;g_esi=n+5;g_edi=n+6;
    g_ebp=n+7;g_seh_ebp=n+8;g_fs_base=n+9;g_icall_saved_esp=n+10;g_icall_dispatch_form=n+11;
    memset(&g_xmm0,(int)n,sizeof(g_xmm0));g_xmm1=g_xmm2=g_xmm3=g_xmm4=g_xmm5=g_xmm6=g_xmm7=g_xmm0;
    g_mm0.q=n;g_mm1=g_mm2=g_mm3=g_mm4=g_mm5=g_mm6=g_mm7=g_mm0;
    for(i=0;i<8;++i)g_fp_stack[i]=(double)n+(double)i+0.25;
    g_fp_top=(int)n;g_fp_cmp=-(int)n;g_df=(int)n;g_fp_control_word=(uint16_t)(n+1);g_fp_cc=(uint16_t)(n+2);
}
static void check_seed(void) {
    unsigned i;CHECK(g_eax==4 && g_ebx==5 && g_ecx==6 && g_edx==7 && g_esi==8 && g_edi==9);
    CHECK(g_ebp==10 && g_seh_ebp==11 && g_fs_base==12 && g_icall_saved_esp==13 && g_icall_dispatch_form==14);
    CHECK(g_xmm0.q[0]==UINT64_C(0x0303030303030303) && g_xmm0.q[1]==g_xmm0.q[0]);
    CHECK(!memcmp(&g_xmm0,&g_xmm1,sizeof(g_xmm0)) && !memcmp(&g_xmm0,&g_xmm2,sizeof(g_xmm0)));
    CHECK(!memcmp(&g_xmm0,&g_xmm3,sizeof(g_xmm0)) && !memcmp(&g_xmm0,&g_xmm4,sizeof(g_xmm0)));
    CHECK(!memcmp(&g_xmm0,&g_xmm5,sizeof(g_xmm0)) && !memcmp(&g_xmm0,&g_xmm6,sizeof(g_xmm0)) && !memcmp(&g_xmm0,&g_xmm7,sizeof(g_xmm0)));
    CHECK(g_mm0.q==3 && g_mm1.q==3 && g_mm2.q==3 && g_mm3.q==3 && g_mm4.q==3 && g_mm5.q==3 && g_mm6.q==3 && g_mm7.q==3);
    for(i=0;i<8;++i)CHECK(g_fp_stack[i]==3.25+(double)i);
    CHECK(g_fp_top==3 && g_fp_cmp==-3 && g_df==3 && g_fp_control_word==4 && g_fp_cc==5);
}
static void callback(void) {
    uint32_t stack=g_esp;CHECK(stack==0xfff && MEM32(stack)==0 && MEM32(stack+4)==0x2468);
    CHECK(g_seh_ebp==stack);++called;seed(99);
    if(jumping) {(void)recomp_guest_longjmp(0x100,7);CHECK(0);}
    g_esp=stack+(malformed?4:8);
}
static tsfp_guest_entry_t lookup(void *ctx,uint32_t address) {CHECK(ctx==ram);return address==0x4444?callback:NULL;}
static void *output(void *ctx,uint32_t address,uint32_t bytes) {
    CHECK(ctx==ram && bytes==8);return resolve_fail || address>sizeof(ram)-bytes?NULL:ram+address;
}
int main(void) {
    tsfp_guest_callback_ops ops={ram,lookup,output};unsigned i;
    memset(ram,0xa7,sizeof(ram));g_xbox_mem_offset=(ptrdiff_t)ram;seed(3);g_esp=0x1007;
    CHECK(!tsfp_guest_callback_one(NULL,0x4444,0x2468));
    CHECK(!tsfp_guest_callback_one(&ops,0,0x2468));
    CHECK(!tsfp_guest_callback_one(&ops,0x9999,0x2468));
    resolve_fail=1;CHECK(!tsfp_guest_callback_one(&ops,0x4444,0x2468));resolve_fail=0;
    for(i=0;i<sizeof(ram);++i)CHECK(ram[i]==0xa7);check_seed();CHECK(!called && g_esp==0x1007);
    CHECK(tsfp_guest_callback_one(&ops,0x4444,0x2468));check_seed();CHECK(called==1 && g_esp==0x1007);
    CHECK(ram[0xffe]==0xa7 && ram[0x1007]==0xa7);
    malformed=1;CHECK(!tsfp_guest_callback_one(&ops,0x4444,0x2468));check_seed();CHECK(called==2 && g_esp==0x1007);
    malformed=0;g_esp=7;CHECK(!tsfp_guest_callback_one(&ops,0x4444,0x2468) && g_esp==7 && called==2);
    /* Native continuation owns its target state; callback wrapper must not restore over it. */
    g_esp=0x1007;MEM32(0x100)=0x1122;MEM32(0x104)=0x3344;MEM32(0x108)=0x5566;MEM32(0x10c)=0x7788;MEM32(0x110)=0x9900;
    jumping=1;
    switch(setjmp(*recomp_setjmp_slot(0x100))) {
    case 0:(void)tsfp_guest_callback_one(&ops,0x4444,0x2468);CHECK(0);break;
    case 7:break;
    default:CHECK(0);
    }
    CHECK(called==3 && g_ebp==0x1122 && g_ebx==0x3344 && g_edi==0x5566 && g_esi==0x7788 && g_esp==0x9904);
    puts("PASS: bounded callback frame, complete project register preservation, malformed cleanup and native guest jump");return 0;
}
