#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(3); } } while(0)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Trace {
    uint32_t original, reported, draws, failed, first, vertices, stride, xpt, count;
    uint32_t stream_first, stream_count, set_first;
    uint16_t indices[1024];
} Trace;
static Trace trace;
static uint32_t args[3], g_push_draws, g_push_failed;
static int g_push_active, g_base_vertex_seen=1, device=1, readable=1, gather_ok=1;
static uint32_t g_base_vertex=7, g_stream0_stride=32;
static uint16_t guest_indices[1024];
static uint8_t gathered_storage[4];
static int hle_original_D3DDevice_DrawIndexedVertices=1;
static unsigned allocations, frees, fail_alloc;
static void *tracked_malloc(size_t bytes) {
    allocations++;
    return fail_alloc ? NULL : malloc(bytes);
}
static void tracked_free(void *p) { if(p)frees++; free(p); }
#define HLE_ARG(i) args[i]
#define HLE_PTR(va) ((void)(va),guest_indices)
#define HLE_RETURN(v) do {(void)(v);return;}while(0)
#define HLE_CALL_ORIGINAL(name) (++trace.original)
static void first_call(int *seen,const char *name,uint32_t v) {(void)seen;(void)name;(void)v;}
static int original_missing(int original,const char *name) {(void)name;return !original;}
static int hle_d3d8_shadow_device(void) {return device;}
static int guest_readable(uint32_t va,uint64_t bytes) {(void)va;return readable && bytes<=sizeof guest_indices;}
static uint8_t *push_gather(uint32_t first,uint32_t vertices,uint32_t *stride) {
    trace.first=first;trace.vertices=vertices;*stride=32;
    /* The real gather owns a distinct allocation. Keep that boundary intact. */
    return gather_ok ? malloc(sizeof gathered_storage) : NULL;
}
static void report(void) {trace.reported++;}
static const void *stream0_vertices(uint32_t first,uint32_t vertices) {
    trace.stream_first=first;trace.stream_count=vertices;return gathered_storage;
}
static void hle_d3d8_shadow_set_first_vertex(uint32_t first) {trace.set_first=first;}
static void hle_d3d8_shadow_draw_indexed(uint32_t xpt,uint32_t count,const uint16_t *idx,
                                       const void *verts,uint32_t stride,int from_buffer) {
    (void)from_buffer;CHECK(verts && count<=1024);
    trace.draws++;trace.xpt=xpt;trace.count=count;trace.stride=stride;
    memcpy(trace.indices,idx,count*sizeof *idx);
}
/* ORIGINAL */
/* CANDIDATE */

static Trace run(void (*fn)(void)) {
    memset(&trace,0,sizeof trace);g_push_draws=g_push_failed=0;
    allocations=frees=0;fn();trace.failed=g_push_failed;
    CHECK(g_push_draws==trace.draws || !g_push_active);
    return trace;
}
int main(void) {
    args[0]=5;args[2]=0x1000;g_push_active=1;
    uint32_t rng=1234567;
    for(unsigned n=0;n<12000;n++) {
        args[0]=n%13;
        args[1]=1+n%1024;
        for(unsigned i=0;i<args[1];i++) {
            rng=rng*1664525u+1013904223u;
            guest_indices[i]=(uint16_t)(rng>>16);
        }
        if(n%2==0)guest_indices[n%args[1]]=0;
        Trace a=run(driver_original);unsigned original_allocations=allocations;
        Trace b=run(driver_candidate);unsigned candidate_allocations=allocations;
        CHECK(memcmp(&a,&b,sizeof a)==0);
        CHECK(original_allocations==1);
        CHECK(candidate_allocations==(a.first!=0));
        CHECK(frees==candidate_allocations);
    }
    /* Explicit index extrema, repeated references and empty/out-of-range calls. */
    for(unsigned edge=0;edge<5;edge++) {
        args[1]=edge==0?0:edge==1?1025:6;
        for(unsigned i=0;i<6;i++)guest_indices[i]=edge==2?65535:edge==3?0:(uint16_t)(i?65535:0);
        Trace a=run(driver_original),b=run(driver_candidate);
        CHECK(memcmp(&a,&b,sizeof a)==0);
    }
    args[1]=6;args[2]=0;
    Trace absent_a=run(driver_original),absent_b=run(driver_candidate);
    CHECK(memcmp(&absent_a,&absent_b,sizeof absent_a)==0 && absent_b.draws==0);
    args[2]=0x1000;
    /* Invalid outer calls and the ordinary stream path remain unchanged. */
    args[1]=6;
    for(int mode=0;mode<5;mode++) {
        device=mode!=0;readable=mode!=1;gather_ok=mode!=2;
        g_push_active=mode!=3;hle_original_D3DDevice_DrawIndexedVertices=mode!=4;
        Trace a=run(driver_original),b=run(driver_candidate);
        CHECK(memcmp(&a,&b,sizeof a)==0);
    }
    device=readable=gather_ok=hle_original_D3DDevice_DrawIndexedVertices=g_push_active=1;
    /* A nonzero minimum retains allocation failure and draw suppression. */
    for(unsigned i=0;i<6;i++)guest_indices[i]=(uint16_t)(20+i);
    fail_alloc=1;
    Trace a=run(driver_original),b=run(driver_candidate);
    CHECK(memcmp(&a,&b,sizeof a)==0 && b.failed==1 && b.draws==0);
    /* Zero-based draws no longer need that allocation to succeed. */
    guest_indices[0]=0;b=run(driver_candidate);
    CHECK(b.draws==1 && !b.failed && allocations==0);
    fail_alloc=0;
    puts("PASS: 12000 production-driver comparisons, zero/nonzero ranges, fresh indices, invalid inputs, stream path and allocation failure");
    return 0;
}
