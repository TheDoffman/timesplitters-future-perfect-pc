#define main queue_fixture_main
#include "io_queue_contract.c"
#undef main
#include "guest_io_queue_wire.h"
#include "guest_device_body.h"
static unsigned char device_memory[74],saved_device[72],saved_packet[8][128];
static tsfp_io_queue_wire wire;
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
static unsigned char *device(void) {return device_memory+1;}
static void setup(void) {
    unsigned i;init();memset(device_memory,0xa5,sizeof(device_memory));
    CHECK(tsfp_device_body_initialize(device(),72,0x6001,0x8000,0,0,0,0));
    memcpy(saved_device,device(),72);
    CHECK(tsfp_io_queue_wire_init(&wire,&queue,device(),72,0x6001));
    for(i=0;i<8;++i) {
        begin(i);put(packets[i].bytes+0x44,0x77889900+i);
        put(packets[i].bytes+0x4c,0x11223300+i);put(packets[i].bytes+0x50,0xaabbcc00+i);
        memset(packets[i].bytes+0x49,0x55,3);memcpy(saved_packet[i],packets[i].bytes,128);
    }
}
static void mirrors(unsigned current,const unsigned *pending,unsigned count) {
    unsigned i;unsigned char *p=device();uint32_t sentinel=0x602d;
    CHECK(word(p+0x10)==(current<8?at(current)->address:0));
    CHECK(p[0x2b]==(current<8 || count));
    CHECK(word(p+0x2c)==(count?at(pending[0])->address+0x3c:sentinel));
    CHECK(word(p+0x30)==(count?at(pending[count-1])->address+0x3c:sentinel));
    for(i=0;i<count;++i) {
        const unsigned char *entry=packets[pending[i]].bytes;
        CHECK(entry[0x48]==1 && at(pending[i])->waiting_queue==&queue);
        CHECK(word(entry+0x3c)==(i+1<count?at(pending[i+1])->address+0x3c:sentinel));
        CHECK(word(entry+0x40)==(i?at(pending[i-1])->address+0x3c:sentinel));
    }
    for(i=0;i<72;++i)if(!(i>=0x10 && i<0x14) && !(i>=0x2b && i<0x34))CHECK(p[i]==saved_device[i]);
    CHECK(device_memory[0]==0xa5 && device_memory[73]==0xa5);
}
static void preserved(void) {
    unsigned i,j;
    for(i=0;i<8;++i)for(j=0;j<128;++j)
        if(!(j>=0x3c && j<0x44) && j!=0x48)CHECK(packets[i].bytes[j]==saved_packet[i][j]);
}
static void transitions(void) {
    tsfp_io_owner *selected,*old,*queued=(tsfp_io_owner *)1;unsigned pending[3]={1,2,3};
    setup();mirrors(8,NULL,0);
    CHECK(tsfp_io_queue_submit(&queue,at(0),&selected) && selected==at(0) && at(0)->clients==3);
    CHECK(!at(0)->waiting_queue && !packets[0].bytes[0x48]);mirrors(0,NULL,0);
    CHECK(!tsfp_io_queue_submit(&queue,at(0),&queued) && queued==(tsfp_io_owner *)1);
    CHECK(tsfp_io_queue_submit(&queue,at(1),&queued) && !queued);
    CHECK(tsfp_io_queue_submit(&queue,at(2),&queued) && !queued);
    CHECK(tsfp_io_queue_submit(&queue,at(3),&queued) && !queued);mirrors(0,pending,3);
    CHECK(!tsfp_io_queue_submit(&queue,at(4),&queued));
    old=selected;CHECK(tsfp_io_queue_next(&queue,old,&selected) && selected==at(1));
    CHECK(tsfp_io_owner_release(old));CHECK(!at(1)->waiting_queue && !packets[1].bytes[0x48]);
    CHECK(tsfp_io_queue_submit(&queue,at(4),&queued) && !queued);
    pending[0]=2;pending[1]=3;pending[2]=4;mirrors(1,pending,3);
    /* Guest corruptions cannot redirect traversal or confer membership. */
    put(device()+0x2c,0xffffffff);put(packets[2].bytes+0x3c,0xdeadbeef);
    old=selected;CHECK(tsfp_io_queue_next(&queue,old,&selected) && selected==at(2));
    CHECK(tsfp_io_owner_release(old));CHECK(!packets[2].bytes[0x48]);
    CHECK(tsfp_io_queue_submit(&queue,at(5),&queued) && !queued);
    pending[0]=3;pending[1]=4;pending[2]=5;mirrors(2,pending,3);
    preserved();tsfp_io_queue_stop(&queue);mirrors(2,NULL,0);
    CHECK(!packets[3].bytes[0x48] && !packets[4].bytes[0x48] && !packets[5].bytes[0x48]);
    CHECK(!at(3)->waiting_queue && !at(4)->waiting_queue && !at(5)->waiting_queue);
    CHECK(tsfp_io_owner_release(selected));finish();mirrors(8,NULL,0);
}
static void membership(void) {
    tsfp_io_queue other;tsfp_io_owner *storage[2],*selected=(tsfp_io_owner *)1;unsigned p[1]={0};
    tsfp_io_queue_wire other_wire;unsigned char other_device[72];tsfp_io_owner *empty=(tsfp_io_owner *)1;
    setup();CHECK(tsfp_io_queue_init(&other,storage,2,&queue,qlock,qunlock));
    CHECK(tsfp_device_body_initialize(other_device,72,0x7000,0x8000,0,0,0,0));
    CHECK(tsfp_io_queue_wire_init(&other_wire,&other,other_device,72,0x7000));
    CHECK(tsfp_io_queue_push(&queue,at(0)));mirrors(8,p,1);
    CHECK(!tsfp_io_queue_push(&other,at(0)) && at(0)->clients==2);
    CHECK(!tsfp_io_queue_submit(&other,at(0),&selected) && selected==(tsfp_io_owner *)1);
    CHECK(!tsfp_io_queue_submit(&queue,at(1),&selected)); /* idle waiting work from push */
    CHECK(tsfp_io_queue_next(&queue,NULL,&selected) && selected==at(0));
    CHECK(tsfp_io_queue_push(&other,at(0))); /* upper current may wait below */
    CHECK(packets[0].bytes[0x48]==1 && word(packets[0].bytes+0x3c)==0x702c);
    tsfp_io_queue_stop(&queue);
    CHECK(tsfp_io_queue_next(&queue,selected,&empty) && !empty);
    /* Publishing/retiring the upper current must not clobber lower membership. */
    CHECK(at(0)->waiting_queue==&other && packets[0].bytes[0x48]==1);
    CHECK(word(packets[0].bytes+0x3c)==0x702c && word(packets[0].bytes+0x40)==0x702c);
    tsfp_io_queue_stop(&other);CHECK(!at(0)->waiting_queue && at(0)->clients==2);
    CHECK(tsfp_io_owner_release(selected));preserved();finish();
}
static void rejection(void) {
    tsfp_io_queue q_before;tsfp_io_queue_wire before;unsigned char bytes[72];tsfp_io_owner *selected=(tsfp_io_owner *)1;
    init();CHECK(tsfp_device_body_initialize(device(),72,0x6001,0x8000,0,0,0,0));
    memset(&wire,0xa5,sizeof(wire));memcpy(&before,&wire,sizeof(wire));memcpy(&q_before,&queue,sizeof(queue));memcpy(bytes,device(),72);
    CHECK(!tsfp_io_queue_wire_init(&wire,&queue,device(),71,0x6001));
    CHECK(!tsfp_io_queue_wire_init(&wire,&queue,device(),72,0xfffffff0));
    CHECK(!memcmp(&q_before,&queue,sizeof(queue)) && !memcmp(&before,&wire,sizeof(wire)) && !memcmp(bytes,device(),72));
    CHECK(!tsfp_io_queue_submit(&queue,at(0),&selected) && selected==(tsfp_io_owner *)1);
    begin(0);at(0)->clients=UINT32_MAX-3;
    CHECK(!tsfp_io_queue_submit(&queue,at(0),&selected) && !queue.current && at(0)->clients==UINT32_MAX-3);
    at(0)->clients=1;finish();
}
int main(void) {
    unsigned i;
#ifdef _WIN32
    InitializeCriticalSection(&queue_mutex);for(i=0;i<8;++i)InitializeCriticalSection(&owner_mutex[i]);
#else
    CHECK(!pthread_mutex_init(&queue_mutex,NULL));for(i=0;i<8;++i)CHECK(!pthread_mutex_init(&owner_mutex[i],NULL));
#endif
    transitions();membership();rejection();
#ifdef _WIN32
    for(i=0;i<8;++i)DeleteCriticalSection(&owner_mutex[i]);DeleteCriticalSection(&queue_mutex);
#else
    for(i=0;i<8;++i)CHECK(!pthread_mutex_destroy(&owner_mutex[i]));CHECK(!pthread_mutex_destroy(&queue_mutex));
#endif
    puts("PASS: atomic current/queued submission, exclusive waiting membership, host-authored guest links, unaligned mirrors, corrupt-link resistance and unchanged opaque fields");return 0;
}
