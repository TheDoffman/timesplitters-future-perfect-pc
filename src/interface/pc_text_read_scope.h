#ifndef TSFP_PC_TEXT_READ_SCOPE_H
#define TSFP_PC_TEXT_READ_SCOPE_H
#include "pc_guest_span.h"

/* Only for synchronous text layout: no event dispatch or mapping/protection
 * changes may occur inside a scope. Never retain VM facts across layout calls.
 * Keep separate intervals because text and guest stack can have different
 * protections. Cache reads only; scratch writes always use the live query. */
typedef struct PcTextReadScope {
    unsigned depth, next;
    struct { uintptr_t first; size_t bytes; } regions[8];
} PcTextReadScope;

static inline void pc_text_read_begin(PcTextReadScope *scope)
{
    if (!scope->depth++) {
        scope->next=0;
        for(unsigned i=0;i<8;i++) scope->regions[i].bytes=0;
    }
}
static inline void pc_text_read_end(PcTextReadScope *scope)
{ if(scope->depth) --scope->depth; }

/* A text scan can consume successive validated chunks; a guest call frame
 * instead requires its entire known extent before dereferencing arguments. */
static inline size_t pc_text_read_span(PcTextReadScope *scope,uintptr_t address,size_t minimum)
{
    if(!minimum || minimum>UINTPTR_MAX-address)return 0;
    if(scope->depth) for(unsigned i=0;i<8;i++) {
        uintptr_t first=scope->regions[i].first;
        if(address>=first && address-first<scope->regions[i].bytes &&
           minimum<=scope->regions[i].bytes-(address-first))
            return scope->regions[i].bytes-(address-first);
    }
    PcGuestRegion region=pc_guest_query_span(address,minimum);
    size_t bytes=pc_guest_region_remaining(&region,address,0);
    if(bytes<minimum)return 0;
    if(scope->depth && bytes) {
        unsigned i=scope->next++%8;
        /* Cache the OS-validated interval, not only its suffix. Guest stack
           reads descend and adjacent labels need not be visited in order. */
        scope->regions[i].first=region.base;scope->regions[i].bytes=region.size;
    }
    return bytes;
}
static inline size_t pc_text_read_region(PcTextReadScope *scope,uintptr_t address)
{ return pc_text_read_span(scope,address,1); }
#endif
