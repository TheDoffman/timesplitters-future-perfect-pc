#include "host_blocks.h"
#include <string.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif

static void enter(tsfp_host_blocks *s) {
#ifdef _MSC_VER
    while (_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while (__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_host_blocks *s) {
#ifdef _MSC_VER
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int power_two(size_t n) { return n && !(n & (n-1)); }
static int span(const void *p,size_t n) { return p && n && (uintptr_t)p<=UINTPTR_MAX-n; }
static int overlap(const void *a,size_t an,const void *b,size_t bn) {
    return (uintptr_t)a<(uintptr_t)b+bn && (uintptr_t)b<(uintptr_t)a+an;
}
int tsfp_host_blocks_init(tsfp_host_blocks *s,const tsfp_host_block_backend *backend,
    size_t page_size,tsfp_host_block *records,size_t capacity) {
    tsfp_host_block_backend copy;
    size_t record_bytes;
    if (!span(s,sizeof(*s)) || !backend || !backend->map || !backend->unmap ||
        !power_two(page_size) || !capacity || capacity>SIZE_MAX/sizeof(*records)) return 0;
    record_bytes=capacity*sizeof(*records);
    if (!span(records,record_bytes) || overlap(s,sizeof(*s),records,record_bytes) ||
        s->active || s->live) return 0;
    copy=*backend;
    memset(s,0,sizeof(*s)); memset(records,0,record_bytes);
    s->backend=copy; s->records=records; s->capacity=capacity;
    s->page_size=page_size; s->active=1;
    return 1;
}
void *tsfp_host_block_allocate(tsfp_host_blocks *s,size_t bytes,
    size_t alignment,uint32_t protection,uint32_t kind) {
    size_t size=bytes?bytes:1, extra, rounded, slot;
    uintptr_t raw, aligned;
    void *mapping, *result=NULL;
    tsfp_host_block record;
    if (!s || !kind) return NULL;
    enter(s);
    if (!s->active || s->live==s->capacity) goto done;
    if (!alignment) alignment=s->page_size;
    if (!power_two(alignment) || size>SIZE_MAX-(s->page_size-1)) goto done;
    rounded=(size+s->page_size-1)&~(s->page_size-1);
    extra=alignment>s->page_size?alignment-s->page_size:0;
    if (rounded>SIZE_MAX-extra) goto done;
    rounded+=extra;
    for (slot=0;slot<s->capacity && s->records[slot].mapping;++slot) { }
    if (slot==s->capacity) goto done;
    mapping=s->backend.map(s->backend.context,rounded,protection);
    if (!mapping) goto done;
    memset(&record,0,sizeof(record));
    record.mapping=mapping; record.mapping_size=rounded; record.kind=kind;
    /* Even a broken backend result remains owned until unmap confirms release.
     * It is never returned to a caller or confused with another record. */
    s->records[slot]=record; ++s->live;
    raw=(uintptr_t)mapping;
    if ((raw & (s->page_size-1)) || raw>UINTPTR_MAX-rounded ||
        raw>UINTPTR_MAX-(alignment-1)) {
        if (s->backend.unmap(s->backend.context,mapping,rounded)) {
            memset(s->records+slot,0,sizeof(record)); --s->live;
        }
        goto done;
    }
    aligned=(raw+alignment-1)&~((uintptr_t)alignment-1);
    if (aligned-raw>rounded-size) {
        if (s->backend.unmap(s->backend.context,mapping,rounded)) {
            memset(s->records+slot,0,sizeof(record)); --s->live;
        }
        goto done;
    }
    s->records[slot].address=(void *)aligned;
    s->records[slot].bytes=bytes;
    result=(void *)aligned;
done:
    leave(s); return result;
}
int tsfp_host_block_release(tsfp_host_blocks *s,void *address,uint32_t kind) {
    int released=0;
    if (!s || !address) return 0;
    enter(s);
    for (size_t i=0;i<s->capacity;++i) {
        tsfp_host_block *r=s->records+i;
        if (r->address!=address || (kind && r->kind!=kind)) continue;
        if (s->backend.unmap(s->backend.context,r->mapping,r->mapping_size)) {
            memset(r,0,sizeof(*r)); --s->live; released=1;
        }
        break;
    }
    leave(s); return released;
}
int tsfp_host_block_find(tsfp_host_blocks *s,const void *address,tsfp_host_block *out) {
    uintptr_t pointer=(uintptr_t)address;
    int found=0;
    if (!s || !address || !out) return 0;
    enter(s);
    for (size_t i=0;i<s->capacity;++i) {
        const tsfp_host_block *r=s->records+i;
        uintptr_t start=(uintptr_t)r->address;
        if (!start || pointer<start) continue;
        if (pointer-start<r->bytes || (!r->bytes && pointer==start)) {
            *out=*r; found=1; break;
        }
    }
    leave(s); return found;
}
size_t tsfp_host_blocks_stop(tsfp_host_blocks *s) {
    size_t remaining;
    if (!s) return 0;
    enter(s); s->active=0;
    for (size_t i=0;i<s->capacity;++i) {
        tsfp_host_block *r=s->records+i;
        if (r->mapping && s->backend.unmap(s->backend.context,r->mapping,r->mapping_size)) {
            memset(r,0,sizeof(*r)); --s->live;
        }
    }
    remaining=s->live; leave(s); return remaining;
}
