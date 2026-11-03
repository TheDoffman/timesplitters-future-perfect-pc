/* Original group-local affinity selection. No operating-system layout parsing. */
#ifndef TSFP_GUEST_CPU_CHOICE_H
#define TSFP_GUEST_CPU_CHOICE_H
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
typedef struct tsfp_cpu_choice {uint64_t mask;unsigned rank;} tsfp_cpu_choice;
static inline void tsfp_cpu_consider(tsfp_cpu_choice *choice,uint64_t allowed,
                                     unsigned group,unsigned index,unsigned rank) {
    uint64_t bit;
    if(group || index>=64)return;bit=UINT64_C(1)<<index;
    if(!(allowed&bit))return;
    if(!choice->mask || rank>choice->rank || (rank==choice->rank && bit>choice->mask)) {
        choice->mask=bit;choice->rank=rank;
    }
}
static inline uint64_t tsfp_cpu_request(const char *text,uint64_t allowed,uint64_t preferred) {
    unsigned long long requested;char *end;uint64_t bit;
    if(!text || !*text || *text=='-')return 0;
    errno=0;requested=strtoull(text,&end,0);
    if(errno || end==text || *end || !requested)return 0;
    if(requested==1) {
        if(preferred && !(preferred&(preferred-1)) && (preferred&allowed))return preferred;
        for(bit=UINT64_C(1)<<63;bit;bit>>=1)if(bit&allowed)return bit;
        return 0;
    }
    if(requested>65)return 0;
    return (UINT64_C(1)<<(unsigned)(requested-2))&allowed;
}
#endif
