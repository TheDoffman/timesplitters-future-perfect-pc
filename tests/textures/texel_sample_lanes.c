/* Preserve the original checksum and exact sample footprint, including tails. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TSFP_TEST_FORTIFIED_COPY
static void *fortified_copy(void *dst,const void *src,size_t size) { return memcpy(dst,src,size); }
#undef memcpy
#define memcpy(dst,src,size) fortified_copy(dst,src,size)
#endif
typedef int D3DFORMAT;
typedef struct { uint32_t fmt,width,height,levels,guest_pitch;int linear;uint32_t phys,bytes; } texture_layout;
static unsigned char *memory;
static size_t start,extent,reads[2048],read_count;
static int failed;
#define CONTIG_BASE 0u
#define HLE_PTR(a) (memory+(a))
static uint32_t level_rows(uint32_t fmt,uint32_t h) { return fmt?(h+3)/4:h; }
static uint32_t d3d8_row_pitch(D3DFORMAT fmt,uint32_t width) { return fmt?((width+3)/4)*8:width; }
static void *checked_copy(void *dst,const void *src,size_t size)
{
    size_t offset=(size_t)((const unsigned char *)src-memory);
    if (size!=8 || offset<start || offset-start>extent || size>extent-(offset-start) || read_count>=2048) {
        failed=1;memset(dst,0,size);return dst;
    }
    reads[read_count++]=offset-start;
    return memcpy(dst,src,size);
}
/* PRODUCTION_SAMPLE */

/* Frozen scalar recurrence from the original four-lane implementation. */
static uint32_t reference(const unsigned char *p,uint32_t n)
{
    uint64_t h[4]={0xCBF29CE484222325ull,0x84222325CBF29CE4ull,
        0x9E3779B97F4A7C15ull,0x7F4A7C159E3779B9ull},word;
    uint32_t words=n/8,step=words>1024?words/1024:1;
    if (!words) {
        uint32_t sum=2166136261u;
        for(uint32_t i=0;i<n;i++)sum=(sum^p[i])*16777619u;
        return sum;
    }
    for(uint32_t i=0,lane=0;i<words;i+=step,lane=(lane+1)&3) {
        checked_copy(&word,p+(size_t)i*8,8);
        h[lane]=(h[lane]^word)*0x100000001B3ull;
    }
    word=h[0]^(h[1]*3)^(h[2]*5)^(h[3]*7);
    return (uint32_t)(word^(word>>32));
}
static int check(texture_layout t)
{
    size_t expected_reads[2048];
    uint32_t n=(t.linear?t.guest_pitch:d3d8_row_pitch((D3DFORMAT)t.fmt,t.width))*level_rows(t.fmt,t.height);
    start=t.phys;extent=n;read_count=0;failed=0;
    uint32_t expected=reference(memory+start,n);
    size_t count=read_count;memcpy(expected_reads,reads,count*sizeof(size_t));
    read_count=0;
    uint32_t actual=texel_sample(&t);
    if (failed || actual!=expected || read_count!=count || memcmp(expected_reads,reads,count*sizeof(size_t))) {
        fprintf(stderr,"FAIL: length=%u linear=%d format=%u checksum/footprint/bounds\n",n,t.linear,t.fmt);return 0;
    }
    return 1;
}
int main(void)
{
    const size_t capacity=64u*1024*1024;
    memory=malloc(capacity+8);if(!memory)return 2;
    uint32_t random=0x371b5281;
    for(size_t i=0;i<capacity+8;i++) { random=random*1664525u+1013904223u;memory[i]=(unsigned char)(random>>24); }
    for(uint32_t n=0;n<32776;n++) {
        texture_layout t={0,0,1,1,n,1,n&7,n};
        if(!check(t))return 1;
    }
    /* Step changes, partial groups, long strides, and unaligned sources. */
    for(uint32_t step=2;step<=8192;step*=2)for(int delta=-9;delta<=9;delta++) {
        uint32_t n=(uint32_t)((int)(step*8192)+delta);
        if(n>capacity)continue;
        texture_layout t={0,0,1,1,n,1,3,n};
        if(!check(t))return 1;
        memory[3+n/2]^=0xa5;
        if(!check(t))return 1;
    }
    for(unsigned trial=0;trial<256;trial++) {
        random=random*1664525u+1013904223u;
        uint32_t n=random%((uint32_t)capacity+1);
        texture_layout t={0,0,1,1,n,1,trial&7,n};
        if(!check(t))return 1;
    }
    for(uint32_t format=0;format<=1;format++)for(uint32_t height=1;height<=17;height++)for(uint32_t width=1;width<=2049;width+=127) {
        texture_layout t={format,width,height,1,0,0,5,0};
        if(!check(t))return 1;
    }
    free(memory);puts("PASS: scalar checksum parity, identical sample positions/order, bounds, lengths, strides and compressed rows");return 0;
}
