#include "host_blocks.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct mapping { void *parent,*returned;size_t size; } mapping;
static mapping maps[64];
static tsfp_host_blocks owner;
static tsfp_host_block records[32];
static size_t page_size,alive,map_calls,unmap_calls;
static int map_fail,unmap_fail,bad_map;
static uint32_t last_protection;

static void *map_bytes(void *context,size_t bytes,uint32_t protection) {
    size_t slot=0;
    void *p;
    uintptr_t aligned;
    assert(context==maps && !(bytes&(page_size-1)) && bytes);
    ++map_calls;last_protection=protection;
    if(map_fail)return NULL;
    while(slot<64 && maps[slot].returned)++slot;
    assert(slot<64 && bytes<SIZE_MAX-page_size);
    p=calloc(1,bytes+page_size);assert(p);
    aligned=((uintptr_t)p+page_size-1)&~((uintptr_t)page_size-1);
    maps[slot].parent=p;maps[slot].returned=(void *)(aligned+(bad_map?1:0));
    maps[slot].size=bytes;++alive;return maps[slot].returned;
}
static int unmap_bytes(void *context,void *p,size_t size) {
    size_t slot=0;
    assert(context==maps);++unmap_calls;
    while(slot<64 && maps[slot].returned!=p)++slot;
    assert(slot<64 && maps[slot].size==size);
    if(unmap_fail)return 0;
    free(maps[slot].parent);memset(maps+slot,0,sizeof(*maps));--alive;return 1;
}
static void setup(size_t page,size_t capacity) {
    tsfp_host_block_backend backend={maps,map_bytes,unmap_bytes};
    assert(!alive);memset(&owner,0,sizeof(owner));memset(records,0xa5,sizeof(records));
    page_size=page;map_fail=0;unmap_fail=0;bad_map=0;map_calls=0;unmap_calls=0;
    assert(tsfp_host_blocks_init(&owner,&backend,page,records,capacity));
}
static void allocation_checks(void) {
    for(unsigned host=0;host<2;++host) {
        setup(host?16384:4096,32);
        for(size_t alignment=1;alignment<=1048576;alignment*=2) {
            const size_t sizes[]={0,1,17,4095,4096,16385};
            for(size_t j=0;j<sizeof(sizes)/sizeof(*sizes);++j) {
                tsfp_host_block found;
                size_t bytes=sizes[j],backing=bytes?bytes:1;
                unsigned char *p=tsfp_host_block_allocate(&owner,bytes,alignment,0x40,7);
                assert(p && !((uintptr_t)p&(alignment-1)) && last_protection==0x40);
                assert(tsfp_host_block_find(&owner,p,&found) && found.bytes==bytes && found.kind==7);
                assert((uintptr_t)p>UINT32_MAX); /* native addresses really exceed guest width */
                assert(found.mapping_size>=backing && owner.live==1 && alive==1);
                for(size_t i=0;i<backing;++i)assert(p[i]==0);
                memset(p,0x39,backing);
                if(bytes) {
                    assert(tsfp_host_block_find(&owner,p+bytes-1,&found));
                    assert(!tsfp_host_block_find(&owner,p+bytes,&found));
                } else assert(!tsfp_host_block_find(&owner,p+1,&found));
                if(found.mapping!=p)assert(!tsfp_host_block_find(&owner,found.mapping,&found));
                assert(!tsfp_host_block_release(&owner,p,8));
                assert(!tsfp_host_block_release(&owner,p+1,7));
                assert(tsfp_host_block_release(&owner,p,7));
                assert(!tsfp_host_block_release(&owner,p,7) && !owner.live && !alive);
            }
        }
        assert(tsfp_host_blocks_stop(&owner)==0);
    }
}
static void failure_checks(void) {
    tsfp_host_block result,before;
    void *a,*b,*c;
    size_t calls;
    setup(4096,2);
    assert(!tsfp_host_block_allocate(&owner,SIZE_MAX,4096,4,1));
    assert(!tsfp_host_block_allocate(&owner,1,3,4,1));
    assert(!tsfp_host_block_allocate(&owner,1,4096,4,0) && !map_calls);
    map_fail=1;assert(!tsfp_host_block_allocate(&owner,1,4096,4,1) && owner.live==0);
    map_fail=0;
    a=tsfp_host_block_allocate(&owner,11,0,4,1);
    b=tsfp_host_block_allocate(&owner,13,8192,4,2);assert(a && b && a!=b);
    calls=map_calls;
    assert(!tsfp_host_block_allocate(&owner,1,4096,4,1) && map_calls==calls);
    unmap_fail=1;
    assert(!tsfp_host_block_release(&owner,a,1) && owner.live==2 && alive==2);
    assert(tsfp_host_block_find(&owner,a,&result) && result.bytes==11);
    assert(tsfp_host_blocks_stop(&owner)==2 && !owner.active && alive==2);
    assert(!tsfp_host_block_allocate(&owner,1,4096,4,1) && map_calls==calls);
    unmap_fail=0;
    assert(tsfp_host_block_release(&owner,a,1));
    assert(tsfp_host_blocks_stop(&owner)==0 && !alive);
    memset(&before,0x35,sizeof(before));result=before;
    assert(!tsfp_host_block_find(&owner,b,&result) && !memcmp(&result,&before,sizeof(result)));
    setup(4096,2);bad_map=1;unmap_fail=1;
    c=tsfp_host_block_allocate(&owner,31,4096,4,1);
    assert(!c && owner.live==1 && alive==1 && records[0].address==NULL);
    assert(!tsfp_host_block_find(&owner,records[0].mapping,&result));
    unmap_fail=0;assert(tsfp_host_blocks_stop(&owner)==0 && !alive);
}
static void lifecycle_checks(void) {
    tsfp_host_blocks original;
    tsfp_host_block prior[32];
    tsfp_host_block_backend backend={maps,map_bytes,unmap_bytes};
    setup(4096,32);original=owner;memcpy(prior,records,sizeof(prior));
    assert(!tsfp_host_blocks_init(&owner,&backend,16384,records,32));
    assert(!memcmp(&owner,&original,sizeof(owner)) && !memcmp(prior,records,sizeof(prior)));
    assert(!tsfp_host_blocks_stop(&owner));original=owner;
    assert(!tsfp_host_blocks_init(&owner,&backend,3,records,32));
    assert(!tsfp_host_blocks_init(&owner,&backend,4096,records,SIZE_MAX));
    assert(!tsfp_host_blocks_init(&owner,&backend,4096,(tsfp_host_block *)&owner,1));
    assert(!memcmp(&owner,&original,sizeof(owner)));
    assert(tsfp_host_blocks_init(&owner,&backend,16384,records,32));page_size=16384;
    assert(tsfp_host_block_allocate(&owner,7,0,4,1));
    assert(!tsfp_host_blocks_stop(&owner) && !alive);
}
static void worker(void) {
    for(unsigned i=0;i<500;++i) {
        unsigned char *p=tsfp_host_block_allocate(&owner,73,8192,4,11);
        tsfp_host_block found;
        assert(p);memset(p,(int)(i&255),73);
        assert(tsfp_host_block_find(&owner,p+72,&found) && found.address==p && found.bytes==73);
        assert(tsfp_host_block_release(&owner,p,11));
    }
}
#ifdef _WIN32
static DWORD WINAPI thread_entry(void *unused) { (void)unused;worker();return 0; }
#else
static void *thread_entry(void *unused) { (void)unused;worker();return NULL; }
#endif
static void concurrent_checks(void) {
    setup(4096,32);
#ifdef _WIN32
    HANDLE threads[4];
    for(unsigned i=0;i<4;++i){threads[i]=CreateThread(NULL,0,thread_entry,NULL,0,NULL);assert(threads[i]);}
    assert(WaitForMultipleObjects(4,threads,TRUE,INFINITE)==WAIT_OBJECT_0);
    for(unsigned i=0;i<4;++i)CloseHandle(threads[i]);
#else
    pthread_t threads[4];
    for(unsigned i=0;i<4;++i)assert(!pthread_create(threads+i,NULL,thread_entry,NULL));
    for(unsigned i=0;i<4;++i)assert(!pthread_join(threads[i],NULL));
#endif
    assert(!owner.live && !alive && map_calls==2000 && unmap_calls==2000);
    assert(!tsfp_host_blocks_stop(&owner));
}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"allocate"))allocation_checks();
    else if(!strcmp(argv[1],"failure"))failure_checks();
    else if(!strcmp(argv[1],"lifecycle"))lifecycle_checks();
    else if(!strcmp(argv[1],"concurrent"))concurrent_checks();
    else abort();
    puts("native block ownership contracts passed");return 0;
}
