#include "owned_arena.h"
#include "shared_views.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
typedef struct model { unsigned reserves,releases,places,restores;int fail_reserve,fail_release,place,restore;size_t bytes,offset;uintptr_t token; } model;
static void *reserve(void *p,size_t bytes) { model *m=p;++m->reserves;m->bytes=bytes;return m->fail_reserve?NULL:(void *)(uintptr_t)0x40000000; }
static int release(void *p,void *base,size_t bytes) { model *m=p;CHECK(base==(void *)(uintptr_t)0x40000000 && bytes);++m->releases;return !m->fail_release; }
static int place(void *p,void *base,size_t bytes,uintptr_t token,size_t offset,unsigned access) {
    model *m=p;CHECK(base && access<=7);++m->places;m->bytes=bytes;m->token=token;m->offset=offset;return m->place;
}
static int restore(void *p,void *base,size_t bytes) { model *m=p;CHECK(base);++m->restores;m->bytes=bytes;return m->restore; }
static void contracts(size_t page) {
    model m={0};tsfp_owned_arena s={0};tsfp_arena_view views[2];
    tsfp_arena_backend backend={&m,reserve,release,place,restore};
    uintptr_t base=0x40000000;
    void *a=(void *)base,*b=(void *)(base+page*4);
    unsigned calls;
    m.place=m.restore=1;
    CHECK(!tsfp_owned_arena_init(&s,&backend,page*16,3,views,2));CHECK(!m.reserves);
    m.fail_reserve=1;
    CHECK(!tsfp_owned_arena_init(&s,&backend,page*16,page,views,2));CHECK(!s.base);
    m.fail_reserve=0;
    CHECK(tsfp_owned_arena_init(&s,&backend,page*16-1,page,views,2));CHECK(s.bytes==page*16);
    CHECK(!tsfp_owned_arena_init(&s,&backend,page*16,page,views,2));
    CHECK(tsfp_owned_arena_intersects(&s,a,0));
    CHECK(tsfp_owned_arena_intersects(&s,(void *)(base-page),page+1));
    CHECK(!tsfp_owned_arena_intersects(&s,(void *)(base-page),page));
    CHECK(!tsfp_owned_arena_intersects(&s,(void *)(base+page*16),page));
    CHECK(tsfp_owned_arena_intersects(&s,(void *)(base+page),SIZE_MAX));
    CHECK(!tsfp_owned_arena_map(&s,NULL,page,0,0,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,(void *)(base-page),page*2,0,0,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,(void *)(base+1),page,0,0,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,a,SIZE_MAX,0,0,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,a,page,0,page,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,a,page,17,1,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,a,page,17,0,8));CHECK(!m.places);
    CHECK(tsfp_owned_arena_map(&s,a,page+1,0,0,TSFP_VIEW_WRITE)==a);
    CHECK(s.live==1 && m.bytes==page*2 && !m.token);
    calls=m.places;
    CHECK(!tsfp_owned_arena_map(&s,(void *)(base+page),page,0,0,TSFP_VIEW_WRITE));
    CHECK(!tsfp_owned_arena_map(&s,(void *)(base+page*15),page+1,0,0,TSFP_VIEW_WRITE));
    CHECK(m.places==calls);
    CHECK(tsfp_owned_arena_map(&s,b,page,17,page*3,TSFP_VIEW_READ)==b);
    CHECK(m.token==17 && m.offset==page*3 && s.live==2);
    calls=m.places;
    CHECK(!tsfp_owned_arena_map(&s,(void *)(base+page*8),page,0,0,TSFP_VIEW_WRITE));CHECK(m.places==calls);
    CHECK(!tsfp_owned_arena_release(&s,(void *)(base+1)));
    CHECK(!tsfp_owned_arena_release(&s,(void *)(base+page)));CHECK(!m.restores);
    m.restore=0;CHECK(!tsfp_owned_arena_release(&s,a));CHECK(s.live==2 && !s.poisoned);
    m.restore=-1;CHECK(!tsfp_owned_arena_release(&s,a));CHECK(s.live==2 && s.poisoned);
    m.restore=1;CHECK(tsfp_owned_arena_release(&s,a));CHECK(m.bytes==page*2 && s.live==1);
    CHECK(!tsfp_owned_arena_map(&s,a,page,0,0,TSFP_VIEW_WRITE));CHECK(m.places==calls);
    CHECK(!tsfp_owned_arena_stop(&s));CHECK(!m.releases && !s.active);
    CHECK(tsfp_owned_arena_release(&s,b));CHECK(!s.live);
    m.fail_release=1;CHECK(!tsfp_owned_arena_stop(&s));CHECK(s.base && s.poisoned);
    m.fail_release=0;CHECK(tsfp_owned_arena_stop(&s));CHECK(!s.base && !s.poisoned);
    calls=m.releases;CHECK(tsfp_owned_arena_stop(&s));CHECK(m.releases==calls);

    CHECK(tsfp_owned_arena_init(&s,&backend,page*16,page,views,2));
    m.place=0;CHECK(!tsfp_owned_arena_map(&s,a,page,17,0,TSFP_VIEW_WRITE));CHECK(!s.live && !s.poisoned);
    m.place=1;CHECK(tsfp_owned_arena_map(&s,a,page,0,0,TSFP_VIEW_WRITE));
    m.place=-1;CHECK(!tsfp_owned_arena_map(&s,b,page,17,0,TSFP_VIEW_WRITE));
    CHECK(s.live==2 && s.poisoned && views[1].state==2);
    calls=m.restores;CHECK(!tsfp_owned_arena_release(&s,b));CHECK(m.restores==calls);
    CHECK(!tsfp_owned_arena_stop(&s));
    CHECK(tsfp_owned_arena_release(&s,a));
    CHECK(tsfp_owned_arena_stop(&s));CHECK(!s.live && !views[1].base);
}
int main(void) { contracts(4096);contracts(16384);puts("PASS: owned arena boundaries, overlap, quarantine and cleanup ordering");return 0; }
