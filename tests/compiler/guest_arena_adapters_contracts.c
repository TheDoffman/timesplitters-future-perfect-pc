#include "guest_arena_service.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static size_t host_page=16384,last_bytes;
static uintptr_t last_token;
static unsigned last_access,places,restores,whole_releases,opens,closes,outside_maps;
static int restore_status=1,fail_outside_unmap;
static void *reserve_model(void *ctx,size_t bytes) { (void)ctx;CHECK(bytes==UINT64_C(0x100000000));return (void *)(uintptr_t)UINT64_C(0x200000000); }
static int release_model(void *ctx,void *base,size_t bytes) { (void)ctx;CHECK(base && bytes);++whole_releases;return 1; }
static int place_model(void *ctx,void *base,size_t bytes,uintptr_t token,size_t offset,unsigned access) {
    (void)ctx;CHECK(base && offset%host_page==0);++places;last_token=token;last_bytes=bytes;last_access=access;return 1;
}
static int restore_model(void *ctx,void *base,size_t bytes) { (void)ctx;CHECK(base && bytes);++restores;return restore_status; }
static int open_model(void *ctx,size_t bytes,uintptr_t *token) { (void)ctx;CHECK(bytes);*token=++opens;return 1; }
static void close_model(void *ctx,uintptr_t token) { (void)ctx;CHECK(token);++closes; }
static void *map_model(void *ctx,uintptr_t token,size_t offset,size_t bytes,void *hint,unsigned access) {
    tsfp_owned_arena *s=ctx;
    if(hint)return tsfp_owned_arena_map(s,hint,bytes,token,offset,access==TSFP_VIEW_READ?1:3);
    return (void *)(uintptr_t)(UINT64_C(0x500000000)+(uint64_t)(++outside_maps)*0x10000000);
}
static int unmap_model(void *ctx,void *base,size_t bytes) {
    if(tsfp_owned_arena_intersects(ctx,base,bytes))return tsfp_owned_arena_release(ctx,base);
    return !fail_outside_unmap;
}
int tsfp_shared_host_arena_init(tsfp_owned_arena *s,size_t bytes,tsfp_arena_view *v,size_t count) {
    tsfp_arena_backend b={NULL,reserve_model,release_model,place_model,restore_model};
    return tsfp_owned_arena_init(s,&b,bytes,host_page,v,count);
}
int tsfp_shared_host_in_arena(tsfp_shared_backend *b,tsfp_owned_arena *s,size_t *page,size_t *gran) {
    *b=(tsfp_shared_backend){s,open_model,close_model,map_model,unmap_model};*page=*gran=host_page;return 1;
}

/* Only the public ABI vocabulary and unreachable generic fallbacks are modeled;
 * the arena/MMIO adapters below are extracted from the real proposed runtime. */
