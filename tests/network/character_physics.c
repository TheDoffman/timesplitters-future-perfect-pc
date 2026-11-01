/* Production physics adapters with independent peer-local handles/clip slots. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "pc_physical_state.h"
#include "pc_network_bots_state.h"
#ifdef TEST_NATIVE
#undef _WIN32
#define TSFP_NATIVE_NETWORK 1
#elif !defined(_WIN32)
#define _WIN32 1
#endif
static uint8_t memory[0x800000];static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp;
#define WORD_AT(a) (*(uint32_t *)(memory+(a)))
static int guest_writable(uint32_t a,size_t n){return a>=0x1000 && a<sizeof memory && n<=sizeof memory-a;}
enum {BODY=0x20000,ACTOR=0x30000,GROUP=0x40000,PART=0x50000,POOL=0x60000,OWNERS=0x68000};
static unsigned calls,created,released,evaluated,fail_allocate,posed,paused,baked;
void pc_character_pose_present(uint32_t e,const PcCharacterPose *p){assert(e==BODY && p->clip==74);posed++;}
static volatile unsigned marker;
#define STUB(f) void f(void){marker=__COUNTER__+1u;}
STUB(sub_00170ED0) STUB(sub_00165D80) STUB(sub_00165A40) STUB(sub_00167CA0) STUB(sub_00167CE0) STUB(sub_00167BC0)
STUB(sub_00351580) STUB(sub_003515A0) STUB(sub_00352200) STUB(sub_00351F60) STUB(sub_003513C0) STUB(sub_001677E0) STUB(sub_000B2E50)
static uint32_t guest_call(void (*fn)(void),unsigned n,...)
{
    uint32_t a[3]={0};va_list ap;va_start(ap,n);assert(n<=3);
    for(unsigned i=0;i<n;i++)a[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_00351580) return fail_allocate?UINT32_MAX:230;
    if(fn==sub_003515A0){assert(a[0]==230);released++;return 0;}
    if(fn==sub_00352200){assert(a[0]==BODY && a[1]==230 && a[2]==4);created++;
        WORD_AT(GROUP)=230;WORD_AT(GROUP+4)=4;
        for(unsigned i=0;i<3;i++)WORD_AT(GROUP+8+i*4)=PART+i*0x300;
        return GROUP;}
    if(fn==sub_00351F60){assert(a[0]==ACTOR+0x4c);released++;WORD_AT(ACTOR+0x4c)=0;return 0;}
    if(fn==sub_003513C0){assert(a[0]==BODY && a[1]==GROUP);
        for(unsigned i=1;i<=3;i++)assert((int8_t)memory[POOL+i*512+0x1c8]<0);
        evaluated++;return 0;}
    if(fn==sub_001677E0){const unsigned slot=a[0]&65535u;assert(slot>=1 && slot<=3);memory[POOL+slot*512+0x1c8]=255;paused++;return 0;}
    if(fn==sub_000B2E50){assert(n==3 && a[0]==0x70000 && a[1]==GROUP && a[2]==0);assert(!released);
        WORD_AT(GROUP)=236;WORD_AT(0x732a18+4*96+0x20)=236;baked++;return 0;}
    if(fn==sub_00170ED0){assert(a[0]==PART);WORD_AT(PART+0x80)=0;return 0;}
    const unsigned slot=a[0]&65535u;assert(slot>=1 && slot<=3);uint32_t record=POOL+slot*512;
    assert((a[0]>>16)==9);calls++;
    if(fn==sub_00167CE0 || fn==sub_00167BC0) memory[record+0x1c8]=1;
    if(fn==sub_00165D80){
        assert(n==3 && a[2]==0);
        float *m=(float *)(memory+a[1]);assert(m[3]==0 && m[7]==0 && m[11]==0 && m[15]==1);
        memcpy(memory+record,m,64);
    } else if(fn==sub_00167CA0)memcpy(memory+record+0x140,memory+a[1],12);
    else if(fn==sub_00167CE0)memcpy(memory+record+0x14c,memory+a[1],12);
    else if(fn==sub_00167BC0)memcpy(memory+record+0x128,memory+a[1],12);
    else if(fn!=sub_00165A40)assert(0);
    return 0;
}
/* PRODUCTION_PHYSICS */
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;g_esp=0x18000;
    WORD_AT(0x74c458)=POOL;WORD_AT(0x74c46c)=OWNERS;
    WORD_AT(BODY+0x20)=8;WORD_AT(BODY+0x7c)=ACTOR;WORD_AT(BODY+0x258)=123;
    WORD_AT(ACTOR)=BODY;WORD_AT(ACTOR+8)=2;WORD_AT(0x53fa58+4*0x4dc)=3;
    for(unsigned i=0;i<3;i++) {
        uint32_t part=PART+i*0x300,slot=i+1,record=POOL+slot*512,handle=(9u<<16)|slot;
        WORD_AT(part+0x80)=handle;WORD_AT(OWNERS+slot*4)=part;WORD_AT(record+0x1e4)=handle;
        float *rotation=(float *)(memory+record);rotation[0]=rotation[5]=rotation[10]=rotation[15]=1;
        /* Recorded weapon25 inertia: this is deliberately not a rotation. */
        float *inertia=(float *)(memory+record+0x80);inertia[0]=11.19056f;inertia[5]=13.88609f;inertia[10]=29.26760f;inertia[15]=1;
        ((float *)(memory+record+0x140))[0]=(float)i+5;
        ((float *)(memory+record+0x14c))[1]=2.5f;
        ((float *)(memory+record+0x128))[2]=.75f;
    }
    PcNetworkBotState bot={0};bot.body_present=1;bot.clip=74;
    pc_network_bots_present(BODY,&bot);assert(posed==1);
    PcRigidBodyState rigid;pc_rigid_body_capture(&rigid,PART);
    assert(rigid.present && rigid.position[0]==5 && rigid.velocity[1]==2.5f && rigid.angular[2]==.75f);
    rigid.rotation[0]=0;rigid.rotation[1]=1;rigid.rotation[3]=-1;rigid.rotation[4]=0;
    int result=pc_rigid_body_present(PART,&rigid);
    if(!result || calls!=5 || g_esp!=0x18000) fprintf(stderr,"physics present=%d calls=%u stack=%x valid=%d record=%x\n",result,calls,g_esp,pc_rigid_body_valid(&rigid),pc_rigid_body_record(PART));
    assert(result && calls==5 && g_esp==0x18000);
    assert(((float *)(memory+POOL+512+0x80))[0]==11.19056f);
    PcRigidBodyState copy;pc_rigid_body_capture(&copy,PART);assert(!memcmp(&copy,&rigid,sizeof copy));
    WORD_AT(POOL+512+0x1e4)^=0x10000;pc_rigid_body_capture(&copy,PART);assert(!copy.present);
    assert(!pc_rigid_body_present(PART,&rigid) && calls==5);WORD_AT(POOL+512+0x1e4)^=0x10000;
    WORD_AT(OWNERS+4)=BODY;assert(!pc_rigid_body_present(PART,&rigid));WORD_AT(OWNERS+4)=PART;
    rigid.rotation[0]=NAN;assert(!pc_rigid_body_valid(&rigid));rigid.rotation[0]=0;
    rigid.rotation[8]=0;assert(!pc_rigid_body_valid(&rigid));rigid.rotation[8]=1;
    PcRagdollState rag={0};rag.kind=4;rag.count=3;
    for(unsigned i=0;i<3;i++)pc_rigid_body_capture(rag.parts+i,PART+i*0x300);
    assert(pc_ragdoll_valid(&rag));
    assert(pc_character_ragdoll_clip(BODY,229)==-1); /* Never bind host dynamic slot. */
    fail_allocate=1;pc_character_ragdoll_present(BODY,&rag);assert(!created && !WORD_AT(ACTOR+0x4c));
    fail_allocate=0;pc_character_ragdoll_present(BODY,&rag);assert(created==1 && evaluated==1 && calls==20);
    assert(paused==3);
    float y=((float *)(memory+POOL+512+0x140))[1];
    for(unsigned tick=0;tick<60;tick++) if((int8_t)memory[POOL+512+0x1c8]>0) ((float *)(memory+POOL+512+0x140))[1]-=1.f;
    assert(((float *)(memory+POOL+512+0x140))[1]==y);
    assert(pc_character_ragdoll_clip(BODY,229)==230 && pc_character_ragdoll_clip(BODY,231)==230);
    assert(pc_character_ragdoll_clip(BODY,74)==74);
    pc_character_ragdoll_present(BODY,&rag);assert(created==1 && evaluated==2 && calls==35);
    PcRagdollState captured;pc_character_ragdoll_capture(&captured,BODY);assert(!memcmp(&captured,&rag,sizeof rag));
    rag.count=8;assert(!pc_ragdoll_valid(&rag));rag.count=3;
    WORD_AT(0x70020)=0x1000;WORD_AT(0x7007c)=0x732a18+4*96;
    WORD_AT(0x732a10)=8;WORD_AT(0x7b2e20+4*4)=0x70000;
    pc_character_ragdoll_preserve(BODY,0);assert(!baked);
    pc_character_ragdoll_preserve(BODY,0x70000);assert(baked==1 && released==1 && !WORD_AT(ACTOR+0x4c));
    pc_character_ragdoll_preserve(BODY,0x70000);assert(baked==1 && released==1);
    assert(WORD_AT(0x732a18+4*96+0x20)==236 && pc_character_ragdoll_clip(BODY,229)==-1);
    PcRagdollState absent={0};pc_character_ragdoll_present(BODY,&absent);assert(released==1 && !WORD_AT(ACTOR+0x4c));
    pc_character_ragdoll_present(BODY,&absent);assert(released==1);
    pc_character_ragdoll_present(BODY,&rag);assert(created==2 && WORD_AT(0x732a18+4*96+0x20)==236);
    WORD_AT(BODY+0x258)++;unsigned before=calls;
    assert(pc_character_ragdoll_clip(BODY,229)==-1);
    pc_character_ragdoll_present(BODY,&rag);assert(created==2 && calls==before);
    pc_character_ragdoll_present(BODY,&absent);assert(released==1); /* Reused/unowned storage untouched. */
    pc_rigid_body_remove(PART);assert(!WORD_AT(PART+0x80));
    pc_rigid_body_remove(PART);
    puts("PASS: rigid rotation/velocity, local handles and rag clips, allocation retry, generation/owner validation, cleanup and reuse");return 0;
}
