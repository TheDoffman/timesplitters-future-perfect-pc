#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s line %d\n",#x,__LINE__);exit(1);}}while(0)
/* HOST */
#include "network/pc_network_doors.h"
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
#define WORD_AT(a) MEM32(a)
#define DOOR 0x50000u
#define CONTROL 0x60000u
#define STACK 0x70000u
static unsigned char ram[0x800000];
static int guest_writable(uint32_t a,size_t n){return a>=0x10000u && a<sizeof ram && n<=sizeof ram-a;}
enum {PC_ONLINE_ARCADE=1};static unsigned mode=PC_ONLINE_ARCADE;
static unsigned pc_online_mode(void){return mode;}
struct sockaddr_in {unsigned sin_family;struct {uint32_t s_addr;} sin_addr;uint16_t sin_port;};
#define AF_INET 2
#define INVALID_SOCKET (-1)
#define MAX_SOCKETS 2
static uint16_t htons(uint16_t v){return (uint16_t)((v>>8)|(v<<8));}
typedef struct {int socket;unsigned port;} NetSocket;
static NetSocket sockets[MAX_SOCKETS]={{1,7654},{-1,0}};
static struct {uint32_t ip;unsigned char key[8];} peers[16];
static unsigned peer_count=1;static int initialized=1,local_session_registered=1;
static uint32_t local_ip=11;static unsigned char local_session_id[8]={'f','i','x','t','u','r','e','1'};
static int network_lock,locks,packets,lengths[16];static unsigned char wire[16][1024];
static void net_lock(int *lock){(void)lock;CHECK(!locks);locks=1;}
static void net_unlock(int *lock){(void)lock;CHECK(locks);locks=0;}
static int send_packet(NetSocket *s,const char *data,int n,int flags,const struct sockaddr_in *a){
 (void)s;CHECK(locks && !flags && n>0 && n<=668 && a->sin_addr.s_addr==22);
 CHECK(packets<16);lengths[packets]=n;memcpy(wire[packets++],data,n);return n;
}
/* TRANSPORT */
/* GUEST */
/* ORIGINAL_ROUTINES */
#ifdef TSFP_TEST_ORIGINAL_DOORS
RECOMP_TLS uint32_t g_icall_saved_esp,g_icall_dispatch_form;
recomp_func_t recomp_lookup(uint32_t a){(void)a;CHECK(!"unexpected dispatch");return NULL;}
recomp_func_t recomp_lookup_manual(uint32_t a){(void)a;CHECK(!"unexpected manual dispatch");return NULL;}
recomp_func_t recomp_lookup_kernel(uint32_t a){(void)a;CHECK(!"unexpected kernel dispatch");return NULL;}
void recomp_icall_fail_log(uint32_t a){(void)a;CHECK(!"unexpected failed dispatch");}
#endif
static void reset_world(void){
 memset(ram,0,sizeof ram);g_xbox_mem_offset=(ptrdiff_t)ram;g_esp=STACK;
 MEM32(0x76b108)=1;MEM32(0x76b140)=1;MEM32(0x76b1a4)=15;MEM32(0x7de30c)=7;
 MEM8(0x7de400)=2;MEM8(0x7de402)=0;MEM32(0x7de318)=100;
 MEM32(0x732a04)=CONTROL;MEM32(0x732a08)=1;MEM32(0x72f5e8)=DOOR;MEM32(0x7b4fac)=1;
 MEM32(CONTROL)=1;MEM32(CONTROL+4)=0x30303237;MEM32(CONTROL+8)=6;
 MEM32(CONTROL+0xc)=0x400;MEM32(CONTROL+0x58)=0xffffffffu;MEM32(CONTROL+0x44)=0xffffffffu;
 MEM32(DOOR+0x2c)=1;MEM32(DOOR+0x20)=4;MEM32(DOOR+0x7c)=CONTROL;
 MEM32(0x666040+4)=1;MEM32(0x66648c+16)=22;MEM32(0x66648c)=11;
 peers[0].ip=11;memcpy(peers[0].key,local_session_id,8);packets=0;
}
static void deliver(unsigned p){struct sockaddr_in a={0};a.sin_addr.s_addr=11;CHECK(door_control(&a,wire[p],lengths[p]));}
static void frame(void){g_eax=123;g_ecx=234;g_esp=STACK;pc_network_doors_frame();CHECK(g_eax==123 && g_ecx==234 && g_esp==STACK);}
static void test_world(void){
 reset_world();MEM32(CONTROL+0x10)=1;MEM32(CONTROL+0x20)=17;MEMF(CONTROL+0x24)=.4f;
 MEMF(CONTROL+0x74)=1;MEMF(CONTROL+0x38)=2;frame();CHECK(packets==1);
 frame();CHECK(packets==1); /* once per tick */
 MEM32(0x7de318)=114;frame();CHECK(packets==1);
 MEM32(0x7de318)=115;frame();CHECK(packets==2); /* quiet refresh recovers a lost endpoint */
 MEM32(0x76b140)=2;frame();MEM32(CONTROL+0x10)=0;MEMF(CONTROL+0x24)=0;
 MEM32(CONTROL+0xc)=0x40000402u;deliver(0);frame();
 CHECK(MEM32(CONTROL+0x10)==1 && MEMF(CONTROL+0x24)==.4f && MEMF(CONTROL+0x74)==1);
 CHECK(MEM32(CONTROL+0x20)==17 && MEMF(CONTROL+0x38)==2);
 CHECK(MEM32(CONTROL+0xc)==0x402 && (MEM32(DOOR+0x18)&0x42)==0x42);
 /* Wrong level tags, reused world slots and disabled rows never consume into guest objects. */
 for(unsigned guard=0;guard<6;guard++) {
  MEM32(0x7de318)++;uint32_t tick=MEM32(0x7de318);memcpy(wire[0]+20,&tick,4);
  MEMF(CONTROL+0x24)=.2f;MEM32(CONTROL+4)=0x30303237;MEM32(DOOR+0x2c)=1;MEM32(CONTROL+0xc)=0x400;MEM32(CONTROL+8)=6;
  MEM32(DOOR+0x28)=0;MEM32(DOOR+0x7c)=CONTROL;
  if(guard==0)MEM32(CONTROL+4)++;
  if(guard==1)MEM32(DOOR+0x2c)++;
  if(guard==2)MEM32(CONTROL+0xc)|=0x1000000u;
  if(guard==3)MEM32(CONTROL+8)++;
  if(guard==4)MEM32(DOOR+0x28)=0x80000000u;
  if(guard==5)MEM32(DOOR+0x7c)=0;
  deliver(0);frame();CHECK(MEMF(CONTROL+0x24)==.2f);
 }
 /* Losing the match resets queued state before the next scene. */
 MEM32(0x7de30c)=1;frame();CHECK(!door_context.role);
 MEM32(0x76b140)=1;MEM32(0x7de30c)=7;MEM32(CONTROL+4)=0x30303237;MEM32(CONTROL+8)=6;MEM32(CONTROL+0xc)=0x400;MEM32(DOOR+0x7c)=CONTROL;frame();
 CHECK(packets==3); /* host context resumes even with zero bots */
 unsigned before=packets;MEM32(0x732a08)=66;MEM32(0x7de318)++;frame();CHECK(packets==before);
}
static void test_wire(void){
 PcNetworkDoorState s[65],received[65];memset(s,0,sizeof s);
 for(unsigned i=0;i<65;i++){s[i].slot=i;s[i].key=i+1;s[i].kind=6;s[i].phase=1;s[i].progress=.5f;s[i].target=1;}
 const uint32_t target=22;packets=0;pc_network_doors_context(1,0,77);pc_network_doors_targets(&target,1);
 pc_network_doors_send(100,s,65);CHECK(packets==5 && lengths[0]==668 && lengths[4]==68);
 pc_network_doors_context(2,11,77);for(unsigned i=0;i<5;i++)deliver(i);
 CHECK(pc_network_doors_receive(received,65)==65);CHECK(!memcmp(received,s,sizeof s));
 for(unsigned i=0;i<5;i++)deliver(i);CHECK(!pc_network_doors_receive(received,65));
 unsigned char saved[1024];memcpy(saved,wire[0],lengths[0]);
 for(unsigned guard=0;guard<8;guard++){
  memcpy(wire[0],saved,lengths[0]);uint32_t tick=101;memcpy(wire[0]+20,&tick,4);
  struct sockaddr_in a={0};a.sin_addr.s_addr=11;
  if(guard==0)a.sin_addr.s_addr=33;
  if(guard==1)wire[0][8]^=1;
  if(guard==2)wire[0][16]^=1;
  if(guard==3){uint32_t bad=66;memcpy(wire[0]+28,&bad,4);}
  if(guard==4){float bad=NAN;memcpy(wire[0]+28+24,&bad,4);}
  if(guard==5)memcpy(wire[0]+28+sizeof(PcNetworkDoorState),wire[0]+28,sizeof(PcNetworkDoorState));
  if(guard==6){uint32_t bad=17;memcpy(wire[0]+24,&bad,4);}
  CHECK(door_control(&a,wire[0],lengths[0]-(guard==7)));
  CHECK(!pc_network_doors_receive(received,65));
 }
 /* An authenticated later tick is accepted; out-of-order state cannot undo it. */
 memcpy(wire[0],saved,lengths[0]);uint32_t tick=102;memcpy(wire[0]+20,&tick,4);deliver(0);
 CHECK(pc_network_doors_receive(received,65)==16);tick=101;memcpy(wire[0]+20,&tick,4);deliver(0);CHECK(!pc_network_doors_receive(received,65));
 /* Host migration/round reset permits a fresh clock and clears queued state. */
 pc_network_doors_context(0,0,0);CHECK(!door_seen[0]);pc_network_doors_context(2,11,77);
 tick=0xffffffffu;memcpy(wire[0]+20,&tick,4);deliver(0);CHECK(pc_network_doors_receive(received,65)==16);
 tick=0;memcpy(wire[0]+20,&tick,4);deliver(0);CHECK(pc_network_doors_receive(received,65)==16);
 pc_network_doors_context(0,0,0);
}
#ifdef TSFP_TEST_ORIGINAL_DOORS
static void test_original_state_transfer(void){
 reset_world();MEMF(0x475c78)=1.f;
 MEM32(0x359900)=0x3598de;MEM32(0x359904)=0x3598e6;
 MEM32(0x359908)=0x3598e6;MEM32(0x35990c)=0x3598de;
 MEM8(0x71000)=1;
 for(unsigned phase=1;phase<=3;phase+=2){
  MEM8(0x71001)=phase;g_esp=STACK;MEM32(STACK+4)=0x71000;sub_00359880();
  CHECK(g_esp==STACK+4 && MEM32(CONTROL+0x10)==(phase==1?2u:0u) && MEMF(CONTROL+0x24)==(phase==1?1.f:0.f));
 }
 MEM8(0x71001)=2;g_esp=STACK;MEM32(STACK+4)=0x71000;sub_00359880();
 CHECK(g_esp==STACK+4 && MEM32(CONTROL+0x10)==2 && MEMF(CONTROL+0x24)==1);
 MEM8(0x71001)=0;g_esp=STACK;MEM32(STACK+4)=0x71000;sub_00359880();
 CHECK(g_esp==STACK+4 && MEM32(CONTROL+0x10)==0 && MEMF(CONTROL+0x24)==0);
}
#endif
int main(void){
 CHECK(sizeof(PcNetworkDoorState)==40);
#ifdef TSFP_TEST_ORIGINAL_DOORS
 test_original_state_transfer();
#endif
 test_world();test_wire();
 puts("PASS: authoritative doors, zero-bot matches, identity guards, authenticated batches, loss/reorder and resets");return 0;
}
