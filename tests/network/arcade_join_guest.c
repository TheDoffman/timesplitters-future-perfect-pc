/* Run the real lazy-context wrapper with an engine constructor fixture. */
#define main checkpoint_fixture_main
#include "arcade_checkpoint.c"
#undef main
#define TSFP_ARCADE_MIGRATION 1
#include "../src/network/pc_online.h"
#include <stdarg.h>
static Fixture world;
ptrdiff_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a)+g_xbox_mem_offset))
static uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp;
typedef struct GuestRegisters {uint32_t a,c,d,sp,b,si,di,bp;} GuestRegisters;
static void save_registers(GuestRegisters *s){*s=(GuestRegisters){g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp};}
static void restore_registers(const GuestRegisters *s){g_eax=s->a;g_ecx=s->c;g_edx=s->d;g_esp=s->sp;g_ebx=s->b;g_esi=s->si;g_edi=s->di;g_ebp=s->bp;}
static void push(uint32_t v){g_esp-=4;WORD_AT(g_esp)=v;}
static int guest_writable(uint32_t a,size_t n){return a<BYTES && n<=BYTES-a;}
static uint32_t guest_call(void (*fn)(void),unsigned count,...){
    GuestRegisters saved;save_registers(&saved);uint32_t args[12];va_list ap;va_start(ap,count);
    for(unsigned i=0;i<count;i++)args[i]=va_arg(ap,uint32_t);va_end(ap);
    for(unsigned i=count;i;i--)push(args[i-1]);push(0x12345u);fn();uint32_t result=g_eax;
    REQUIRE(g_esp==saved.sp-count*4u);restore_registers(&saved);return result;
}
void sub_0015BCA0(void){
    uint32_t body=WORD_AT(g_esp+4u),count=WORD_AT(0x74c37cu);
    WORD_AT(0x7a3580u+count*4u)=WORD_AT(body+0x7cu);WORD_AT(0x74c37cu)=count+1;g_esp+=4;
}
static unsigned removed_bodies;
void sub_0015DDA0(void){
    uint32_t body=WORD_AT(g_esp+4u);REQUIRE(body==0x200000u);
    REQUIRE(WORD_AT(0x790950u)==3 && WORD_AT(0x7a2958u)==6);
    WORD_AT(body+0x28u)|=0x80000000u;
    WORD_AT(WORD_AT(body+0x7cu))=0;
    WORD_AT(0x74c37cu)=8;WORD_AT(0x7a3580u+12u)=0x10000u+8u*0xea0u;
    removed_bodies++;g_esp+=4;
}
PcOnlineMode pc_online_mode(void){return PC_ONLINE_ARCADE;}
int pc_network_arcade_read(void *ctx,uint32_t a,void *out,size_t n){(void)ctx;return read_guest(&world,a,out,n);}
int pc_network_arcade_write(void *ctx,uint32_t a,const void *v,size_t n,int apply){(void)ctx;return write_guest(&world,a,v,n,apply);}
static uint32_t reported[4];
void pc_network_arcade_slots(const uint32_t slots[4]){if(slots)memcpy(reported,slots,sizeof reported);else memset(reported,0,sizeof reported);}
#include "../src/network/pc_arcade_join_guest.inc"

