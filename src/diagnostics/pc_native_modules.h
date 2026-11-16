#ifndef TSFP_PC_NATIVE_MODULES_H
#define TSFP_PC_NATIVE_MODULES_H
/* Bounded image metadata, published outside the fault handler. Payloads are
 * immutable after publication; unload only changes an atomic live bit. */
#include "pc_breadcrumbs.h"
#include <string.h>
#define PC_NATIVE_MODULE_COUNT 1024
#define PC_NATIVE_MODULE_PATH 512
typedef struct PcNativeImage {
    uint64_t base,begin,end,bias;
    int path_complete;
    char path[PC_NATIVE_MODULE_PATH];
} PcNativeImage;
typedef struct PcNativeImageSlot {
    PcCrumbAtomic published,live;
    PcNativeImage image;
} PcNativeImageSlot;
typedef struct PcNativeModules {
    PcCrumbAtomic writer,reserved,truncated;
    PcNativeImageSlot slots[PC_NATIVE_MODULE_COUNT];
} PcNativeModules;
static inline unsigned pc_native_module_count(PcNativeModules *m) {
    int64_t n=pc_crumb_load(&m->reserved);
    return n<0?0:n>PC_NATIVE_MODULE_COUNT?PC_NATIVE_MODULE_COUNT:(unsigned)n;
}
static inline int pc_native_module_add(PcNativeModules *m,uint64_t base,
        uint64_t begin,uint64_t end,uint64_t bias,const char *path) {
    if(!path || !*path || begin>=end)return 0;
    if(!pc_crumb_lock(&m->writer)){pc_crumb_store(&m->truncated,1);return 0;}
    unsigned n=pc_native_module_count(m);
    if(n==PC_NATIVE_MODULE_COUNT){pc_crumb_store(&m->truncated,1);pc_crumb_store(&m->writer,0);return 0;}
    pc_crumb_store(&m->reserved,n+1);
    PcNativeImageSlot *s=&m->slots[n];
    s->image.base=base;s->image.begin=begin;s->image.end=end;s->image.bias=bias;
    unsigned i=0;for(;i+1<PC_NATIVE_MODULE_PATH && path[i];i++)s->image.path[i]=path[i];
    s->image.path[i]=0;s->image.path_complete=path[i]==0;
    pc_crumb_store(&s->live,1);pc_crumb_store(&s->published,1);
    pc_crumb_store(&m->writer,0);return 1;
}
static inline void pc_native_module_remove(PcNativeModules *m,uint64_t base) {
    unsigned count=pc_native_module_count(m);
    for(unsigned i=0;i<count;i++){
        PcNativeImageSlot *s=&m->slots[i];
        if(pc_crumb_load(&s->published) && s->image.base==base)pc_crumb_store(&s->live,0);
    }
}
static inline int pc_native_module_view(PcNativeModules *m,unsigned i,PcNativeImage *out) {
    if(i>=pc_native_module_count(m))return 0;
    PcNativeImageSlot *s=&m->slots[i];
    if(!pc_crumb_load(&s->published) || !pc_crumb_load(&s->live))return 0;
    *out=s->image;
    return pc_crumb_load(&s->live)!=0;
}
static inline int pc_native_module_find(PcNativeModules *m,uint64_t pc,PcNativeImage *out) {
    for(unsigned i=pc_native_module_count(m);i;i--)
        if(pc_native_module_view(m,i-1,out) && pc>=out->begin && pc<out->end)return 1;
    return 0;
}
static inline int pc_native_module_address(const PcNativeImage *image,uint64_t pc,uint64_t *address) {
    if(pc<image->begin || pc>=image->end || pc<image->bias)return 0;
    *address=pc-image->bias;return 1;
}
#endif
