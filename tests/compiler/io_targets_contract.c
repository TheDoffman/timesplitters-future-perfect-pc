#include "guest_io_targets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#ifdef _WIN32
#include <windows.h>
#endif
#define CHECK(x) do {if(!(x)){fprintf(stderr,"targets line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static tsfp_guest_objects store;
static tsfp_guest_handle_record handles[8];
static tsfp_guest_object_record records[8];
static tsfp_io_targets targets;
static unsigned char *memory[2];
static unsigned freed,closed,depth,maps,alias,fail_map,wake_mode;
static uintptr_t event_host;
static unsigned reject_release,release_calls;
int tsfp_guest_objects_release_actual(tsfp_guest_object_lease *);
int tsfp_guest_objects_release(tsfp_guest_object_lease *p) {
    ++release_calls;if(reject_release && release_calls==reject_release)return 0;
    return tsfp_guest_objects_release_actual(p);
}
static jmp_buf jump;
static void lock(void *p) {CHECK(p==&targets && !depth);++depth;}
static void unlock(void *p) {CHECK(p==&targets && depth==1);--depth;}
static void *allocate(void) {
#ifdef _WIN32
    void *p=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
#else
    void *p=calloc(1,4096);
#endif
    CHECK(p);return p;
}
static int free_body(void *p,uint32_t address,uint32_t kind,uint32_t flags) {
    unsigned i=address==0x10000?0:1;(void)kind;(void)flags;
    CHECK(p==&store && !depth && address==(i?0x20000u:0x10000u) && memory[i]);
#ifdef _WIN32
    CHECK(VirtualFree(memory[i],0,MEM_RELEASE));
#else
    free(memory[i]);
#endif
    memory[i]=NULL;++freed;return 1;
}
static int close_host(void *p,uintptr_t host) {
    CHECK(p==&store && !depth && host==event_host);
#ifdef _WIN32
    CHECK(CloseHandle((HANDLE)host));
#endif
    ++closed;return 1;
}
static void *output(void *p,uint32_t address,uint32_t bytes) {
    CHECK(p==&targets && !depth);++maps;
    CHECK(bytes==(address==0x10004?8u:16u));
    if(fail_map==maps)return NULL;
    CHECK(address==0x10004 || address==0x20000 || address==0x20010);
    if(alias && address!=0x10004)return memory[0]+4;
    return address==0x10004?memory[0]+4:memory[1]+address-0x20000;
}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
static int wake(void *p,uint32_t address,void *signal,uintptr_t host) {
    tsfp_event_view view;CHECK(p==&targets && !depth && memory[0] && memory[1]);
    CHECK(address==targets.region[1].address && signal==memory[1]+address-0x20000+4);
    CHECK(tsfp_event_bind(&view,memory[1]+address-0x20000,16,address) && tsfp_event_state(&view)==1);
    CHECK(memory[0][4]==0x22 && memory[0][7]==0xc0 && memory[0][8]==0x44 && memory[0][11]==0x11);
    CHECK(!tsfp_io_targets_close(&targets));
    CHECK(tsfp_io_targets_state(&targets)==TSFP_IO_TARGET_SIGNALING);
    if(targets.region[1].kind==TSFP_KIND_EVENT_AUTO || targets.region[1].kind==TSFP_KIND_EVENT_MANUAL)CHECK(host==event_host);
    else CHECK(!host);
    if(wake_mode==2)longjmp(jump,1);
    if(wake_mode==1)return 0;
#ifdef _WIN32
    if(host) {CHECK(SetEvent((HANDLE)host));CHECK(WaitForSingleObject((HANDLE)host,0)==WAIT_OBJECT_0);}
#endif
    return 1;
}
static tsfp_io_result result(void) {
    tsfp_io_result r;memset(&r,0,sizeof(r));r.address=0x30000;
    r.user_iosb=0x10004;r.user_event=targets.region[1].address;
    r.status=0xc0000022;r.information=0x11223344;return r;
}
static void init(unsigned native,unsigned synchronization) {
    tsfp_io_target_region iosb={0},event={0};tsfp_io_targets_ops ops={&targets,lock,unlock,output};
    uint64_t key;uint32_t token=0,winner=0;
    CHECK(!memory[0] && !memory[1] && !depth);
    freed=closed=maps=alias=fail_map=wake_mode=reject_release=release_calls=0;
    memset(&targets,0,sizeof(targets));
    CHECK(tsfp_guest_objects_init(&store,handles,records,8,close_host,free_body,&store));
    memory[0]=allocate();memory[1]=allocate();
    CHECK(tsfp_guest_objects_adopt_body(&store,0x10000,42,0,&iosb.key));
    iosb.base=0x10000;iosb.bytes=4096;iosb.address=0x10004;iosb.kind=42;
    event.base=0x20000;event.bytes=4096;event.address=native?0x20000:0x20010;
    event.kind=native?(synchronization?TSFP_KIND_EVENT_AUTO:TSFP_KIND_EVENT_MANUAL):43;
    if(native) {
#ifdef _WIN32
        event_host=(uintptr_t)CreateEventW(NULL,!synchronization,FALSE,NULL);CHECK(event_host);
#else
        event_host=1234;
#endif
        CHECK(tsfp_guest_objects_adopt(&store,event_host,event.kind,0,&token));
        CHECK(tsfp_guest_objects_reference(&store,token,&key));event.key=key;
        CHECK(tsfp_guest_objects_publish(&store,key,0x20000,&winner) && winner==0x20000);
    } else CHECK(tsfp_guest_objects_adopt_body(&store,0x20000,event.kind,0,&event.key));
    CHECK(tsfp_event_initialize(memory[1]+event.address-event.base,16,event.address,synchronization,0));
    CHECK(tsfp_io_targets_init(&targets,&store,0x30000,&iosb,&event,&ops));
    /* Drop every original owner before pending delivery; only captured refs remain. */
    CHECK(tsfp_guest_objects_dereference(&store,iosb.key));
    CHECK(tsfp_guest_objects_dereference(&store,event.key));
    if(native)CHECK(tsfp_guest_objects_close(&store,token));
    CHECK(!freed && !closed);
}
static void write_iosb(tsfp_io_delivery *d) {CHECK(d->cookie==&targets && d->iosb_bytes==8);put(d->iosb,0xc0000022);put((unsigned char *)d->iosb+4,0x11223344);}
static void successful(unsigned native,unsigned sync) {
    tsfp_io_result r;tsfp_io_delivery d,before;
    init(native,sync);r=result();memset(&d,0xa5,sizeof(d));memcpy(&before,&d,sizeof(d));
    ++r.user_iosb;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d) && !memcmp(&d,&before,sizeof(d)) && !maps);--r.user_iosb;
    ++r.address;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d) && !maps);--r.address;
    CHECK(tsfp_io_targets_prepare(&targets,&r,&d) && maps==2);
    CHECK(!tsfp_io_targets_prepare(&targets,&r,&before) && !tsfp_io_targets_close(&targets));
    CHECK(!tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_DONE));
    CHECK(tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_PRIVATE));
    CHECK(tsfp_io_targets_prepare(&targets,&r,&d));write_iosb(&d);
    CHECK(tsfp_io_targets_signal(&targets,wake,&targets));
    CHECK(!tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_PRIVATE));
    CHECK(!freed && !closed && tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_DONE));
    CHECK(!freed && !closed && tsfp_io_targets_close(&targets));
    CHECK(freed==2 && closed==native && !memory[0] && !memory[1]);
    CHECK(tsfp_io_targets_close(&targets));
}
static void failures(void) {
    tsfp_io_result r;tsfp_io_delivery d,before;unsigned n;
    init(0,0);r=result();memset(&d,0xa5,sizeof(d));memcpy(&before,&d,sizeof(d));
    for(n=1;n<=2;++n) {maps=0;fail_map=n;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));CHECK(!memcmp(&d,&before,sizeof(d)) && !targets.lease[0].owner && !targets.lease[1].owner);}
    fail_map=0;++targets.region[0].kind;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));--targets.region[0].kind;
    ++targets.region[0].base;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));--targets.region[0].base;
    targets.region[0].key+=UINT64_C(0x100000000);CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));targets.region[0].key-=UINT64_C(0x100000000);
    /* Mapping alias is a valid event but overlaps the IOSB. */
    CHECK(tsfp_event_initialize(memory[0]+4,16,0x20010,0,0));alias=1;
    CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));alias=0;
    memory[1][0x12]=16;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));memory[1][0x12]=4;
    CHECK(tsfp_io_targets_close(&targets) && freed==2);
    init(1,1);r=result();memory[1][0]=0;CHECK(!tsfp_io_targets_prepare(&targets,&r,&d));
    CHECK(tsfp_io_targets_close(&targets) && freed==2 && closed==1);
}
static void uncertainty(unsigned mode) {
    tsfp_io_result r;tsfp_io_delivery d;unsigned i;
    init(1,0);r=result();CHECK(tsfp_io_targets_prepare(&targets,&r,&d));write_iosb(&d);wake_mode=mode;
    if(mode==2) {if(setjmp(jump)==0) {(void)tsfp_io_targets_signal(&targets,wake,&targets);CHECK(0);}}
    else CHECK(!tsfp_io_targets_signal(&targets,wake,&targets));
    CHECK(tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_UNCERTAIN));
    CHECK(tsfp_io_targets_state(&targets)==TSFP_IO_TARGET_QUARANTINED && !freed && !closed);
    CHECK(!tsfp_io_targets_close(&targets) && !tsfp_io_targets_signal(&targets,wake,&targets));
    CHECK(!tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_DONE));
    /* Fixture-only recovery after the known injected wake failure/unwind. */
    for(i=0;i<2;++i)CHECK(tsfp_guest_objects_release(&targets.lease[i]));
    CHECK(!freed && !closed);
    for(i=0;i<2;++i)CHECK(tsfp_guest_objects_dereference(&store,targets.region[i].key));
    CHECK(freed==2 && closed==1);
}
static void release_failure(void) {
    tsfp_io_result r;tsfp_io_delivery d;unsigned i;
    init(0,0);r=result();CHECK(tsfp_io_targets_prepare(&targets,&r,&d));
    reject_release=1;CHECK(!tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_PRIVATE));
    CHECK(release_calls==2 && tsfp_io_targets_state(&targets)==TSFP_IO_TARGET_QUARANTINED);
    CHECK(targets.lease[0].owner && !targets.lease[1].owner && !freed);
    CHECK(!tsfp_io_targets_close(&targets) && release_calls==2);
    /* Known test-injection cleanup only; production must retain quarantine. */
    reject_release=0;CHECK(tsfp_guest_objects_release(&targets.lease[0]));
    for(i=0;i<2;++i)CHECK(tsfp_guest_objects_dereference(&store,targets.region[i].key));
    CHECK(freed==2);
}
static void absent_and_validation(void) {
    tsfp_io_targets t,before;tsfp_io_target_region a={0},b={0};
    tsfp_io_targets_ops ops={&targets,lock,unlock,output};tsfp_io_result r;tsfp_io_delivery d;
    init(0,0);memset(&t,0xa5,sizeof(t));memcpy(&before,&t,sizeof(t));
    a=targets.region[0];b=targets.region[1];a.bytes=7;
    CHECK(!tsfp_io_targets_init(&t,&store,0x30000,&a,&b,&ops) && !memcmp(&t,&before,sizeof(t)));
    a=targets.region[0];a.base=0xfffffff0;a.address=0xfffffff4;a.bytes=32;
    CHECK(!tsfp_io_targets_init(&t,&store,0x30000,&a,&b,&ops));
    a=targets.region[0];b.address=a.address;b.base=a.base;
    CHECK(!tsfp_io_targets_init(&t,&store,0x30000,&a,&b,&ops));
    a.address=0;CHECK(!tsfp_io_targets_init(&t,&store,0x30000,&a,&b,&ops));
    a=targets.region[0];b=targets.region[1];++b.key;
    CHECK(!tsfp_io_targets_init(&t,&store,0x30000,&a,&b,&ops));
    CHECK(tsfp_io_targets_state(&t)==TSFP_IO_TARGET_CLOSED && !freed);
    CHECK(tsfp_io_targets_close(&targets) && freed==2);
    init(0,0);r=result();
    /* Absent event is valid preflight, but requires a different delivery route. */
    CHECK(tsfp_guest_objects_dereference(&store,targets.region[1].key));
    memset(&targets.region[1],0,sizeof(targets.region[1]));targets.held[1]=0;r.user_event=0;
    CHECK(tsfp_io_targets_prepare(&targets,&r,&d));CHECK(!tsfp_io_targets_signal(&targets,wake,&targets));
    CHECK(!tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_DONE));
    CHECK(tsfp_io_targets_finish(&targets,TSFP_IO_DELIVERY_PRIVATE));
    CHECK(tsfp_io_targets_close(&targets) && freed==2);
}
static unsigned owner_depth,disposed;
static void owner_lock(void *p) {CHECK(p==&disposed && !owner_depth);++owner_depth;}
static void owner_unlock(void *p) {CHECK(p==&disposed && owner_depth==1);--owner_depth;}
static void dispose(void *p,uint32_t address,void *memory_,size_t bytes) {
    CHECK(p==&disposed && !owner_depth && address==0x30000 && memory_ && bytes==256);++disposed;
}
static void pending(void *p,tsfp_io_owner *owner) {CHECK(p==owner && tsfp_io_owner_pending(owner));}
static int prepare(void *p,const tsfp_io_result *r,tsfp_io_delivery *d) {CHECK(p==&targets && !owner_depth);return tsfp_io_targets_prepare(p,r,d);}
static int notify(void *p,void *cookie,const tsfp_io_result *r) {CHECK(p==cookie && p==&targets && r->information==0x11223344);return tsfp_io_targets_signal(p,wake,p);}
static int release(void *p,void *cookie,int disposition) {CHECK(p==cookie && p==&targets);return tsfp_io_targets_finish(p,disposition);}
static void pipeline(void) {
    tsfp_io_owner owner;tsfp_io_owner_ops ops={&disposed,owner_lock,owner_unlock,dispose};
    tsfp_io_complete_ops completion={&targets,prepare,notify,release};
    tsfp_guest_object_scopes scopes={0};unsigned char packet[256];
    init(1,1);disposed=0;
    CHECK(tsfp_io_owner_init(&owner,&ops,packet,256,0x30000,152,2,6));
    put(packet+0x10,0xc0000022);put(packet+0x14,0x11223344);put(packet+0x1c,0x10004);put(packet+0x20,0x20000);
    CHECK(tsfp_io_owner_dispatch(&owner,pending,&owner));
    CHECK(tsfp_io_complete_request(&scopes,&store,&owner,&completion)==TSFP_IO_COMPLETE_DONE);
    CHECK(disposed==1 && !freed && !closed);
    CHECK(tsfp_io_targets_close(&targets) && freed==2 && closed==1);
}
int main(void) {
    successful(0,0);successful(0,1);successful(1,0);successful(1,1);
    failures();uncertainty(1);uncertainty(2);release_failure();absent_and_validation();pipeline();
    puts("PASS: owned completion spans, preflight preservation, actual disposal, native wake, coordinator IOSB ordering and persistent unwind quarantine");return 0;
}
