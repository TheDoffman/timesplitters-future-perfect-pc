#include "memory_statistics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t available_bytes;
static int failure;
static unsigned calls;
static int available(void *context,uint64_t *bytes) {
    assert(context==&calls);++calls;
    *bytes=available_bytes;
    return !failure;
}
int main(void) {
    uint32_t words[9],before[9];
    const uint64_t counts[]={0,1,4095,4096,67108863,67108864,67112960,
        UINT64_C(0x100000000)*4096,UINT64_C(0x100000003)*4096,UINT64_MAX};
    const uint32_t expected[]={0,0,0,1,16383,16384,8192,8192,8192,8192};
    memset(before,0x35,sizeof(before));memcpy(words,before,sizeof(words));
    assert(tsfp_memory_statistics(NULL,&calls,words)==UINT32_C(0xc000000d));
    assert(tsfp_memory_statistics(available,&calls,NULL)==UINT32_C(0xc000000d));
    assert(!calls && !memcmp(words,before,sizeof(words)));
    failure=1;available_bytes=UINT64_MAX;
    assert(tsfp_memory_statistics(available,&calls,words)==UINT32_C(0xc0000001));
    assert(calls==1 && !memcmp(words,before,sizeof(words)));
    failure=0;
    for(size_t i=0;i<sizeof(counts)/sizeof(*counts);++i) {
        available_bytes=counts[i];memcpy(words,before,sizeof(words));
        assert(!tsfp_memory_statistics(available,&calls,words));
        assert(words[0]==36 && words[1]==16384 && words[2]==expected[i]);
        for(unsigned j=3;j<9;++j)assert(!words[j]);
    }
    puts("guest-width memory statistics contracts passed");return 0;
}
