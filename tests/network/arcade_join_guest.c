/* Run the real lazy-context wrapper with an engine constructor fixture. */
#define main checkpoint_fixture_main
#include "arcade_checkpoint.c"
#undef main
#define TSFP_ARCADE_MIGRATION 1
#include "../src/network/pc_online.h"
#include <stdarg.h>
static Fixture world;
static unsigned fixture_bots=6;
int g_tsfp_arcade_migrating;
static double fixture_time=1.;
double pc_network_bots_time(void){return fixture_time;}
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
static unsigned starts;
static uint32_t expected_go_node=0x774180u+3u*0x30cu;
void sub_00357B80(void){REQUIRE(WORD_AT(g_esp+4u)==expected_go_node);starts++;g_esp+=4;}
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
    REQUIRE(WORD_AT(0x790950u)==3 && WORD_AT(0x7a2958u)==fixture_bots);
    REQUIRE(world.memory[0x7de400u]==3 && world.memory[0x7de402u]==fixture_bots);
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
    for(unsigned i=0;i<9u;i++){world.memory[0x7de414u+i]=12;world.memory[0x7de444u+i]=2;}
    WORD_AT(0x53a3d0u+12u*48u)=7;WORD_AT(0x791d4cu)=0x300000u;
    WORD_AT(0x78b860u)=0xffffu; /* mesh zero is not loaded */
    world.memory[0x7de414u+9u]=103;WORD_AT(0x53a3d0u+103u*48u)=8192;
    world.memory[0x7de414u+10u]=115;world.memory[0x7de444u+10u]=5;
    WORD_AT(0x53a3d0u+115u*48u)=8;WORD_AT(0x78b860u+8u*2u)=1;
    WORD_AT(0x300000u+4u)=0x310000u;WORD_AT(0x300000u+48u+4u)=0x320000u;
    /* A mapped resource index alone is not a loaded model. */
    world.memory[0x7de414u+11u]=116;WORD_AT(0x53a3d0u+116u*48u)=9;
    WORD_AT(0x78b860u+9u*2u)=2;
    const uint32_t configs[]={0x7de340u,0x774044u,0x78ad58u};
    for(unsigned i=0;i<3;i++)memcpy(world.memory+configs[i],world.memory+0x7de400u,0xa0u);
    /* A future owner's pointer can exist before its controller becomes active. */
    WORD_AT(0x774180u+3u*0x30cu)=3;
    WORD_AT(0x777244u+3u*0x124u)=0x774180u+3u*0x30cu;
    WORD_AT(0x66648cu+3u*16u)=0x0300fe0au;
    pc_arcade_join_allocate(allocate);
    REQUIRE(WORD_AT(0x790950u)==3 && WORD_AT(0x7a2958u)==6);
    for(unsigned i=0;i<4u;i++){
        const uint32_t address=i?configs[i-1u]:0x7de400u;
        REQUIRE(world.memory[address+0x14u+9u]==12 && world.memory[address+0x44u+9u]==2);
        REQUIRE(world.memory[address+0x14u+10u]==115 && world.memory[address+0x44u+10u]==5);
        REQUIRE(world.memory[address+0x14u+11u]==12);
    }
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
    WORD_AT(0x774180u+3u*0x30cu)=3;
    WORD_AT(0x777244u+3u*0x124u)=0x774180u+3u*0x30cu;WORD_AT(0x77728cu+3u*0x124u)=1;
    g_esp=0x3f000;g_ebp=8;g_edi=7;g_esi=6;g_ebx=5;
    WORD_AT(0x7b0c7cu)=0x40000u;WORD_AT(0x7b0cb8u)=0x1234u;
    PcArcadeControlState control={0};control.native_count=3;
    for(unsigned i=0;i<3;i++)control.native_peers[i].native_id=i;
    pc_arcade_join_frame(&control);
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
    REQUIRE(starts==1);
    fixture_time+=.1;pc_arcade_join_frame(&control);REQUIRE(starts==1);
    fixture_time+=.5;pc_arcade_join_frame(&control);REQUIRE(starts==2);
    control.native_peers[3].native_id=3;control.native_peers[3].ip=0x0300fe0au;control.native_count=4;
    fixture_time+=1;pc_arcade_join_frame(&control);REQUIRE(starts==2);
    /* Moving a surviving member's native record must not erase their score. */
    const uint32_t moved_node=0x774180u+4u*0x30cu;
    WORD_AT(moved_node)=4;WORD_AT(0x66648cu+4u*16u)=0x0300fe0au;
    WORD_AT(0x777244u+3u*0x124u)=moved_node;WORD_AT(actor+0xa6cu)=17;
    control.native_peers[3].native_id=4;
    fixture_time+=1;pc_arcade_join_frame(&control);REQUIRE(WORD_AT(actor+0xa6cu)==17 && starts==2);
    /* Reusing that native record for a new relay member does start at zero. */
    WORD_AT(0x66648cu+4u*16u)=0x0400fe0au;expected_go_node=moved_node;
    fixture_time+=1;pc_arcade_join_frame(&control);REQUIRE(!WORD_AT(actor+0xa6cu) && starts==3);
    control.native_peers[3].ip=0x0400fe0au;
    fixture_time+=1;pc_arcade_join_frame(&control);REQUIRE(starts==3);
    free(world.memory);
    /* Empty slots are not NPCs: even a nonzero unused-row sentinel must not
       invoke bot retirement or prevent constructing/registering the player. */
    fixture_bots=0;init(&world,0x10000u);g_xbox_mem_offset=(ptrdiff_t)world.memory;
    WORD_AT(0x790950u)=3;WORD_AT(0x7a2958u)=0;
    WORD_AT(0x78ad2cu)=11;WORD_AT(0x78ad4cu)=0;WORD_AT(0x4d239cu+8u)=16;
    WORD_AT(0x774028u)=0x774180u;WORD_AT(0x77418cu)=0x777240u;
    for(unsigned slot=0;slot<3;slot++){
        WORD_AT(0x777244u+slot*0x124u)=0x774180u+slot*0x30cu;WORD_AT(0x77728cu+slot*0x124u)=1;
    }
    WORD_AT(0x774180u+3u*0x30cu)=3;WORD_AT(0x777244u+3u*0x124u)=0x774180u+3u*0x30cu;
    WORD_AT(0x66648cu+3u*16u)=0x0300fe0au;
    /* Storage can be allocated before the resource map exists at all. The
       initial human's selected character will still load for this round. */
    world.memory[0x7de414u]=12;world.memory[0x7de444u]=2;
    world.memory[0x7de417u]=103;WORD_AT(0x791d4cu)=0;
    for(unsigned i=0;i<3;i++)memcpy(world.memory+configs[i],world.memory+0x7de400u,0xa0u);
    pc_arcade_join_allocate(allocate);
    REQUIRE(world.memory[0x7de417u]==12 && world.memory[0x7de447u]==2);
    WORD_AT(actor)=0xdeadbeefu;WORD_AT(actor+0xa6cu)=0x80000001u;
    WORD_AT(0x74c37cu)=3;
    for(unsigned i=0;i<3;i++)WORD_AT(0x7a3580u+i*4u)=0x10000u+i*0xea0u;
    WORD_AT(0x77728cu+3u*0x124u)=1;
    control=(PcArcadeControlState){0};control.native_count=3;
    for(unsigned i=0;i<3;i++)control.native_peers[i].native_id=i;
    expected_go_node=0x774180u+3u*0x30cu;fixture_time+=1;
    pc_arcade_join_frame(&control);
    REQUIRE(world.memory[0x7de400u]==4 && !world.memory[0x7de402u]);
    REQUIRE(WORD_AT(actor)==0x103000u && !WORD_AT(actor+0xa6cu));
    REQUIRE(WORD_AT(0x74c37cu)==4 && WORD_AT(0x7a3580u+12u)==actor && removed_bodies==1);
    free(world.memory);puts("PASS: Arcade checkpoint lazy context keeps registry, team and other scores; newcomer starts at zero");return 0;
}
