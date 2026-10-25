/* Compare complete pushed-attribute gathering, including invalid inputs and
 * fresh guest bytes. Guard pages check common and fallback copy sizes. */
#ifndef _WIN32
#define _DEFAULT_SOURCE 1
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
typedef unsigned int UINT;
#define RAM (1024u*1024u)
#define CONTIG_BASE 0x10000u
static uint8_t *memory;
#define HLE_PTR(va) (memory+(uint32_t)(va))
#define HLE_MEM32(va) (*(uint32_t *)HLE_PTR(va))
static uint32_t g_push_fmt[16],g_push_off[16],g_push_known,object,g_push_failed;
static int fail_alloc;
static unsigned allocations,reads;
static uint64_t read_hash;
static uint32_t hle_d3d8_shadow_program_object(void) {return object;}
static int guest_readable(uint32_t va,uint64_t bytes) {
    reads++;read_hash=(read_hash*33)^va^(bytes<<1);
    return va<RAM && bytes<=RAM-va;
}
static void *tracked_calloc(size_t n,size_t size) {allocations++;return fail_alloc?NULL:calloc(n,size);}
/* FORMAT */
/* REFERENCE */
/* CANDIDATE */
static uint32_t rng=0x12345678;
static uint32_t random_word(void) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void compare(uint32_t first,uint32_t vertices) {
    uint32_t sa=0xfeed,sb=0xfeed;
    allocations=reads=g_push_failed=0;read_hash=0;
    uint8_t *a=push_gather_reference(first,vertices,&sa);
    unsigned na=allocations,ra=reads,fa=g_push_failed;uint64_t ha=read_hash;
    allocations=reads=g_push_failed=0;read_hash=0;
    uint8_t *b=push_gather(first,vertices,&sb);
    assert(na==allocations && ra==reads && fa==g_push_failed && ha==read_hash);
    assert((a==NULL)==(b==NULL) && sa==sb);
    if(a)assert(!memcmp(a,b,(size_t)vertices*sa));
    free(a);free(b);
}
static void *guarded(size_t bytes,size_t page) {
#ifdef _WIN32
    uint8_t *p=VirtualAlloc(NULL,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);DWORD old;
    assert(p && VirtualProtect(p+CONTIG_BASE+page,page,PAGE_NOACCESS,&old));
    assert(VirtualProtect(p+CONTIG_BASE+3*page,page,PAGE_NOACCESS,&old));
#else
    uint8_t *p=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    assert(p!=MAP_FAILED && !mprotect(p+CONTIG_BASE+page,page,PROT_NONE));
    assert(!mprotect(p+CONTIG_BASE+3*page,page,PROT_NONE));
#endif
    return p;
}
static void guard_checks(void) {
#ifdef _WIN32
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
#else
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
#endif
    size_t bytes=CONTIG_BASE+4*page;uint8_t *saved=memory,*src=guarded(bytes,page),*dst=guarded(bytes,page);
    memset(src+CONTIG_BASE,0x5a,page);memset(src+CONTIG_BASE+2*page,0xa5,page);memory=src;
    for(unsigned size=1;size<=64;size++) {
        unsigned va=CONTIG_BASE+(unsigned)page-size;
        uint8_t *out=dst+va;
        push_copy_attribute(out,2*page,va,0,(uint32_t)(2*page),2,size);
        assert(!memcmp(out,src+va,size) && !memcmp(out+2*page,src+va+2*page,size));
    }
    memory=saved;
#ifdef _WIN32
    assert(VirtualFree(src,0,MEM_RELEASE) && VirtualFree(dst,0,MEM_RELEASE));
#else
    assert(!munmap(src,bytes) && !munmap(dst,bytes));
#endif
}
static void ordinary_layout(void) {
    object=0x1000;g_push_known=7;memset(HLE_PTR(object),0,276);
    const unsigned formats[]={0x32,0x32,0x22},offsets[]={0,12,24};
    for(unsigned i=0;i<3;i++) {
        HLE_MEM32(object+20+i*16+4)=offsets[i];HLE_MEM32(object+20+i*16+8)=formats[i];
        g_push_fmt[i]=(16u<<8)|formats[i];g_push_off[i]=0x10000+i*0x8000;
    }
}
static volatile uint32_t sink;
static void benchmark(void) {
    ordinary_layout();
    for(unsigned round=0;round<6;round++)for(unsigned order=0;order<2;order++) {
        unsigned candidate=(round+order)&1;uint8_t *(*volatile fn)(uint32_t,uint32_t,uint32_t *)=candidate?push_gather:push_gather_reference;
        clock_t start=clock();
        for(unsigned n=0;n<2000000;n++) {
            HLE_PTR(CONTIG_BASE+g_push_off[0])[0]=(uint8_t)n;
            uint32_t stride;uint8_t *out=fn(0,96,&stride);assert(out && stride==32);
            sink+=out[0];free(out);
        }
        printf("bench round=%u candidate=%u seconds=%.9f checksum=%u\n",round,candidate,(double)(clock()-start)/CLOCKS_PER_SEC,sink);
    }
}
int main(int argc,char **argv) {
    (void)argv;memory=malloc(RAM);assert(memory);
    for(unsigned i=0;i<RAM;i++)memory[i]=(uint8_t)random_word();
    if(argc>1){benchmark();free(memory);return 0;}
    const unsigned formats[]={0,1,2,0x12,0x22,0x32,0x42,0x72,0x11,0x21,0x31,0x41,0x14,0x24,0x34,0x44,0x15,0x25,0x35,0x45,0x16,0xf2,0xff};
    for(unsigned trial=0;trial<12000;trial++) {
        object=0x1000;g_push_known=random_word()&65535;memset(HLE_PTR(object),0,276);
        for(unsigned i=0;i<16;i++) {
            HLE_MEM32(object+20+i*16+4)=random_word()%260;
            HLE_MEM32(object+20+i*16+8)=formats[random_word()%(sizeof(formats)/sizeof(formats[0]))];
            g_push_fmt[i]=((random_word()%65)<<8)|formats[random_word()%(sizeof(formats)/sizeof(formats[0]))];
            g_push_off[i]=0x10000+i*0x8000+(random_word()%4);
        }
        if(trial%5==0)ordinary_layout();
        if(trial%17==0)g_push_off[trial%16]=RAM-1;
        if(trial%19==0)object=RAM-128;
        if(trial%23==0)object=0;
        fail_alloc=trial%29==0;
        compare(random_word()%5,random_word()%258);
        HLE_PTR(0x20000)[trial%256]^=0xff;compare(0,1+trial%96);
    }
    ordinary_layout();fail_alloc=0;g_push_known=0;compare(0,17);
    guard_checks();free(memory);
    puts("PASS: complete pushed arrays, fresh bytes, formats/offsets/holes, errors, allocation failure and guard-page boundaries");
    return 0;
}
