/* Run the production queue with a deliberately stalled, real DDS reader. */
#include "texture_pack_io.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef volatile LONG Counter;
static int load(Counter *p) {return InterlockedCompareExchange(p,0,0);}
static void store(Counter *p,int v) {InterlockedExchange(p,v);}
static void increment(Counter *p) {InterlockedIncrement(p);}
static void sleep_ms(void) {Sleep(1);}
static DWORD renderer_thread;
static int on_renderer(void) {return GetCurrentThreadId()==renderer_thread;}
#else
#include <pthread.h>
#include <time.h>
typedef int Counter;
static int load(Counter *p) {return __atomic_load_n(p,__ATOMIC_SEQ_CST);}
static void store(Counter *p,int v) {__atomic_store_n(p,v,__ATOMIC_SEQ_CST);}
static void increment(Counter *p) {__atomic_add_fetch(p,1,__ATOMIC_SEQ_CST);}
static void sleep_ms(void) {struct timespec t={0,1000000};nanosleep(&t,NULL);}
static pthread_t renderer_thread;
static int on_renderer(void) {return pthread_equal(pthread_self(),renderer_thread);}
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static Counter gate,reads,allocated,released;
static unsigned char *controlled_read(const char *path,uint32_t w,uint32_t h,uint64_t remaining,TsfpDDS *dds)
{
    CHECK(!on_renderer());increment(&reads);
    while(!load(&gate))sleep_ms();
    unsigned char *p=tsfp_pack_read(path,w,h,remaining,dds);
    if(p)increment(&allocated);
    return p;
}
static void counted_free(void *p) {if(p)increment(&released);free(p);}
#define TSFP_STREAM_READ controlled_read
#define free counted_free
#include "texture_pack_stream.h"
#undef free
typedef struct {uint64_t expected;unsigned calls,valid;} Owner;
static void apply(void *pointer,uint64_t key,const unsigned char *data,const TsfpDDS *dds)
{
    Owner *owner=pointer;
    CHECK(on_renderer() && owner && key==owner->expected);
    owner->calls++;
    if(data){CHECK(dds->width==4 && dds->height==4 && dds->bytes==24);owner->valid++;}
}
static void wait_reads(int target)
{for(unsigned i=0;i<5000 && load(&reads)<target;i++)sleep_ms();CHECK(load(&reads)>=target);}
static void wait_owner(Owner *owner,unsigned target)
{for(unsigned i=0;i<5000 && owner->calls<target;i++){tsfp_stream_pump();sleep_ms();}CHECK(owner->calls==target);}
static unsigned count_ready(void)
{
    unsigned n=0;tsfp_stream_enter();
    for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++)n+=tsfp_stream_jobs[i].state==TSFP_STREAM_READY;
    tsfp_stream_leave();return n;
}
static void put(unsigned char *p,uint32_t x)
{p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24);}
int main(int argc,char **argv)
{
    CHECK(argc==3);
#ifdef _WIN32
    renderer_thread=GetCurrentThreadId();
#else
    renderer_thread=pthread_self();
#endif
    unsigned char dds[152]={0};put(dds,0x20534444);put(dds+4,124);put(dds+12,4);put(dds+16,4);put(dds+28,3);
    put(dds+76,32);put(dds+80,4);put(dds+84,0x31545844);
    FILE *f=fopen(argv[1],"wb");CHECK(f);CHECK(fwrite(dds,1,sizeof dds,f)==sizeof dds);CHECK(!fclose(f));
    Owner owner={1,0,0},other={2,0,0};
    if(!strcmp(argv[2],"off")) {
        CHECK(tsfp_stream_submit(&owner,1,argv[1],4,4,apply)==-1);
        CHECK(load(&reads)==0);CHECK(!remove(argv[1]));puts("PASS: explicit synchronous fallback");return 0;
    }
    CHECK(tsfp_stream_submit(&owner,1,argv[1],4,4,apply)==1);wait_reads(1);
    /* A stalled disk read cannot block pump, a second request or cancellation. */
    tsfp_stream_pump();CHECK(owner.calls==0 && load(&gate)==0);
    CHECK(tsfp_stream_submit(&other,2,argv[1],4,4,apply)==1);
    tsfp_stream_cancel(&owner);
    owner.expected=3;CHECK(tsfp_stream_submit(&owner,3,argv[1],4,4,apply)==1);
    store(&gate,1);wait_owner(&other,1);wait_owner(&owner,1);
    CHECK(owner.valid==1 && other.valid==1); /* Old canceled generation never applied. */
    Owner missing={4,0,0};CHECK(tsfp_stream_submit(&missing,4,"absent-stream-fixture.dds",4,4,apply)==1);
    wait_owner(&missing,1);CHECK(!missing.valid);
    /* Queue capacity/backpressure and bounded completed payloads. */
    store(&gate,0);int before=load(&reads);
    Owner owners[TSFP_STREAM_SLOTS+1]={0};
    for(unsigned i=0;i<TSFP_STREAM_SLOTS+1;i++)owners[i].expected=i+100;
    CHECK(tsfp_stream_submit(&owners[0],100,argv[1],4,4,apply)==1);wait_reads(before+1);
    for(unsigned i=1;i<TSFP_STREAM_SLOTS;i++)CHECK(tsfp_stream_submit(&owners[i],i+100,argv[1],4,4,apply)==1);
    CHECK(tsfp_stream_submit(&owners[TSFP_STREAM_SLOTS],TSFP_STREAM_SLOTS+100,argv[1],4,4,apply)==0);
    store(&gate,1);
    for(unsigned i=0;i<5000 && count_ready()<TSFP_STREAM_READY_LIMIT;i++)sleep_ms();
    CHECK(count_ready()==TSFP_STREAM_READY_LIMIT);
    CHECK(load(&reads)==before+TSFP_STREAM_READY_LIMIT);
    tsfp_stream_pump();unsigned delivered=0;
    for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++)delivered+=owners[i].calls;
    CHECK(delivered==1); /* Only one successful upload per pump/frame. */
    for(unsigned i=0;i<TSFP_STREAM_SLOTS;i++)tsfp_stream_cancel(&owners[i]);
    CHECK(tsfp_stream_submit(&owners[TSFP_STREAM_SLOTS],TSFP_STREAM_SLOTS+100,argv[1],4,4,apply)==1);
    wait_owner(&owners[TSFP_STREAM_SLOTS],1);
    tsfp_stream_shutdown();tsfp_stream_shutdown();
    CHECK(load(&allocated)==load(&released));CHECK(!remove(argv[1]));
    puts("PASS: nonblocking disk queue, owner cancellation, generation safety, bounded staging, frame budget and cleanup");
    return 0;
}
