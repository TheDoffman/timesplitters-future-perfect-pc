#include "shared_views.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
typedef struct model {
    unsigned opens,closes,maps,unmaps,fail_open,fail_map,fail_unmap;
    size_t last_offset,last_bytes;
    uintptr_t next;
    unsigned ignore_hint;
} model;
static int open_store(void *p,size_t bytes,uintptr_t *token) {
    model *m=p;++m->opens;CHECK(bytes);
    if(m->fail_open)return 0;
    *token=17;return 1;
}
static void close_store(void *p,uintptr_t token) { model *m=p;CHECK(token==17);++m->closes; }
static void *map_store(void *p,uintptr_t token,size_t offset,size_t bytes,void *hint,unsigned access) {
    model *m=p;CHECK(token==17 && access);++m->maps;
    m->last_offset=offset;m->last_bytes=bytes;
    if(m->fail_map)return NULL;
    if(hint && !m->ignore_hint)return hint;
    m->next+=0x100000;return (void *)m->next;
}
static int unmap_store(void *p,void *base,size_t bytes) {
    model *m=p;CHECK(base && bytes);++m->unmaps;m->last_bytes=bytes;
    return !m->fail_unmap;
}
static void contracts(size_t page) {
    model m={0};tsfp_shared_views s={0};tsfp_shared_view views[3];
    tsfp_shared_backend b={&m,open_store,close_store,map_store,unmap_store};
    const size_t gran=page*4,total=gran*8;
    void *a,*c;
    unsigned before;
    memset(views,0x71,sizeof views);
    CHECK(!tsfp_shared_views_init(&s,&b,0,page,gran,views,3));
    CHECK(!tsfp_shared_views_init(&s,&b,total,3,gran,views,3));
    CHECK(!tsfp_shared_views_init(&s,&b,total,page,page/2,views,3));
    CHECK(!tsfp_shared_views_init(&s,&b,total,page,gran,views,SIZE_MAX));
    CHECK(!m.opens);
    m.fail_open=1;
    CHECK(!tsfp_shared_views_init(&s,&b,total,page,gran,views,3));
    CHECK(!s.backing && !s.active && !s.capacity);
    for(size_t i=0;i<sizeof views;++i)CHECK(((unsigned char *)views)[i]==0x71);
    m.fail_open=0;
    CHECK(tsfp_shared_views_init(&s,&b,total,page,gran,views,3));
    CHECK(!tsfp_shared_views_init(&s,&b,total,page,gran,views,3));
    CHECK(!tsfp_shared_views_map(&s,NULL,0,0,TSFP_VIEW_WRITE));
    CHECK(!tsfp_shared_views_map(&s,NULL,page,page,TSFP_VIEW_WRITE));
    CHECK(!tsfp_shared_views_map(&s,NULL,total,page,TSFP_VIEW_WRITE));
    CHECK(!tsfp_shared_views_map(&s,NULL,gran,total,TSFP_VIEW_WRITE));
    CHECK(!tsfp_shared_views_map(&s,NULL,0,page+1,TSFP_VIEW_WRITE));
    CHECK(!tsfp_shared_views_map(&s,NULL,0,page,3));
    CHECK(!tsfp_shared_views_map(&s,(void *)(uintptr_t)(gran+1),0,page,TSFP_VIEW_WRITE));
    CHECK(!tsfp_shared_views_map(&s,(void *)(UINTPTR_MAX-(gran-1)),0,gran,TSFP_VIEW_WRITE));
    CHECK(!m.maps);
    m.fail_map=1;
    CHECK(!tsfp_shared_views_map(&s,NULL,0,page,TSFP_VIEW_WRITE));CHECK(!s.live);
    m.fail_map=0;
    a=tsfp_shared_views_map(&s,NULL,gran,page,TSFP_VIEW_WRITE);
    CHECK(a && s.live==1 && m.last_offset==gran && m.last_bytes==page);
    before=m.maps;
    CHECK(!tsfp_shared_views_map(&s,a,0,page,TSFP_VIEW_READ));CHECK(m.maps==before);
    CHECK(!tsfp_shared_views_release(&s,(void *)((uintptr_t)a+1)));
    CHECK(!tsfp_shared_views_release(&s,(void *)(uintptr_t)1234));CHECK(!m.unmaps);
    m.fail_unmap=1;
    CHECK(!tsfp_shared_views_release(&s,a));CHECK(s.live==1 && views[0].base==a);
    m.fail_unmap=0;
    CHECK(tsfp_shared_views_release(&s,a));CHECK(!s.live && !views[0].base);
    CHECK(!tsfp_shared_views_release(&s,a));

    m.ignore_hint=1;m.fail_unmap=1;
    CHECK(!tsfp_shared_views_map(&s,(void *)(uintptr_t)0x10000000,0,page,TSFP_VIEW_WRITE));
    CHECK(s.live==1 && views[0].base && !views[0].published);
    before=m.unmaps;m.fail_unmap=0;
    CHECK(!tsfp_shared_views_release(&s,views[0].base));CHECK(m.unmaps==before);
    m.fail_unmap=1;
    CHECK(!tsfp_shared_views_reap(&s));CHECK(s.live==1);
    m.fail_unmap=0;CHECK(tsfp_shared_views_reap(&s));CHECK(!s.live);
    CHECK(!tsfp_shared_views_map(&s,(void *)(uintptr_t)0x10000000,0,page,TSFP_VIEW_WRITE));
    CHECK(!s.live); /* wrong-address successful cleanup */
    m.ignore_hint=0;
    a=tsfp_shared_views_map(&s,(void *)(uintptr_t)0x10000000,0,page,TSFP_VIEW_READ);
    CHECK(a==(void *)(uintptr_t)0x10000000 && s.live==1);
    c=tsfp_shared_views_map(&s,NULL,0,page,TSFP_VIEW_WRITE);CHECK(c);
    CHECK(tsfp_shared_views_map(&s,NULL,0,page,TSFP_VIEW_WRITE));CHECK(s.live==3);
    before=m.maps;CHECK(!tsfp_shared_views_map(&s,NULL,0,page,TSFP_VIEW_WRITE));
    CHECK(m.maps==before);
    m.fail_unmap=1;CHECK(!tsfp_shared_views_stop(&s));
    CHECK(s.live==3 && s.backing==17 && !s.active && !m.closes);
    CHECK(!tsfp_shared_views_map(&s,NULL,0,page,TSFP_VIEW_WRITE));CHECK(m.maps==before);
    m.fail_unmap=0;CHECK(tsfp_shared_views_release(&s,a));CHECK(s.live==2);
    CHECK(tsfp_shared_views_stop(&s));CHECK(!s.live && !s.backing && m.closes==1);
    CHECK(tsfp_shared_views_stop(&s));CHECK(m.closes==1);
    CHECK(tsfp_shared_views_init(&s,&b,total,page,gran,views,3));
    CHECK(tsfp_shared_views_stop(&s));CHECK(m.closes==2);
}
int main(void) {
    contracts(4096);contracts(16384);
    puts("PASS: shared view ownership, refusal, failed cleanup, capacity and stop retry");return 0;
}
