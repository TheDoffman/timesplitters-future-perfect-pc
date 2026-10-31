#include "guest_memory_services.h"

static void word(void *output,uint32_t value) {
    unsigned char *p=(unsigned char *)output;
    for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(value>>(i*8));
}
static int window(const tsfp_guest_mm *m) {
    return m && m->physical_size &&
        (uint64_t)m->physical_base+m->physical_size<=UINT64_C(0x100000000);
}
uint32_t tsfp_guest_mm_allocate(const tsfp_guest_mm *m,enum tsfp_guest_mm_kind kind,uint32_t bytes) {
    uint32_t base,alignment;
    if(!m)return 0;
    if(kind==TSFP_MM_CONTIGUOUS)return tsfp_physical_allocate(m->vm.allocations,bytes,4096);
    if(kind!=TSFP_MM_SYSTEM && kind!=TSFP_MM_DEBUG && kind!=TSFP_MM_IO && kind!=TSFP_MM_STACK)return 0;
    if(!bytes && (kind==TSFP_MM_SYSTEM || kind==TSFP_MM_DEBUG))return 0;
    alignment=(kind==TSFP_MM_SYSTEM || kind==TSFP_MM_IO)?4096:16;
    base=tsfp_heap_allocate(m->vm.allocations,bytes,alignment);
    /* The allocator validates the complete extent before handing out a base. */
    return base && kind==TSFP_MM_STACK?base+bytes:base;
}
void tsfp_guest_mm_release(const tsfp_guest_mm *m,enum tsfp_guest_mm_kind kind,uint32_t address) {
    if(!m)return;
    if(kind==TSFP_MM_CONTIGUOUS) {
        if(tsfp_physical_release(m->vm.allocations,address))return;
    } else if(kind!=TSFP_MM_SYSTEM && kind!=TSFP_MM_DEBUG && kind!=TSFP_MM_IO && kind!=TSFP_MM_STACK)return;
    (void)tsfp_heap_release(m->vm.allocations,address);
}
uint32_t tsfp_guest_mm_contiguous_ex(const tsfp_guest_mm *m,uint32_t bytes,
    uint32_t low,uint32_t high,uint32_t alignment) {
    uint64_t width;
    uint32_t ceiling;
    if(!window(m) || high<low || low>=m->physical_size)return 0;
    width=(uint64_t)high-low+1u;
    if(low && width<=(uint64_t)bytes+4096u) {
        if(bytes>width)return 0;
        return tsfp_physical_allocate_at(m->vm.allocations,m->physical_base+low,bytes);
    }
    ceiling=high<m->physical_size?high:m->physical_size-1;
    return tsfp_physical_allocate_range(m->vm.allocations,bytes,alignment,
        m->physical_base+low,m->physical_base+ceiling);
}
uint32_t tsfp_guest_mm_size(const tsfp_guest_mm *m,uint32_t address) {
    return m?tsfp_allocation_remaining(m->vm.allocations,address):0;
}
uint32_t tsfp_guest_mm_physical(const tsfp_guest_mm *m,uint32_t address) {
    if(window(m) && address>=m->physical_base && address-m->physical_base<m->physical_size)
        return address-m->physical_base;
    return address;
}
uint32_t tsfp_guest_mm_gpu(const tsfp_guest_mm *m,uint32_t bytes,uint32_t padding_word) {
    uint64_t start;
    void *padding=NULL;
    if(!window(m))return 0;
    if(bytes==UINT32_MAX)bytes=m->gpu_default;
    if(bytes>m->physical_size)return 0;
    start=(uint64_t)m->physical_base+m->physical_size-bytes;
    if(start>UINT32_MAX)return 0;
    if(padding_word) {
        if(!m->vm.resolve)return 0;
        padding=m->vm.resolve(m->vm.host,padding_word,4,1);
        if(!padding)return 0;
    }
    if(padding)word(padding,0);
    return (uint32_t)start;
}
uint32_t tsfp_guest_mm_statistics(const tsfp_guest_mm *m,uint32_t output) {
    uint32_t values[TSFP_MEMORY_STATISTICS_WORDS],status;
    unsigned char *p;
    if(!m || !output || !m->vm.resolve)return UINT32_C(0xc000000d);
    p=(unsigned char *)m->vm.resolve(m->vm.host,output,TSFP_MEMORY_STATISTICS_BYTES,1);
    if(!p)return UINT32_C(0xc000000d);
    status=tsfp_memory_statistics(m->available,m->vm.host,values);
    if(status)return status;
    for(unsigned i=0;i<TSFP_MEMORY_STATISTICS_WORDS;++i)word(p+i*4,values[i]);
    return 0;
}
uint32_t tsfp_guest_mm_query_access(const tsfp_guest_mm *m,uint32_t address) {
    void *p;
    if(!address)return 0;
    if(!m || !m->vm.resolve || !m->query_access)return 1;
    p=m->vm.resolve(m->vm.host,address,1,-1);
    return p?m->query_access(m->vm.host,p):1;
}
uint32_t tsfp_guest_mm_global(const tsfp_guest_mm *m,uint32_t address) {
    void *p;
    if(!m || !address || !m->vm.resolve)return 0;
    p=m->vm.resolve(m->vm.host,address,4,1);
    if(!p)return 0;
    word(p,address);return address;
}
int tsfp_guest_mm_set_access(const tsfp_guest_mm *m,uint32_t address,uint32_t bytes,
    uint32_t protection) {
    void *p;
    if(!m || !address || !bytes || !m->vm.resolve || !m->set_access)return 0;
    p=m->vm.resolve(m->vm.host,address,bytes,-1);
    return p?m->set_access(m->vm.host,p,bytes,protection):0;
}
uint32_t tsfp_guest_mm_advisory(enum tsfp_guest_mm_advisory query,uint32_t address) {
    if(query==TSFP_MM_VALID_ADDRESS)return 1;
    if(query==TSFP_MM_AVAILABLE_PAGES)return 0x10000;
    return query==TSFP_MM_WRITE_CHECK?address:0;
}
