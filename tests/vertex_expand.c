/* Complete conversion, fresh/aliased inputs, failure/FP parity and guard pages. */
#ifndef _WIN32
#define _DEFAULT_SOURCE 1
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
typedef unsigned int DWORD;
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fenv.h>
typedef unsigned UINT;typedef struct {int unused;} shadow_decl_tokens;
#define SHADOW_MAX_PACKED 8
/* DEFINITIONS */
#define NO_FIRST_VERTEX 0xffffffffu
static struct shadow_program g_programs[1];
static int g_shadow_vs_is_program=1,g_shadow_vs_slot;
static unsigned alloc_count,stream_count;static uint64_t stream_hash;
static int fail_alloc,missing_stream;
static uint8_t sources[3][32768];static uint32_t strides[3];
static uint8_t *source_override[3],*input_override;
static uint8_t *guard_base[2],*guard_pointer[2];static unsigned guard_used;static int guard_output;
static size_t page_size;
static uint8_t *guard_mapping(void) {
#ifdef _WIN32
    uint8_t *base=VirtualAlloc(NULL,2*page_size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);DWORD old;
    assert(base&&VirtualProtect(base+page_size,page_size,PAGE_NOACCESS,&old));
#else
    uint8_t *base=mmap(NULL,2*page_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    assert(base!=MAP_FAILED&&!mprotect(base+page_size,page_size,PROT_NONE));
#endif
    return base;
}
static void guard_release(uint8_t *base) {
#ifdef _WIN32
    assert(VirtualFree(base,0,MEM_RELEASE));
#else
    assert(!munmap(base,2*page_size));
#endif
}
static void *tracked_calloc(size_t n,size_t size){
    ++alloc_count;if(fail_alloc)return NULL;
    if(!guard_output)return calloc(n,size);
    assert(n&&size&&n*size<=page_size&&guard_used<2);
    uint8_t *base=guard_mapping();
    uint8_t *p=base+page_size-n*size;guard_base[guard_used]=base;guard_pointer[guard_used++]=p;return p;
}
static void tracked_free(void *p){
    for(unsigned i=0;i<guard_used;i++)if(p&&p==guard_pointer[i]){guard_release(guard_base[i]);guard_pointer[i]=NULL;return;}
    free(p);
}
#define free tracked_free
static const void *hle_d3d8_stream_vertices(uint32_t stream,uint32_t first,uint32_t vertices,uint32_t *stride){
    assert(stream<3);++stream_count;stream_hash=stream_hash*33+stream+first*7+vertices*15;
    *stride=strides[stream];return missing_stream==(int)stream?NULL:(source_override[stream]?source_override[stream]:sources[stream])+(size_t)first*strides[stream];
}
/* REFERENCE */
/* CANDIDATE */
static uint32_t random_state=0x6342abcdu;
static uint32_t random_word(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static void compare(unsigned vertices,unsigned input_stride,uint32_t first){
    UINT a_stride=input_stride,b_stride=input_stride;int a_failed,b_failed;
    alloc_count=stream_count=0;stream_hash=0;
    feclearexcept(FE_ALL_EXCEPT);
    uint8_t *a=reference(input_override?input_override:sources[0],vertices,&a_stride,first,&a_failed);
    int exceptions=fetestexcept(FE_ALL_EXCEPT);
    unsigned allocations=alloc_count,streams=stream_count;uint64_t hash=stream_hash;
    alloc_count=stream_count=0;stream_hash=0;
    feclearexcept(FE_ALL_EXCEPT);
    uint8_t *b=candidate(input_override?input_override:sources[0],vertices,&b_stride,first,&b_failed);
    assert(exceptions==fetestexcept(FE_ALL_EXCEPT));
    assert(a_failed==b_failed&&a_stride==b_stride&&(a==NULL)==(b==NULL));
    assert(allocations==alloc_count&&streams==stream_count&&hash==stream_hash);
    if(a)assert(!memcmp(a,b,(size_t)vertices*a_stride));free(a);free(b);
}
static void guard_checks(void){
#ifdef _WIN32
    SYSTEM_INFO info;GetSystemInfo(&info);page_size=info.dwPageSize;
#else
    page_size=(size_t)sysconf(_SC_PAGESIZE);
#endif
    uint8_t *maps[2];
    for(unsigned i=0;i<2;i++){
        maps[i]=guard_mapping();
        memset(maps[i],0xa7+i,page_size);
    }
    fail_alloc=missing_stream=0;g_ff_active=0;g_shadow_vs_is_program=1;g_shadow_vs_slot=0;
    for(unsigned size=1;size<=64;size++){
        memset(g_programs,0,sizeof g_programs);struct shadow_program *p=g_programs;
        p->packed_count=1;p->packed_stream[0]=1;p->packed_raw[0]=1;p->packed_size[0]=size;p->expanded_bytes=128;
        strides[1]=size;input_override=maps[0]+page_size-4*size;source_override[1]=maps[1]+page_size-4*size;
        guard_used=0;guard_output=1;compare(4,size,0);assert(guard_used==2&&!guard_pointer[0]&&!guard_pointer[1]);guard_output=0;
    }
    for(unsigned components=1;components<=4;components++){
        struct shadow_program *p=g_programs;p->packed_raw[0]=0;p->packed_format[0]=(components<<4)|5;p->packed_size[0]=components*2;
        strides[1]=components*2;input_override=maps[0]+page_size-64;source_override[1]=maps[1]+page_size-4*strides[1];
        guard_used=0;guard_output=1;compare(4,16,0);assert(guard_used==2);guard_output=0;
    }
    g_programs[0].packed_format[0]=0x16;g_programs[0].packed_size[0]=4;strides[1]=4;source_override[1]=maps[1]+page_size-16;
    guard_used=0;guard_output=1;compare(4,16,0);assert(guard_used==2);guard_output=0;
    g_programs[0].packed_format[0]=0x05;g_programs[0].packed_size[0]=0;strides[1]=0;source_override[1]=maps[1]+page_size;
    guard_used=0;guard_output=1;compare(4,16,0);assert(guard_used==2);guard_output=0;
    input_override=source_override[1]=NULL;
    for(unsigned i=0;i<2;i++)guard_release(maps[i]);
}
static void configure(unsigned fixed,unsigned stride){
    static const uint32_t formats[]={0x12,0x22,0x32,0x42,0x11,0x21,0x31,0x41,0x15,0x25,0x35,0x45,0x14,0x24,0x34,0x44,0x16,0x40};
    memset(&g_ff,0,sizeof g_ff);memset(g_programs,0,sizeof g_programs);g_ff_active=(int)fixed;
    g_shadow_vs_is_program=1;g_shadow_vs_slot=0;missing_stream=0;
    for(unsigned stream=0;stream<3;stream++)strides[stream]=stride;
    if(fixed){
        g_ff.count=1+(int)(random_word()%8);g_ff.out_stride=160;
        for(int k=0;k<g_ff.count;k++){
            uint32_t fmt=formats[random_word()%(sizeof(formats)/sizeof(*formats))];
            unsigned components=fmt==0x16?3:fmt>>4;
            unsigned bytes=ff_format_size(fmt);
            g_ff.stream[k]=(uint8_t)(random_word()%3);g_ff.offset[k]=random_word()%(stride-bytes+1);
            g_ff.format[k]=fmt;g_ff.floats[k]=(int)components;g_ff.out[k]=random_word()%(160-components*4+1);
            g_ff.as_color[k]=(int)((fmt&15)!=2&&(random_word()%2));
        }
    }else{
        struct shadow_program *p=g_programs;p->packed_count=1+(int)(random_word()%8);p->expanded_bytes=160;
        for(int k=0;k<p->packed_count;k++){
            static const unsigned sizes[]={1,2,3,4,6,8,12,16,17};
            unsigned raw=random_word()%2,components=1+random_word()%4;
            unsigned bytes=raw?sizes[random_word()%9]:2*components;
            p->packed_raw[k]=(uint8_t)raw;p->packed_size[k]=bytes;
            p->packed_format[k]=(components<<4)|5;
            if(!raw&&random_word()%2){p->packed_format[k]=0x16;bytes=4;components=3;}
            p->packed_stream[k]=(uint8_t)(random_word()%3);
            p->packed_offset[k]=random_word()%(stride-bytes+1);
            p->packed_out[k]=random_word()%(160-(raw?bytes:components*4)+1);
        }
    }
}
static void bench_setup(int scenario){
    memset(&g_ff,0,sizeof g_ff);memset(g_programs,0,sizeof g_programs);
    for(unsigned stream=0;stream<3;stream++)strides[stream]=scenario==2?50:32;
    missing_stream=0;g_ff_active=scenario==1;
    if(scenario==1){
        g_ff.count=4;g_ff.out_stride=48;
        for(int k=0;k<4;k++){g_ff.stream[k]=(uint8_t)(k%3);g_ff.format[k]=0x32;g_ff.floats[k]=3;g_ff.out[k]=(UINT)k*12;g_ff.offset[k]=0;}
    }else{
        struct shadow_program *p=g_programs;p->packed_count=3;p->expanded_bytes=32;
        p->packed_stream[0]=1;p->packed_raw[0]=1;p->packed_size[0]=16;
        p->packed_stream[1]=2;p->packed_raw[1]=1;p->packed_size[1]=4;p->packed_out[1]=16;
        p->packed_format[2]=0x16;p->packed_out[2]=20;
    }
}
static double clock_seconds(void) {
#ifdef _WIN32
    LARGE_INTEGER f,n;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&n);return (double)n.QuadPart/f.QuadPart;
#else
    struct timespec n;assert(!clock_gettime(CLOCK_MONOTONIC,&n));return n.tv_sec+n.tv_nsec/1e9;
#endif
}
int main(int argc,char **argv){
    for(unsigned s=0;s<3;s++)for(unsigned i=0;i<sizeof sources[s];i++)sources[s][i]=(uint8_t)random_word();
    if(argc==3){
        int scenario=atoi(argv[1]),enabled=atoi(argv[2]);bench_setup(scenario);
        UINT stride=scenario==2?50:32;
        uint8_t *(*convert)(const void*,UINT,UINT*,uint32_t,int*)=enabled?candidate:reference;
        double start=clock_seconds();
        volatile uint32_t checksum=0;
        for(unsigned draw=0;draw<600000;draw++){UINT out_stride=stride;int failed;uint8_t *out=convert(sources[0],96,&out_stride,0,&failed);assert(out&&!failed);checksum+=out[draw%32];free(out);}
        printf("%.9f\n",clock_seconds()-start);return 0;
    }
    assert(argc==1);
    int modes[]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    for(unsigned mode=0;mode<4;mode++){
      assert(!fesetround(modes[mode]));
      for(unsigned trial=0;trial<12000;trial++){
        unsigned stride=20+random_word()%77;configure(trial%2,stride);
        source_override[1]=trial%7==0?sources[0]+1:NULL;
        source_override[2]=trial%11==0?sources[0]+3:NULL;
        sources[random_word()%3][random_word()%32768]^=0x5a;
        fail_alloc=trial%17==0;missing_stream=trial%19==0?1:0;
        if(trial%23==0)strides[1]=1;
        if(trial%29==0)g_shadow_vs_is_program=0;
        if(trial%31==0)g_shadow_vs_slot=-1;
        if(trial%37==0)g_programs[0].packed_count=0;
        compare(trial%129,stride,trial%13==0?NO_FIRST_VERTEX:trial%7);
      }
    }
    assert(!fesetround(FE_TONEAREST));source_override[1]=source_override[2]=NULL;guard_checks();
    puts("PASS:48000 full-function output/stride/failure/stream/allocation/FP comparisons and source/destination guard pages");return 0;
}
