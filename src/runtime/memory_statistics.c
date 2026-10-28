#include "memory_statistics.h"
#include <string.h>

uint32_t tsfp_memory_statistics(tsfp_memory_available available,void *context,
    uint32_t words[TSFP_MEMORY_STATISTICS_WORDS]) {
    uint64_t bytes=0,pages;
    uint32_t snapshot[TSFP_MEMORY_STATISTICS_WORDS]={0};
    if(!available || !words)return UINT32_C(0xc000000d);
    if(!available(context,&bytes))return UINT32_C(0xc0000001);
    pages=bytes/4096u;
    snapshot[0]=TSFP_MEMORY_STATISTICS_BYTES;
    snapshot[1]=16384;
    snapshot[2]=pages>16384?8192:(uint32_t)pages;
    memcpy(words,snapshot,sizeof(snapshot));return 0;
}
