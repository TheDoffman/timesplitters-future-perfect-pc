/* Execute the original Gladiator reassignment, host update and guest receiver. */
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
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s line %d\n",#x,__LINE__);exit(1);}}while(0)
#define POOL 0x10000u
#define STACK 0x70000u
#define EVENT 0x71000u
#define WIRE 0x72000u
static int enabled=1,selections,sends,wire_bytes,legacy_writes,legacy_reads;
static int pc_network_enabled(void){return enabled;}
static int pc_story_network_actor_active(uint32_t a,unsigned b,uint32_t c){(void)a;(void)b;(void)c;return 0;}
static uint32_t pc_story_network_wire_entity(unsigned a){(void)a;return 0;}
static uint32_t pc_story_network_world_key(uint32_t a){(void)a;return 0;}
static uint32_t pc_story_network_world_entity(uint32_t a){(void)a;return 0;}
void stock_active(void);
/* PRODUCTION_ADAPTER */
void sub_003568C0(void){pc_network_bots_guest_active(stock_active);}
void sub_003533F0(void){g_eax=MEM32(0x76b108);g_esp+=4;}
void sub_00355EC0(void){g_eax=MEM32(0x76b140)==1;g_esp+=4;}
void sub_00153B40(void){g_eax=0;g_esp+=4;}
void sub_003C848C(void){g_eax=(uint32_t)(int32_t)g_fp_stack[g_fp_top];g_fp_top=(g_fp_top+1)&7;g_esp+=4;}
void sub_000D6C90(void){CHECK(!"unexpected non-Gladiator callback");}
void sub_000B91F0(void){selections++;g_esp+=4;}
void sub_00120FF0(void){g_esp+=4;}
void sub_00120F70(void){g_esp+=4;}
static void stock_encode(void){
    uint32_t cursor=MEM32(g_eax),slot=MEM32(MEM32(g_ecx+0x7c)+8);
    CHECK(slot<2);MEM8(cursor)=1;MEM8(cursor+1)=slot;MEM32(g_eax)+=2;legacy_writes++;g_esp+=4;
}
void sub_00359400(void){pc_network_bots_guest_encode(stock_encode);}
static void stock_decode(void){
    uint32_t cursor=MEM32(g_ecx);CHECK(MEM8(cursor)==1 && MEM8(cursor+1)<2);
    g_eax=MEM32(POOL+MEM8(cursor+1)*0xea0u);MEM32(g_ecx)+=2;legacy_reads++;g_esp+=4;
}
void sub_003594B0(void){pc_network_bots_guest_decode(stock_decode);}
void sub_00353990(void){
    CHECK(MEM32(g_esp+8)==0x28);uint32_t data=MEM32(g_esp+12);wire_bytes=MEM32(g_esp+16);
    /* The title's common four-byte prefix precedes the two-byte actor ref. */
    CHECK(wire_bytes==6);memcpy((void *)(g_xbox_mem_offset+WIRE),(void *)(g_xbox_mem_offset+data),6);
    sends++;g_esp+=4;
}
void sub_00064350(void);void sub_0027C2F0(void);void sub_0035A5C0(void);void sub_0035A670(void);
/* ORIGINAL_ROUTINES */
static uint32_t actor(unsigned slot){return POOL+slot*0xea0u;}
static uint32_t body(unsigned slot){return 0x30000u+slot*0x300u;}
static void call(void (*fn)(void)){
    g_esp=STACK;g_ebx=11;g_esi=22;g_edi=33;g_ebp=44;fn();
    CHECK(g_esp==STACK+4 && g_ebx==11 && g_esi==22 && g_edi==33 && g_ebp==44);
}
static void assign(uint32_t victim,uint32_t killer){MEM32(EVENT)=victim;MEM32(EVENT+4)=killer;MEM32(STACK+4)=EVENT;call(sub_0027C2F0);}
static void receive(uint32_t previous){
    MEM32(0x76b140)=2;MEM32(0x7de320)=previous;MEM32(STACK+8)=WIRE+4;
    call(sub_0035A670);CHECK(MEM32(STACK+8)==WIRE+6);MEM32(0x76b140)=1;
}
int main(void){
    void *ram=calloc(1,0x800000);CHECK(ram);g_xbox_mem_offset=(ptrdiff_t)ram;
    /* CONSTANTS */
    MEM32(0x7356d8)=POOL;MEM8(0x7de400)=2;MEM8(0x7de402)=2;
    MEM32(0x790950)=2;MEM32(0x7a2958)=2;MEM32(0x76b108)=1;MEM32(0x76b140)=1;
    MEM8(0x7de456)=0x12;MEM32(0x79094c)=1;MEM32(0x7b0c48)=0x73000;MEM32(0x73014)=body(0);
    for(unsigned i=0;i<4;i++){
        MEM32(actor(i))=body(i);MEM32(actor(i)+8)=i;MEM32(actor(i)+0xa68)=i;
        MEM32(body(i)+0x7c)=actor(i);MEM32(body(i)+0x20)=8;MEM32(body(i)+0x1d8)=100;
        if(i<2)MEM8(0x77728c+i*0x124)=1;
    }
    /* Host or guest human suicide; the original selector chooses the bot
       with the highest score. It must remain selected across host updates. */
    for(unsigned human=0;human<2;human++){
        MEM32(0x7de320)=body(human);assign(body(human),body(human));
        CHECK(MEM32(0x7de320)==body(3));selections=sends=0;
        for(unsigned tick=0;tick<120;tick++)call(sub_0035A5C0);
        CHECK(selections==0 && sends==120 && MEM32(0x7de320)==body(3));
        CHECK(MEM8(WIRE+4)==4 && MEM8(WIRE+5)==3);receive(body(human));
        CHECK(MEM32(0x7de320)==body(3) && MEM32(body(3)+0x1cc)==100);
    }
    /* A bot's own suicide must still trigger the original replacement rule. */
    MEM32(0x7de320)=body(3);assign(body(3),body(3));CHECK(MEM32(0x7de320)==body(2));
    selections=0;for(unsigned tick=0;tick<60;tick++)call(sub_0035A5C0);
    CHECK(selections==0 && MEM32(0x7de320)==body(2));receive(body(3));CHECK(MEM32(0x7de320)==body(2));
    /* Ordinary lethal kills transfer ownership to a human; controller
       membership and the existing human serialization path remain intact. */
    assign(body(2),body(1));CHECK(MEM32(0x7de320)==body(1));
    selections=0;call(sub_0035A5C0);CHECK(selections==0 && legacy_writes==1);
    receive(body(2));CHECK(MEM32(0x7de320)==body(1) && legacy_reads==1);
    /* A disconnected human remains ineligible and still gets replaced. */
    MEM8(0x77728c+0x124)=0;selections=0;call(sub_0035A5C0);
    CHECK(selections==1 && MEM32(0x7de320)==body(3));
    /* Bot wins a human's role through a normal kill; guests never originate
       the host's selection/update messages. */
    MEM8(0x77728c+0x124)=1;MEM32(0x7de320)=body(1);assign(body(1),body(3));
    selections=0;call(sub_0035A5C0);CHECK(selections==0 && MEM32(0x7de320)==body(3));
    MEM32(0x76b140)=2;int before=sends;call(sub_0035A5C0);CHECK(sends==before && selections==0);
    MEM32(0x76b140)=1;
    /* Human-only matches retain the original selection and membership rules. */
    MEM8(0x7de402)=0;MEM32(0x7a2958)=0;
    MEM32(0x7de320)=body(0);assign(body(0),body(0));CHECK(MEM32(0x7de320)==body(1));
    selections=0;call(sub_0035A5C0);CHECK(selections==0 && MEM32(0x7de320)==body(1));
    for(unsigned i=2;i<4;i++)CHECK(MEM8(0x77728c+i*0x124)==0);
    free(ram);puts("PASS: original Gladiator suicide selection, stable bot ownership, host publication and guest receiver");return 0;
}
