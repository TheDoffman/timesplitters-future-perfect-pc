#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define TSFP_NATIVE_NETWORK 1
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t memory[0x800000/4],g_esp,g_eax;
static uintptr_t g_xbox_mem_offset;
#define WORD_AT(a) (*(uint32_t *)(g_xbox_mem_offset+(uint32_t)(a)))
typedef struct GuestRegisters {uint32_t sp,a;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;r->a=g_eax;}
static void restore_registers(const GuestRegisters *r){g_esp=r->sp;g_eax=r->a;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t a){CHECK(n<128 && a==4);return 0x6000;}
enum {PC_ONLINE_NONE,PC_ONLINE_ARCADE,PC_ONLINE_STORY};
static int mode=PC_ONLINE_ARCADE,enabled=1,closed,reset,pumps,match_reset,menus,stock,fallback;
static uint32_t target,caption;
static unsigned recorded,eligibility_checks;
static void sub_000668F0(void){++recorded;}
static int pc_online_mode(void){return mode;}
static int pc_network_enabled(void){++eligibility_checks;return enabled;}
static void pc_online_disconnect(void){++closed;mode=PC_ONLINE_NONE;}
static void sub_00079B70(void){++menus;}
static void sub_001CC360(void){++reset;}
static void sub_001A8D30(void){++stock;g_esp+=4;}
static void sub_001A8F30(void){++stock;g_esp+=4;}
static void sub_003544B0(void){++pumps;WORD_AT(0x76b1a4)=WORD_AT(0x76b140)==1?11:12;}
static void sub_003535C0(void){++match_reset;}
static void sub_002EAF60_gen(void){++stock;}
static void sub_002EB030_gen(void){++stock;}
static void sub_002EBE60(void){++stock;g_esp+=4;}
static void sub_00065470_gen(void){WORD_AT(0x6f11b4)=1;g_esp+=4;}
static void sub_002EC650_gen(void);
static uint32_t guest_call(void (*fn)(void),unsigned n,...){(void)n;fn();return 0;}
static void pc_story_network_result_action(void (*fn)(void),unsigned a){(void)a;++fallback;fn();}
static void pc_story_network_return_lobby(void (*fn)(void)){++fallback;fn();}
static void pc_story_network_disconnect_exit(void (*fn)(void)){++fallback;fn();}
static void pc_story_network_lobby_lost(void (*fn)(void)){++fallback;fn();}
static void pc_story_network_rewards_menu(void (*fn)(void)){++fallback;fn();}
#include "../src/network/pc_multiplayer_exit.inc"
static void disconnect_original(void){WORD_AT(0x6f11b4)=2;g_esp+=4;}
static void sub_002EC650_gen(void){pc_multiplayer_exit_action(sub_00065470_gen,0);}
static void factory(void){target=WORD_AT(g_esp+8);g_esp+=4;}
static void button(void){caption=WORD_AT(g_esp+4);g_esp+=4;}
static void setup(unsigned role){
 uint8_t heap[128];memcpy(heap,(void *)(g_xbox_mem_offset+0x6000),sizeof heap);
 memset(memory,0,sizeof memory);memcpy((void *)(g_xbox_mem_offset+0x6000),heap,sizeof heap);
 mode=PC_ONLINE_ARCADE;enabled=1;
 multiplayer_destination=0;multiplayer_lobby_pending=0;multiplayer_exit_depth=0;
 closed=reset=pumps=match_reset=menus=stock=fallback=0;recorded=0;
 g_esp=0x1800;WORD_AT(0x79094c)=44;WORD_AT(0x76b108)=1;
 WORD_AT(0x76b140)=role;WORD_AT(0x76b1a4)=15;WORD_AT(0x761bbc)=0x3000;
 WORD_AT(0x7613e4)=0x2000;WORD_AT(g_esp+4)=0x4000;WORD_AT(0x418c)=0x40;
 WORD_AT(0x774178)=1;
}
static void build(unsigned want){
 WORD_AT(0x79094c)=101;g_esp=0x1800;WORD_AT(g_esp+4)=0x2000;
 uint32_t incoming=(want==0x4e9574 || want==0x4e92c4)?0x4e92c4:0x4e99d4;
 WORD_AT(g_esp+8)=incoming;
 pc_multiplayer_exit_menu(factory);CHECK(target==want && g_esp==0x1804);
 CHECK(WORD_AT(0x1808)==incoming && !multiplayer_destination);
}
static void save_profile(void){
 CHECK(WORD_AT(0x7bedc0)==0x7bca00 && WORD_AT(0x6f1e0c)==0x7bedc8);
 WORD_AT(0x7bedc4)=99;
}
int main(void){
 for(unsigned i=0;i<1000000;i++)CHECK(!multiplayer_exit_lookup(0x99990u));
 CHECK(!eligibility_checks); /* No guest memory exists yet. */
 g_xbox_mem_offset=(uintptr_t)memory;
 for(unsigned results=0;results<4;++results){
  setup(1);WORD_AT(0x7de30c)=results?8:7;
  if(!results || results==3) {void (*fn)(void)=multiplayer_exit_lookup(0x2ebe60);CHECK(fn);fn();}
  else pc_multiplayer_exit_action(results==1?sub_002EAF60_gen:sub_002EB030_gen,0);
  CHECK(recorded==(!results));
  CHECK(g_esp==0x1804 && !multiplayer_exit_depth && !closed);
  CHECK(multiplayer_destination==1 && WORD_AT(0x6f11b4)==1);
  CHECK(WORD_AT(0x78ad20)==13 && !WORD_AT(0x774178));
  build(0x4e99c4);CHECK(!closed && mode==PC_ONLINE_ARCADE && !reset);
  WORD_AT(0x76b1a4)=17;g_esp=0x1800;
  void (*fn)(void)=multiplayer_exit_lookup(0x1a8d30);CHECK(fn);fn();
  CHECK(pumps==1 && match_reset==1 && !multiplayer_lobby_pending);
 }
 /* A guest's own Leave/Quit and browser Back keep their original callbacks. */
 setup(2);CHECK(!multiplayer_exit_lookup(0x2ebe60));
 for(unsigned result=0;result<2;++result){
  pc_multiplayer_exit_action(result?sub_002EB030_gen:sub_002EAF60_gen,0);
  CHECK(!multiplayer_destination);
 }
 CHECK(stock==2 && fallback==2);
 g_esp=0x1800;WORD_AT(g_esp)=0x2ebe4e;pc_multiplayer_disconnect_exit(disconnect_original);
 CHECK(!multiplayer_destination && !closed);
 g_esp=0x1800;WORD_AT(g_esp)=0x35c360;WORD_AT(0x76b1a4)=18;
 pc_multiplayer_disconnect_exit(disconnect_original);CHECK(!multiplayer_destination && !closed);
 WORD_AT(0x79094c)=101;CHECK(!multiplayer_exit_lookup(0x1a4350));
 setup(2);pc_multiplayer_return_lobby(disconnect_original);
 CHECK(multiplayer_destination==1 && WORD_AT(0x6f11b4)==1 && !menus);
 build(0x4e99d4);CHECK(!closed && !reset);
 WORD_AT(0x76b1a4)=17;g_esp=0x1800;multiplayer_guest_tick();
 CHECK(pumps==1 && match_reset==1 && WORD_AT(0x76b1a4)==12);
 setup(2);multiplayer_destination=1;WORD_AT(g_esp)=0x35c360;pc_multiplayer_disconnect_exit(disconnect_original);
 CHECK(multiplayer_destination==1); /* A retained return keeps its existing destination. */
 setup(2);WORD_AT(g_esp)=0x35c360;pc_multiplayer_disconnect_exit(disconnect_original);
 CHECK(multiplayer_destination==3 && WORD_AT(0x6f11b4)==2);
 build(0x4e92c4);CHECK(closed==1 && reset==1);
 setup(2);WORD_AT(0x79094c)=101;WORD_AT(g_esp)=0x1cc552;
 WORD_AT(g_esp+4)=0;WORD_AT(g_esp+8)=0x8d;
 pc_multiplayer_lobby_lost(disconnect_original);CHECK(multiplayer_destination==3);
 build(0x4e92c4);CHECK(closed==1);
 setup(1);WORD_AT(0x418c)=0x41;multiplayer_leave_confirm();CHECK(stock==1 && !multiplayer_destination);
 for(unsigned role=1;role<=2;++role)for(unsigned ended=0;ended<2;++ended){
  setup(role);WORD_AT(g_esp+20)=ended?0x2ec030:0x2ec060;WORD_AT(g_esp+4)=0x5000;
  CHECK(multiplayer_exit_button(button)==(role==1));
  if(role==1)CHECK(!strcmp((char *)(g_xbox_mem_offset+caption),"Return to Lobby") && WORD_AT(0x1804)==0x5000);
 }
 setup(1);mode=PC_ONLINE_NONE;CHECK(multiplayer_match()); /* System Link */
 mode=PC_ONLINE_STORY;CHECK(!multiplayer_exit_lookup(0x2ebe60));
 pc_multiplayer_exit_action(sub_002EAF60_gen,0);CHECK(fallback==1 && !multiplayer_destination);
 mode=PC_ONLINE_ARCADE;
 for(unsigned level=0;level<=100;++level){WORD_AT(0x79094c)=level;CHECK(multiplayer_match());}
 WORD_AT(0x79094c)=101;CHECK(!multiplayer_match());
 WORD_AT(0x79094c)=2;WORD_AT(0x76b108)=0;CHECK(!multiplayer_match());
 setup(2);WORD_AT(0x74e934)=1;WORD_AT(0x7bedc0)=0x7bca00;
 multiplayer_profiles_capture();CHECK(multiplayer_profile_count==1);
 WORD_AT(0x7bedc0)=0x7bca40;WORD_AT(0x7bedc4)=77;
 WORD_AT(0x7bedc0+0x1f54)=0x7bca00;WORD_AT(0x6f1e0c)=0x7bedc8+0x1f54;
 multiplayer_destination=1;CHECK(multiplayer_profile_save(save_profile));
 CHECK(WORD_AT(0x7bedc0)==0x7bca40 && WORD_AT(0x7bedc4)==77);
 CHECK(WORD_AT(0x7bedc0+0x1f54)==0x7bca00 && WORD_AT(0x7bedc4+0x1f54)==99);
 CHECK(WORD_AT(0x6f1e0c)==0x7bedc8+0x1f54);
 /* Stock frontend saves may follow the lobby factory consuming the route. */
 build(0x4e99d4);CHECK(!multiplayer_destination);
 CHECK(multiplayer_profile_save(save_profile));
 WORD_AT(0x76b108)=0;CHECK(!multiplayer_profile_save(save_profile));
 WORD_AT(0x76b108)=1;mode=PC_ONLINE_STORY;
 CHECK(!multiplayer_profile_save(save_profile));
 puts("PASS: party return, original guest exits, host loss, profile mapping and captions");return 0;
}