static void construct_slot(unsigned slot){
    const uint32_t ctx=0x40000u+slot*0x1584u,actor=0x10000u+slot*0xea0u,body=0x100000u+slot*0x1000u;
    WORD_AT(ctx)=slot;WORD_AT(ctx+4u)=UINT32_MAX;WORD_AT(ctx+0x14u)=body;
    WORD_AT(body+0x7cu)=actor;WORD_AT(body+0x28u)=0x61u;WORD_AT(body+0x150u)=ctx;
    WORD_AT(actor)=body;WORD_AT(actor+8u)=slot;WORD_AT(actor+0x18u)=1;WORD_AT(actor+0xa6cu)=300u+slot;
}
static void initial_humans(void){
    REQUIRE(WORD_AT(0x790950u)==3 && WORD_AT(0x7a2958u)==6);
    REQUIRE(world.memory[0x7de400u]==3 && world.memory[0x7de402u]==6);
    for(unsigned slot=0;slot<3;slot++)construct_slot(slot);
}
static void allocate(void){
    REQUIRE(WORD_AT(0x790950u)==16);
    WORD_AT(0x7b0c48u)=0x40000u;WORD_AT(0x7b0c78u)=0x80000u;
    REQUIRE(pc_arcade_join_initial_construct(initial_humans));
}
void sub_000D2FD0_gen(void){
    REQUIRE(pc_arcade_join_context_entry());
    REQUIRE(g_edi==3 && !g_esi && g_ebx==0x40000u);
    REQUIRE(WORD_AT(g_esp+0x10u)==3 && WORD_AT(g_esp+0x14u)==3*0x1584u);
    REQUIRE(WORD_AT(0x40000u+3*0x1584u+0xf74u)==0x80000u+3*0x3e0u);
    construct_slot(g_edi);g_ebp=0xeeeeu;g_edi=0xdddd;g_esi=0xcccc;g_ebx=0xbbbb;
    WORD_AT(0x7b0c7cu)=0;WORD_AT(0x7b0cb8u)=0x9999u;
    REQUIRE(pc_arcade_join_context_tail());
}
int main(void){
    init(&world,0x10000u);g_xbox_mem_offset=(ptrdiff_t)world.memory;
    WORD_AT(0x790950u)=3;WORD_AT(0x7a2958u)=6;
    WORD_AT(0x78ad2cu)=11;WORD_AT(0x78ad4cu)=0;WORD_AT(0x4d239cu+8u)=16;
    WORD_AT(0x774028u)=0x774180u;WORD_AT(0x77418cu)=0x777240u;
    for(unsigned slot=0;slot<3u;slot++){
        WORD_AT(0x777244u+slot*0x124u)=0x774180u+slot*0x30cu;WORD_AT(0x77728cu+slot*0x124u)=1;
    }
    uint8_t before[13][0xea0];
    for(unsigned slot=3;slot<16;slot++){
        memset(world.memory+0x10000u+slot*0xea0u,0xa5,0xea0u);
        memcpy(before[slot-3],world.memory+0x10000u+slot*0xea0u,0xea0u);
    }
    world.memory[0x7de427u]=2;
    const uint32_t configs[]={0x7de340u,0x774044u,0x78ad58u};
    for(unsigned i=0;i<3;i++)memcpy(world.memory+configs[i],world.memory+0x7de400u,0xa0u);
    pc_arcade_join_allocate(allocate);
    REQUIRE(WORD_AT(0x790950u)==3 && WORD_AT(0x7a2958u)==6);
    for(unsigned slot=3;slot<16;slot++){
        REQUIRE(!memcmp(before[slot-3],world.memory+0x10000u+slot*0xea0u,0xea0u));
        REQUIRE(!WORD_AT(0x40000u+slot*0x1584u+0x14u));
    }
    /* Original actor registry and AI constructor run after storage reservation. */
    const uint32_t actor=0x10000u+3u*0xea0u,bot=0x200000u;
    WORD_AT(actor)=bot;WORD_AT(actor+0xa6cu)=91;WORD_AT(bot+0x7cu)=actor;WORD_AT(bot+0x28u)=0x61;
    WORD_AT(0x74c37cu)=9;
    for(unsigned i=0;i<9;i++)WORD_AT(0x7a3580u+i*4u)=0x10000u+i*0xea0u;
    for(unsigned i=0;i<6;i++)WORD_AT(0x7a2d00u+i*4u)=bot+i*0x1000u;
    WORD_AT(0x777244u+3u*0x124u)=0x774180u+3u*0x30cu;WORD_AT(0x77728cu+3u*0x124u)=1;
    g_esp=0x3f000;g_ebp=8;g_edi=7;g_esi=6;g_ebx=5;
    WORD_AT(0x7b0c7cu)=0x40000u;WORD_AT(0x7b0cb8u)=0x1234u;
    pc_arcade_join_frame();
    REQUIRE(g_esp==0x3f000 && g_ebp==8 && g_edi==7 && g_esi==6 && g_ebx==5);
    REQUIRE(WORD_AT(0x7b0c7cu)==0x40000u && WORD_AT(0x7b0cb8u)==0x1234u);
    REQUIRE(world.memory[0x7de400u]==4 && world.memory[0x7de402u]==5);
    REQUIRE(WORD_AT(actor)==0x103000u && !WORD_AT(actor+0xa6cu));
    REQUIRE(WORD_AT(actor+0x84u)==2 && WORD_AT(0x777288u+3u*0x124u)==2);
    REQUIRE(WORD_AT(0x10000u+0xa6cu)==300 && WORD_AT(0x7de318u)==3600);
    REQUIRE(WORD_AT(0x74c37cu)==9 && WORD_AT(0x7a3580u+12u)==actor);
    REQUIRE(removed_bodies==1);
    REQUIRE(WORD_AT(0x7a2d00u)==bot+0x1000u && !WORD_AT(0x7a2d00u+20u));
    REQUIRE(reported[0]==16 && reported[1]==15 && reported[2]==0x1f0);
    free(world.memory);puts("PASS: Arcade checkpoint lazy context keeps registry, team and other scores; newcomer starts at zero");return 0;
}
