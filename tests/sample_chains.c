/* Actual sampler key insertion, without OS thread suspension or game data. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* PRODUCTION_CHAIN_TABLE */
int main(void) {
    SampleChain *table=calloc(SAMPLE_CHAIN_SLOTS,sizeof *table);
    assert(table);
    uintptr_t frames[32];
    for(unsigned i=0;i<32;i++)frames[i]=0x1000u+i*16u;
    unsigned used=0;
    assert(chain_add(table,&used,frames,32));
    frames[7]+=4;
    assert(chain_add(table,&used,frames,32));
    assert(used==2); /* Same six nearest frames, different gameplay caller. */
    frames[31]+=4;
    assert(chain_add(table,&used,frames,32));
    assert(used==3); /* Preserve the maximum configured capture depth too. */
    assert(chain_add(table,&used,frames,32) && used==3);
    unsigned samples=0;
    for(unsigned i=0;i<SAMPLE_CHAIN_SLOTS;i++)samples+=table[i].count;
    assert(samples==4);
    /* Ignore unobserved trailing storage, but distinguish short/long stacks. */
    assert(chain_add(table,&used,frames,1));
    frames[7]+=4;
    assert(chain_add(table,&used,frames,1) && used==4);
    /* Full table reports loss without evicting earlier evidence. Existing
       keys must still accumulate even when no new key can be inserted. */
    memset(table,0,SAMPLE_CHAIN_SLOTS*sizeof *table);used=0;
    for(unsigned i=0;i<SAMPLE_CHAIN_SLOTS;i++) {
        frames[0]=0x10000u+(uintptr_t)i*4;
        assert(chain_add(table,&used,frames,1));
    }
    assert(used==SAMPLE_CHAIN_SLOTS);
    frames[0]=0x10000u+(uintptr_t)SAMPLE_CHAIN_SLOTS*4;
    assert(!chain_add(table,&used,frames,1) && used==SAMPLE_CHAIN_SLOTS);
    frames[0]=0x10000u;
    assert(chain_add(table,&used,frames,1));
    samples=0;
    for(unsigned i=0;i<SAMPLE_CHAIN_SLOTS;i++)samples+=table[i].count;
    assert(samples==SAMPLE_CHAIN_SLOTS+1);
    free(table);
    puts("PASS: deep caller keys, short stacks, full-table accounting and retained samples");
    return 0;
}
