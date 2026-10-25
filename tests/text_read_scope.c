#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define TSFP_PC_GUEST_MEMORY_H
#define TSFP_PC_GUEST_SPAN_H
#define pc_guest_query_span(address,minimum) pc_guest_query_region(address)
static unsigned queries;
static uintptr_t base=0x1000;
static size_t length=0x1000;
typedef struct PcGuestRegion {uintptr_t base;size_t size;int writable;} PcGuestRegion;
static inline PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    ++queries;
    PcGuestRegion result={0,0,0};
    if(address>=base && address-base<length) {result.base=base;result.size=length;}
    return result;
}
static inline size_t pc_guest_region_remaining(const PcGuestRegion *region,uintptr_t address,int writing)
{
    return !writing && address>=region->base && address-region->base<region->size ?
        region->size-(address-region->base):0;
}
#include "pc_text_read_scope.h"
#define CHECK(x) do {if(!(x)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)
int main(void)
{
    PcTextReadScope scope={0};
    pc_text_read_begin(&scope);
    /* Guest stack addresses descend; labels can share a mapping in any order. */
    for(unsigned i=999;i>0;i--) CHECK(pc_text_read_region(&scope,base+i)==length-i);
    CHECK(pc_text_read_region(&scope,base)==length);
    CHECK(queries==1);
    CHECK(!pc_text_read_span(&scope,base+length-4,8));
    CHECK(!pc_text_read_span(&scope,UINTPTR_MAX-4,8));
    CHECK(!pc_text_read_region(&scope,base-1)); /* Never extend below the mapping. */
    pc_text_read_end(&scope);
    queries=0;
    pc_text_read_begin(&scope);
    for(unsigned i=0;i<1000;i++) CHECK(pc_text_read_region(&scope,base+i)==length-i);
    CHECK(queries==1); /* A word wrapper's successive prefixes/suffixes. */
    pc_text_read_begin(&scope);
    CHECK(pc_text_read_region(&scope,base+2)==length-2 && queries==1);
    pc_text_read_end(&scope);
    CHECK(pc_text_read_region(&scope,base+3)==length-3 && queries==1);
    CHECK(!pc_text_read_region(&scope,base+length)); /* No crossing the region. */
    CHECK(!pc_text_read_region(&scope,UINTPTR_MAX));
    pc_text_read_end(&scope);
    length=0; /* Protection/unmap changes between layout calls must be observed. */
    CHECK(!pc_text_read_region(&scope,base));
    pc_text_read_begin(&scope);
    CHECK(!pc_text_read_region(&scope,base));
    pc_text_read_end(&scope);
    length=0x1000;
    pc_text_read_begin(&scope);
    for(unsigned i=0;i<20;i++) {base+=0x2000;CHECK(pc_text_read_region(&scope,base)==length);}
    pc_text_read_end(&scope);
    puts("PASS: bounded layout queries, nested scopes, mapping boundaries and invalidation");
    return 0;
}
