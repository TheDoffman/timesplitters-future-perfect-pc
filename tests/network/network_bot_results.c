/* Execute the original completed-results builder against disposable actors. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x);exit(1); } } while(0)
static int enabled=1,sends;
static int pc_network_enabled(void) { return enabled; }
/* PRODUCTION_ADAPTER */
static void network_original(void) { g_eax=MEM32(0x76b108);g_esp+=4; }
void sub_003533F0(void) { pc_network_bots_guest_score_network(network_original); }
void sub_003568C0(void) { g_eax=MEM32(g_esp+4)<2;g_esp+=4; }
void sub_00355EC0(void) { g_eax=MEM32(0x76b140)==1;g_esp+=4; }
void sub_00357B00(void) { ++sends;CHECK(MEM8(0x6f0b08+2*0x68)==2);g_esp+=4; }
void sub_003C8960(void) {
    g_eax=MEM32(g_esp+4);
    strncpy((char *)(g_xbox_mem_offset+g_eax),
            (char *)(g_xbox_mem_offset+MEM32(g_esp+8)),MEM32(g_esp+12));
    g_esp+=4;
}
/* ORIGINAL_ROUTINE */
int main(void) {
    void *mem=calloc(1,0x800000);CHECK(mem);g_xbox_mem_offset=(ptrdiff_t)mem;
    MEM32(0x7356d8)=0x10000;MEM8(0x7de400)=2;MEM8(0x7de402)=6;
    MEM32(0x76b108)=1;MEM8(0x7de455)=11;MEM8(0x7741b4)=0x10;
    for(unsigned slot=0;slot<8;slot++) {
        unsigned actor=0x10000+slot*0xea0,body=0x30000+slot*0x300;
        MEM32(actor)=body;MEM32(actor+8)=slot;MEM32(actor+0x84)=slot%2;
        MEM32(actor+0x70)=0x40000+slot*64;
        sprintf((char *)(g_xbox_mem_offset+MEM32(actor+0x70)),"Actor %u",slot);
        MEM16(actor+0x48)=42+slot;MEM16(actor+0xa52)=slot+3;
        MEM16(actor+0xa50)=slot+11;MEM32(actor+0xa68)=slot+1;
        MEM32(actor+0xa6c)=slot+7;
        for(unsigned victim=0;victim<16;victim++)MEM16(actor+0xa10+victim*2)=slot+victim;
        if(slot<2)MEM32(0x777244+slot*0x124)=0x50000; /* Real peer only. */
    }
    for(unsigned guest=0;guest<2;guest++) {
        memset((void *)(g_xbox_mem_offset+0x6f0b08),0,16*0x68);
        MEM32(0x76b140)=guest?2:1;MEM32(0x6f118c)=guest;
        if(guest)for(unsigned slot=0;slot<8;slot++) {
            unsigned row=0x6f0b08+slot*0x68;
            MEM16(row+2)=99;MEM16(row+0x4e)=88; /* Reliable final host packet. */
        }
        g_esp=0x70000;g_ebx=11;g_esi=22;g_edi=33;g_ebp=44;
        sub_00063EB0();
        CHECK(g_esp==0x70004 && g_ebx==11 && g_esi==22 && g_edi==33 && g_ebp==44);
        CHECK(MEM32(0x6f1188)==1 && sends==1);
        for(unsigned slot=0;slot<8;slot++) {
            unsigned row=0x6f0b08+slot*0x68;
            CHECK(MEM8(row)==slot && MEM16(row+0x64)==42+slot);
            CHECK(!strcmp((char *)(g_xbox_mem_offset+row+0x24),(char *)(g_xbox_mem_offset+0x40000+slot*64)));
            CHECK(MEM32(row+0x58)==slot+1 && MEM32(row+0x5c)==slot+7);
            CHECK(MEM16(row+0x22)==slot+11);
            CHECK(MEM16(row+2)==(guest?99:slot));
            CHECK(MEM16(row+0x4e)==(guest?88:slot+3));
        }
        CHECK(MEM32(0x6f0b08+8*0x68)==0); /* Unused slots stay unused. */
    }
    free(mem);puts("PASS: original host/guest bot results, portraits and final packet preservation");
    return 0;
}
