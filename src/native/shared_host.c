#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#include "shared_views.h"
#if defined(_WIN32)
#include <windows.h>
static int open_backing(void *context,size_t bytes,uintptr_t *token) {
    HANDLE h;
    uint64_t n=(uint64_t)bytes;
    (void)context;
    h=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,(DWORD)(n>>32),(DWORD)n,NULL);
    if(!h)return 0;
    *token=(uintptr_t)h;return 1;
}
static void close_backing(void *context,uintptr_t token) {
    (void)context;(void)CloseHandle((HANDLE)token);
}
static void *map_view(void *context,uintptr_t token,size_t offset,size_t bytes,void *hint,unsigned access) {
    uint64_t at=(uint64_t)offset;
    (void)context;
    return MapViewOfFileEx((HANDLE)token,access==TSFP_VIEW_READ?FILE_MAP_READ:FILE_MAP_WRITE,
        (DWORD)(at>>32),(DWORD)at,bytes,hint);
}
static int unmap_view(void *context,void *base,size_t bytes) {
    (void)context;(void)bytes;return UnmapViewOfFile(base)!=0;
}
#else
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <limits.h>
static int open_backing(void *context,size_t bytes,uintptr_t *token) {
    FILE *stream;
    off_t length=(off_t)bytes;
    (void)context;
    if(length<=0 || (uintmax_t)length!=(uintmax_t)bytes)return 0;
    stream=tmpfile();
    if(!stream)return 0;
    if(ftruncate(fileno(stream),length)) { fclose(stream);return 0; }
    *token=(uintptr_t)stream;return 1;
}
static void close_backing(void *context,uintptr_t token) {
    /* fclose consumes the stream even on error. Retrying a descriptor number
     * risks closing a newly reused descriptor, so this is deliberately once. */
    (void)context;(void)fclose((FILE *)token);
}
static void *map_view(void *context,uintptr_t token,size_t offset,size_t bytes,void *hint,unsigned access) {
    off_t at=(off_t)offset;
    void *p;
    if(at<0 || (uintmax_t)at!=(uintmax_t)offset)return NULL;
    if(context && tsfp_owned_arena_intersects(context,hint,bytes)) {
        int fd=fileno((FILE *)token);
        if(fd<0)return NULL;
        return tsfp_owned_arena_map(context,hint,bytes,(uintptr_t)fd+1,offset,
            TSFP_ARENA_READ|(access==TSFP_VIEW_WRITE?TSFP_ARENA_WRITE:0));
    }
    p=mmap(hint,bytes,PROT_READ|(access==TSFP_VIEW_WRITE?PROT_WRITE:0),MAP_SHARED,
        fileno((FILE *)token),at);
    return p==MAP_FAILED?NULL:p;
}
static int unmap_view(void *context,void *base,size_t bytes) {
    if(context && tsfp_owned_arena_intersects(context,base,bytes))
        return tsfp_owned_arena_release(context,base);
    return munmap(base,bytes)==0;
}
static void *reserve_arena(void *context,size_t bytes) {
    void *p;
    (void)context;p=mmap(NULL,bytes,PROT_NONE,MAP_PRIVATE|MAP_ANON,-1,0);
    return p==MAP_FAILED?NULL:p;
}
static int release_arena(void *context,void *base,size_t bytes) {
    (void)context;return munmap(base,bytes)==0;
}
static int place_in_arena(void *context,void *base,size_t bytes,uintptr_t token,size_t offset,unsigned access) {
    off_t at=(off_t)offset;
    int fd;
    int flags=MAP_FIXED|(token?MAP_SHARED:MAP_PRIVATE|MAP_ANON);
    int protection=((access&TSFP_ARENA_READ)?PROT_READ:0)|
        ((access&TSFP_ARENA_WRITE)?PROT_WRITE:0)|((access&TSFP_ARENA_EXECUTE)?PROT_EXEC:0);
    void *p;
    (void)context;
    if(at<0 || (uintmax_t)at!=(uintmax_t)offset || token>(uintptr_t)INT_MAX+1)return 0;
    /* Native arena tokens are fd+1 so fd zero and private storage differ.
     * Shared backing streams remain owned by the separate shared-view owner. */
    fd=token?(int)(token-1):-1;
    p=mmap(base,bytes,protection,flags,fd,at);
    /* Failure of a replacing operation is conservatively uncertain. Retain
     * the owned interval; the arena will not admit another placement. */
    return p==base?1:-1;
}
static int restore_arena(void *context,void *base,size_t bytes) {
    void *p;
    (void)context;p=mmap(base,bytes,PROT_NONE,MAP_FIXED|MAP_PRIVATE|MAP_ANON,-1,0);
    return p==base?1:-1;
}
#endif
int tsfp_shared_host_backend(tsfp_shared_backend *out,size_t *page,size_t *granularity) {
    size_t p,g;
    tsfp_shared_backend b={NULL,open_backing,close_backing,map_view,unmap_view};
    if(!out || !page || !granularity)return 0;
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);p=info.dwPageSize;g=info.dwAllocationGranularity;
#else
    long n=sysconf(_SC_PAGESIZE);
    if(n<=0)return 0;
    p=g=(size_t)n;
#endif
    if(!p || !g)return 0;
    *out=b;*page=p;*granularity=g;return 1;
}
int tsfp_shared_host_arena_init(tsfp_owned_arena *s,size_t bytes,tsfp_arena_view *v,size_t capacity) {
#if defined(_WIN32)
    (void)s;(void)bytes;(void)v;(void)capacity;return 0;
#else
    tsfp_arena_backend b={NULL,reserve_arena,release_arena,place_in_arena,restore_arena};
    long page=sysconf(_SC_PAGESIZE);
    return page>0 && tsfp_owned_arena_init(s,&b,bytes,(size_t)page,v,capacity);
#endif
}
int tsfp_shared_host_in_arena(tsfp_shared_backend *out,tsfp_owned_arena *arena,size_t *page,size_t *granularity) {
#if defined(_WIN32)
    (void)out;(void)arena;(void)page;(void)granularity;return 0;
#else
    tsfp_shared_backend b;size_t p,g;
    if(!out || !page || !granularity || !arena || !arena->base || !arena->active ||
       !tsfp_shared_host_backend(&b,&p,&g) || p!=arena->page_size)return 0;
    b.context=arena;*out=b;*page=p;*granularity=g;return 1;
#endif
}