typedef void *LPVOID;typedef const void *LPCVOID;typedef void *HANDLE;
typedef size_t SIZE_T;typedef uint32_t DWORD;typedef int BOOL;
typedef struct w32_object { int kind,fd;size_t map_size; } w32_object;
#define FALSE 0
#define TRUE 1
#define K_FILEMAP 7
#define ERROR_INVALID_PARAMETER 87
#define ERROR_INVALID_ADDRESS 487
#define ERROR_INVALID_HANDLE 6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define MEM_RESERVE 0x2000
#define MEM_COMMIT 0x1000
#define MEM_RELEASE 0x8000
#define MEM_DECOMMIT 0x4000
#define PAGE_READWRITE 4
#define PAGE_EXECUTE_READWRITE 64
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 4096
#define MAP_FAILED ((void *)-1)
#define FILE_MAP_ALL_ACCESS 0xf001f
#define FILE_MAP_READ 4
#define off_t int64_t
#define __APPLE__ 1
static unsigned registry_takes,registry_adds;
static volatile unsigned unexpected_fallbacks;
static void SetLastError(DWORD error) { (void)error; }
static int prot_from_page(DWORD p) { return p==64?7:(p==4?3:(p==2?1:0)); }
static void *model_mmap(void *a,size_t n,int p,int f,int fd,int64_t o) { (void)a;(void)n;(void)p;(void)f;(void)fd;(void)o;++unexpected_fallbacks;return MAP_FAILED; }
static int model_munmap(void *a,size_t n) { (void)a;(void)n;++unexpected_fallbacks;return -1; }
static int model_mprotect(void *a,size_t n,int p) { (void)a;(void)n;(void)p;++unexpected_fallbacks;return -1; }
static void view_register(void *p,size_t n) { (void)p;(void)n;++registry_adds; }
static size_t view_take(const void *p) { (void)p;++registry_takes;return 0; }
#define mmap model_mmap
#define munmap model_munmap
#define mprotect model_mprotect
#include "guest_arena_adapters.inc"
#undef mmap
#undef munmap
#undef mprotect
#undef off_t
#undef __APPLE__
int main(void) {
    unsigned before;
    unsigned char *base=w32_guest_arena_reserve((size_t)UINT64_C(0x100000000));
    void *private_page,*shared,*nv,*mc,*alias;
    w32_object mapping={K_FILEMAP,0,65536};
    CHECK(base==(void *)(uintptr_t)UINT64_C(0x200000000) && tsfp_guest_arena_ready());
    CHECK(!w32_guest_arena_reserve((size_t)UINT64_C(0x100000000)));
    private_page=VirtualAlloc(base+0x10000,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    CHECK(private_page==base+0x10000 && last_bytes==16384 && last_access==7 && !last_token);
    CHECK(!VirtualAlloc(private_page,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    shared=MapViewOfFileEx(&mapping,FILE_MAP_ALL_ACCESS,0,0,65536,base+0x20000);
    CHECK(shared==base+0x20000 && last_token==1 && last_access==3);
    CHECK(!MapViewOfFileEx(&mapping,FILE_MAP_ALL_ACCESS,0,0,65536,base+0x20000));
    CHECK(!registry_adds);
    restore_status=-1;CHECK(!UnmapViewOfFile(shared));CHECK(!tsfp_guest_arena_ready());
    CHECK(!registry_takes);restore_status=1;CHECK(UnmapViewOfFile(shared));
    before=whole_releases;CHECK(!w32_guest_arena_release());CHECK(whole_releases==before);
    CHECK(!tsfp_guest_arena_ready());CHECK(VirtualFree(private_page,0,MEM_RELEASE));
    CHECK(w32_guest_arena_release());CHECK(!registry_takes && !registry_adds);
    CHECK(w32_guest_arena_release());CHECK(whole_releases==before+1);

    base=w32_guest_arena_reserve((size_t)UINT64_C(0x100000000));CHECK(base);
    nv=w32_guest_mmio_map(base+0xfd000000u,0x1000000);CHECK(nv==base+0xfd000000u);
    mc=w32_guest_mmio_map(base+0xfe800000u,0x800000);CHECK(mc==base+0xfe800000u);
    alias=w32_guest_mmio_alias((unsigned char *)nv+0x600100,4);CHECK(alias);
    before=places+restores+opens+closes;
    CHECK(w32_guest_mmio_alias((unsigned char *)nv+0x600104,4)==(unsigned char *)alias+4);
    CHECK(!w32_guest_mmio_alias((unsigned char *)nv+0x1000000-1,4));
    CHECK(places+restores+opens+closes==before);
    CHECK(!VirtualAlloc(mc,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    CHECK(!w32_guest_mmio_map(base+0xff000000u,0x100000));
    fail_outside_unmap=1;CHECK(!w32_guest_arena_release());CHECK(!tsfp_guest_arena_ready());
    CHECK(!w32_guest_mmio_alias(nv,4));CHECK(!w32_guest_mmio_map(base+0xff000000u,0x100000));
    CHECK(!w32_guest_arena_reserve((size_t)UINT64_C(0x100000000)));
    fail_outside_unmap=0;CHECK(w32_guest_arena_release());CHECK(opens==closes);
    base=w32_guest_arena_reserve((size_t)UINT64_C(0x100000000));CHECK(base);
    CHECK(w32_guest_mmio_map(base+0xfd000000u,0x1000000));
    CHECK(w32_guest_arena_release());CHECK(opens==closes);
    CHECK(!unexpected_fallbacks);
    puts("PASS: actual arena/VirtualAlloc/MapView/MMIO adapters share ownership and retain failed cleanup");return 0;
}
