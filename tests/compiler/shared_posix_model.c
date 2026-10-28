#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "shared_views.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static unsigned opened,closed,truncated,mapped,unmapped;
static int fail_open,fail_size,fail_map,fail_unmap;
static size_t host_page=16384,last_bytes;
static int64_t last_length,last_offset;
static void *last_hint;
static int last_protection,last_flags,last_fd;
static FILE *fake_stream=(FILE *)(uintptr_t)0x1234;
static FILE *model_tmpfile(void) { ++opened;return fail_open?NULL:fake_stream; }
static int model_fclose(FILE *f) { CHECK(f==fake_stream);++closed;return -1; }
static int model_fileno(FILE *f) { CHECK(f==fake_stream);return 0; }
static int model_ftruncate(int fd,int64_t n) { CHECK(fd==0);++truncated;last_length=n;return fail_size?-1:0; }
static long model_sysconf(int name) { (void)name;return (long)host_page; }
static void *model_mmap(void *hint,size_t bytes,int protection,int flags,int fd,int64_t offset) {
    ++mapped;last_hint=hint;last_bytes=bytes;last_protection=protection;last_flags=flags;
    last_fd=fd;last_offset=offset;
    return fail_map?(void *)(intptr_t)-1:((flags&16)?hint:(void *)(uintptr_t)0x40000000);
}
static int model_munmap(void *base,size_t bytes) {
    CHECK(base==(void *)(uintptr_t)0x40000000);++unmapped;last_bytes=bytes;return fail_unmap?-1:0;
}
/* Models never interpose process libc symbols; all host APIs stay private. */
#undef _WIN32
#define tmpfile model_tmpfile
#define fclose model_fclose
#define fileno model_fileno
#define ftruncate model_ftruncate
#define sysconf model_sysconf
#define mmap model_mmap
#define munmap model_munmap
#define off_t int64_t
#include "shared_host.c"
#undef tmpfile
#undef fclose
#undef fileno
#undef ftruncate
#undef sysconf
#undef mmap
#undef munmap
#undef off_t
int main(void) {
    tsfp_shared_backend b;size_t page=1,gran=2;uintptr_t token=99;
    tsfp_shared_views s={0};tsfp_shared_view view[1];
    CHECK(tsfp_shared_host_backend(&b,&page,&gran));CHECK(page==16384 && gran==16384);
    CHECK(!b.open(NULL,SIZE_MAX,&token));CHECK(token==99 && !opened);
    fail_open=1;CHECK(!b.open(NULL,page,&token));CHECK(token==99 && opened==1 && !closed && !truncated);
    fail_open=0;fail_size=1;
    CHECK(!b.open(NULL,page,&token));CHECK(token==99 && closed==1 && truncated==1);
    fail_size=0;
    CHECK(b.open(NULL,(size_t)UINT64_C(0x100004000),&token));
    CHECK(token==(uintptr_t)fake_stream && last_length==INT64_C(0x100004000));
    CHECK(b.map(NULL,token,(size_t)UINT64_C(0x100000000),page,(void *)(uintptr_t)0x20000000,TSFP_VIEW_WRITE));
    CHECK(last_offset==INT64_C(0x100000000) && last_bytes==page && last_hint==(void *)(uintptr_t)0x20000000);
    CHECK(last_flags==MAP_SHARED && last_protection==(PROT_READ|PROT_WRITE) && last_fd==0);
    CHECK(b.map(NULL,token,0,page,NULL,TSFP_VIEW_READ));CHECK(last_protection==PROT_READ);
    CHECK(!b.map(NULL,token,SIZE_MAX,page,NULL,TSFP_VIEW_READ));
    fail_map=1;CHECK(!b.map(NULL,token,0,page,NULL,TSFP_VIEW_READ));fail_map=0;
    fail_unmap=1;CHECK(!b.unmap(NULL,(void *)(uintptr_t)0x40000000,page));
    fail_unmap=0;CHECK(b.unmap(NULL,(void *)(uintptr_t)0x40000000,page));CHECK(last_bytes==page);
    b.close(NULL,token);CHECK(closed==2); /* consumed exactly once despite fclose error */
    CHECK(tsfp_shared_views_init(&s,&b,page,page,gran,view,1));
    fail_unmap=1;
    CHECK(!tsfp_shared_views_map(&s,(void *)(uintptr_t)0x20000000,0,page,TSFP_VIEW_WRITE));
    CHECK(s.live==1 && !view[0].published && last_flags==MAP_SHARED);
    CHECK(!tsfp_shared_views_stop(&s) && closed==2 && s.backing);
    fail_unmap=0;CHECK(tsfp_shared_views_stop(&s));CHECK(closed==3 && !s.backing);
    CHECK(tsfp_shared_views_stop(&s));CHECK(closed==3);
    {
        tsfp_owned_arena arena={0};tsfp_arena_view records[3];
        void *base;
        unsigned calls;
        CHECK(tsfp_shared_host_arena_init(&arena,page*32,records,3));
        base=arena.base;CHECK(last_protection==PROT_NONE && last_flags==(MAP_PRIVATE|MAP_ANON));
        CHECK(tsfp_shared_host_in_arena(&b,&arena,&page,&gran));
        for(unsigned access=0;access<8;++access) {
            void *private_at=(void *)((uintptr_t)base+page*8);
            CHECK(tsfp_owned_arena_map(&arena,private_at,page,0,0,access)==private_at);
            CHECK(last_protection==(int)access && last_flags==(MAP_PRIVATE|MAP_ANON|MAP_FIXED) && last_fd==-1);
            CHECK(tsfp_owned_arena_release(&arena,private_at));
        }
        calls=mapped;
        CHECK(!tsfp_owned_arena_map(&arena,base,page,(uintptr_t)INT_MAX+2,0,TSFP_ARENA_READ));
        CHECK(mapped==calls && !arena.poisoned);
        CHECK(tsfp_shared_views_init(&s,&b,page,page,gran,view,1));
        CHECK(tsfp_shared_views_map(&s,base,0,page,TSFP_VIEW_WRITE)==base);
        CHECK(last_flags==(MAP_SHARED|MAP_FIXED) && last_fd==0);
        calls=mapped;
        CHECK(!tsfp_owned_arena_map(&arena,base,page,0,0,TSFP_VIEW_WRITE));CHECK(mapped==calls);
        CHECK(!tsfp_owned_arena_map(&arena,(void *)((uintptr_t)base-page),page*2,0,0,TSFP_VIEW_WRITE));
        CHECK(mapped==calls);
        fail_map=1;CHECK(!tsfp_shared_views_release(&s,base));CHECK(arena.poisoned && s.live==1);
        fail_map=0;CHECK(tsfp_shared_views_release(&s,base));
        CHECK(last_flags==(MAP_PRIVATE|MAP_ANON|MAP_FIXED) && last_protection==PROT_NONE && last_fd==-1);
        CHECK(tsfp_shared_views_stop(&s));CHECK(tsfp_owned_arena_stop(&arena));
    }
    puts("PASS: controlled POSIX backing lifetime, descriptor zero, wide offsets and failed cleanup");return 0;
}
