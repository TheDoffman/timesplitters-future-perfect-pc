#include "shared_views.h"
#include "guest_map_layout.h"
#include "guest_arena_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
typedef struct work { tsfp_shared_views *owner;size_t offset,page;unsigned char value; } work;
static void exercise(work *w) {
    for(unsigned i=0;i<40;++i) {
        volatile unsigned char *p=tsfp_shared_views_map(w->owner,NULL,w->offset,w->page,TSFP_VIEW_WRITE);
        CHECK(p);p[0]=w->value;p[w->page-1]=w->value;
        CHECK(tsfp_shared_views_release(w->owner,(void *)p));
    }
}
#if defined(_WIN32)
static DWORD WINAPI thread_main(void *p) { exercise(p);return 0; }
#else
static void *thread_main(void *p) { exercise(p);return NULL; }
#endif
int main(void) {
    tsfp_shared_backend b;size_t page,gran,total;
    tsfp_shared_views s={0},separate={0};tsfp_shared_view views[12],other[1];
    volatile unsigned char *a,*mirror,*tail,*independent;
    work jobs[4];
#if defined(_WIN32)
    HANDLE threads[4];
    MEMORY_BASIC_INFORMATION info;
#else
    pthread_t threads[4];
#endif
    CHECK(tsfp_shared_host_backend(&b,&page,&gran));total=gran*8;
    CHECK(tsfp_shared_views_init(&s,&b,total,page,gran,views,12));
    CHECK(tsfp_shared_views_init(&separate,&b,total,page,gran,other,1));
    a=tsfp_shared_views_map(&s,NULL,0,total,TSFP_VIEW_WRITE);CHECK(a);
    mirror=tsfp_shared_views_map(&s,NULL,0,total,TSFP_VIEW_READ);CHECK(mirror && mirror!=a);
    independent=tsfp_shared_views_map(&separate,NULL,0,total,TSFP_VIEW_WRITE);CHECK(independent);
    for(size_t i=0;i<total;++i)CHECK(!a[i] && !mirror[i] && !independent[i]);
    a[0]=0xa7;a[total-1]=0xc3;
    CHECK(mirror[0]==0xa7 && mirror[total-1]==0xc3 && !independent[0] && !independent[total-1]);
    tail=tsfp_shared_views_map(&s,NULL,gran,page,TSFP_VIEW_WRITE);CHECK(tail);
    tail[0]=0x5a;tail[page-1]=0x69;
    CHECK(a[gran]==0x5a && mirror[gran+page-1]==0x69 && !independent[gran]);
    CHECK(a[0]==0xa7); /* offset mapping must not silently select byte zero */
#if defined(_WIN32)
    CHECK(VirtualQuery((const void *)mirror,&info,sizeof info)==sizeof info);
    CHECK(info.Protect==PAGE_READONLY);
#endif
    CHECK(!tsfp_shared_views_map(&separate,(void *)a,0,total,TSFP_VIEW_WRITE));
    /* Other owner is full; make room and exercise actual OS collision too. */
    CHECK(tsfp_shared_views_release(&separate,(void *)independent));
    CHECK(!tsfp_shared_views_map(&separate,(void *)a,0,total,TSFP_VIEW_WRITE));
    CHECK(a[0]==0xa7 && mirror[total-1]==0xc3 && !separate.live);
    CHECK(!tsfp_shared_views_release(&s,(void *)(a+page)));
    CHECK(tsfp_shared_views_release(&s,(void *)tail));
    CHECK(tsfp_shared_views_release(&s,(void *)mirror));
    for(unsigned i=0;i<4;++i) {
        jobs[i]=(work){&s,(size_t)(i+2)*gran,page,(unsigned char)(0x30+i)};
#if defined(_WIN32)
        threads[i]=CreateThread(NULL,0,thread_main,jobs+i,0,NULL);CHECK(threads[i]);
#else
        CHECK(!pthread_create(threads+i,NULL,thread_main,jobs+i));
#endif
    }
    for(unsigned i=0;i<4;++i) {
#if defined(_WIN32)
        CHECK(WaitForSingleObject(threads[i],15000)==WAIT_OBJECT_0);CHECK(CloseHandle(threads[i]));
#else
        CHECK(!pthread_join(threads[i],NULL));
#endif
        CHECK(a[jobs[i].offset]==jobs[i].value && a[jobs[i].offset+page-1]==jobs[i].value);
    }
    CHECK(s.live==1 && tsfp_shared_views_release(&s,(void *)a));
    CHECK(tsfp_shared_views_stop(&s) && tsfp_shared_views_stop(&separate));
    CHECK(!s.live && !s.backing);
#if !defined(_WIN32)
    {
        tsfp_owned_arena arena={0};tsfp_arena_view records[8];
        tsfp_shared_views owned={0};tsfp_shared_view mapped[3];
        unsigned char *base,*one,*two,*private_view;
        CHECK(tsfp_shared_host_arena_init(&arena,page*32,records,8));
        CHECK(tsfp_shared_host_in_arena(&b,&arena,&page,&gran));
        base=arena.base;
        CHECK(tsfp_shared_views_init(&owned,&b,page*2,page,gran,mapped,3));
        one=tsfp_shared_views_map(&owned,base,0,page*2,TSFP_VIEW_WRITE);CHECK(one==base);
        two=tsfp_shared_views_map(&owned,base+page*4,0,page*2,TSFP_VIEW_WRITE);CHECK(two==base+page*4);
        one[page+7]=0x42;CHECK(two[page+7]==0x42);two[1]=0x35;CHECK(one[1]==0x35);
        private_view=tsfp_owned_arena_map(&arena,base+page*8,4096,0,0,TSFP_ARENA_READ|TSFP_ARENA_WRITE);
        CHECK(private_view==base+page*8 && !private_view[0]);private_view[0]=0x77;
        CHECK(!tsfp_shared_views_map(&owned,private_view,0,page,TSFP_VIEW_WRITE));
        CHECK(private_view[0]==0x77 && one[1]==0x35);
        CHECK(!tsfp_owned_arena_stop(&arena));
        CHECK(tsfp_shared_views_stop(&owned));
        CHECK(tsfp_owned_arena_release(&arena,private_view));
        CHECK(tsfp_owned_arena_stop(&arena));
        CHECK(tsfp_shared_host_arena_init(&arena,page*32,records,8));
        CHECK(tsfp_owned_arena_stop(&arena));
    }
#endif
    {
        tsfp_guest_map_layout layout={0};tsfp_guest_host_session session={0};
        tsfp_layout_host host;
        tsfp_guest_map_config config={0x100000,0x20000,1};
        CHECK(tsfp_guest_map_host(&host,&session));
        CHECK(tsfp_guest_map_init(&layout,&host,&config));
        CHECK(layout.ram && layout.physical && layout.tiled && layout.kernel);
        CHECK(layout.tiled_bytes==config.physical_bytes);
        ((volatile unsigned char *)layout.physical)[0x1000]=0x6b;
        CHECK(((volatile unsigned char *)layout.tiled)[0x1000]==0x6b);
        CHECK(layout.kernel==(unsigned char *)layout.physical+0x10000);
        if(layout.mirrors[0]) {
            ((volatile unsigned char *)layout.mirrors[0])[0]=0x47;
            CHECK(((volatile unsigned char *)layout.ram)[0]==0x47);
        }
#if !defined(_WIN32)
        {
            tsfp_guest_map_layout other_layout={0};tsfp_guest_host_session other_session={0};tsfp_layout_host other_host;
            CHECK(tsfp_guest_map_host(&other_host,&other_session));
            CHECK(!tsfp_guest_map_init(&other_layout,&other_host,&config));
            CHECK(!other_layout.started && !other_session.owns_arena);
            CHECK(tsfp_guest_arena_ready());
            CHECK(((volatile unsigned char *)layout.tiled)[0x1000]==0x6b);
        }
#endif
        CHECK(tsfp_guest_map_stop(&layout)==1 && !layout.started && !session.owns_arena);
        CHECK(!tsfp_guest_arena_occupied());
    }
    puts("PASS: real shared aliases, separate backing, offsets, collisions and concurrent views");return 0;
}
