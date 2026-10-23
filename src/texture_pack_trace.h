/* Opt-in render-thread diagnostics, shared by common RHI and legacy GL.
 * CPU wall spans include scheduling/I/O waits, not GPU completion. Logging
 * follows the measured end and can perturb subsequent frames. No clock/file
 * work when disabled. Hashing is measured even on a cached upload. */
#ifndef TSFP_TEXTURE_PACK_TRACE_H
#define TSFP_TEXTURE_PACK_TRACE_H
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <time.h>
#endif

#define TSFP_TEXTURE_TRACE_ROWS 8192u
#define TSFP_TEXTURE_TRACE_BYTES (4u*1024u*1024u)
typedef struct {
    int active, enhanced;
    const char *backend;
    uint64_t key, begin, hash_begin, hash_end, read_begin, open_end, read_end, validated;
    uint64_t image_begin, image_end, view_begin, view_end;
    uint32_t width, height, source_bytes;
} TsfpTextureTrace;
static struct {
    int initialized;
    FILE *file;
    unsigned rows, bytes;
} tsfp_texture_trace_state;

static uint64_t tsfp_texture_trace_now(void) {
#ifdef _WIN32
    LARGE_INTEGER n,f;
    if(!QueryPerformanceCounter(&n)||!QueryPerformanceFrequency(&f))return 0;
    return (uint64_t)(n.QuadPart/f.QuadPart)*UINT64_C(1000000000)+
        (uint64_t)(n.QuadPart%f.QuadPart)*UINT64_C(1000000000)/(uint64_t)f.QuadPart;
#else
    struct timespec n;
    if(clock_gettime(CLOCK_MONOTONIC,&n))return 0;
    return (uint64_t)n.tv_sec*UINT64_C(1000000000)+(uint64_t)n.tv_nsec;
#endif
}
static void tsfp_texture_trace_close(void) {
    if(tsfp_texture_trace_state.file)fclose(tsfp_texture_trace_state.file);
    tsfp_texture_trace_state.file=NULL;
}
static TsfpTextureTrace tsfp_texture_trace_begin(uint64_t key,uint32_t w,
        uint32_t h,uint32_t source_bytes,const char *backend) {
    TsfpTextureTrace t={0};
    if(!tsfp_texture_trace_state.initialized) {
        const char *path=getenv("TSFP_TEXTURE_UPLOAD_TRACE");
        tsfp_texture_trace_state.initialized=1;
        if(path && *path) {
            FILE *f=fopen(path,"wb");tsfp_texture_trace_state.file=f;
            if(f) {
                const char *header="key,backend,enhanced,width,height,source_bytes,replacement_bytes,outcome,begin_ns,hash_begin_ns,hash_end_ns,read_begin_ns,open_end_ns,read_end_ns,validated_ns,image_begin_ns,image_end_ns,view_begin_ns,view_end_ns,end_ns\n";
                setvbuf(f,NULL,_IOFBF,64u*1024u);
                int bytes=fprintf(f,"%s",header);
                if(bytes<0)tsfp_texture_trace_close();
                else {tsfp_texture_trace_state.bytes=(unsigned)bytes;atexit(tsfp_texture_trace_close);}
            }
        }
    }
    if(!tsfp_texture_trace_state.file)return t;
    t.active=1;t.key=key;t.width=w;t.height=h;t.source_bytes=source_bytes;
    t.backend=backend;t.begin=tsfp_texture_trace_now();return t;
}
#define TSFP_TEXTURE_STAMP(t,field) do { \
    if((t) && (t)->active)(t)->field=tsfp_texture_trace_now(); \
} while(0)
static void tsfp_texture_trace_finish(TsfpTextureTrace *t,const char *outcome,uint64_t bytes) {
    if(!t->active || !tsfp_texture_trace_state.file)return;
    uint64_t end=tsfp_texture_trace_now();
    int n=fprintf(tsfp_texture_trace_state.file,
        "%016" PRIx64 ",%s,%d,%u,%u,%u,%" PRIu64 ",%s,%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
        ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
        t->key,t->backend,t->enhanced,t->width,t->height,t->source_bytes,bytes,outcome,
        t->begin,t->hash_begin,t->hash_end,t->read_begin,t->open_end,t->read_end,t->validated,
        t->image_begin,t->image_end,t->view_begin,t->view_end,end);
    if(n<0) {tsfp_texture_trace_close();return;}
    tsfp_texture_trace_state.bytes+=(unsigned)n;
    if(++tsfp_texture_trace_state.rows>=TSFP_TEXTURE_TRACE_ROWS ||
       tsfp_texture_trace_state.bytes>=TSFP_TEXTURE_TRACE_BYTES-512u)tsfp_texture_trace_close();
    else if(tsfp_texture_trace_state.rows%64u==0 && fflush(tsfp_texture_trace_state.file))
        tsfp_texture_trace_close();
}
#endif
