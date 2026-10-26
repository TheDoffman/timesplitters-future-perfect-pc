#ifndef TSFP_SURFACE_GPU_CACHE_H
#define TSFP_SURFACE_GPU_CACHE_H
#include "graphics/surface_gpu.h"

/* The supported guest constructor creates five resources, each with four
 * images (three feedback intermediates and a final shading target). Retain the
 * actual images so a freed allocation's address cannot impersonate an old key.
 * Start this render-thread registry zeroed. The caller must clear it on scene/device reset and
 * advance generation when a resource is reinitialized in place. No eviction of
 * potentially live entries: a full cache returns NULL for original-path fallback.
 * This helper does not install the guest/HLE hooks or execute event passes. */
#define PC_SURFACE_GPU_CACHE_SIZE 5u
typedef struct PcSurfaceGpuEntry {
    uint32_t resource;
    uint64_t generation;
    RhiImage *images[4];
    PcSurfaceGpu *gpu;
} PcSurfaceGpuEntry;
typedef struct PcSurfaceGpuCache { PcSurfaceGpuEntry entries[PC_SURFACE_GPU_CACHE_SIZE]; } PcSurfaceGpuCache;

static inline void pc_surface_gpu_entry_clear(PcSurfaceGpuEntry *entry) {
    pc_surface_gpu_destroy(entry->gpu);
    for(unsigned i=0;i<4;i++)if(entry->images[i])rhi_image_destroy(entry->images[i]);
    memset(entry,0,sizeof *entry);
}
/* Suspend timing without recompiling/reallocating on an FPS round trip.
 * Borrowed output becomes invalid. Scene/device destruction still uses clear. */
static inline void pc_surface_gpu_cache_suspend(PcSurfaceGpuCache *cache) {
    if(cache)for(unsigned i=0;i<PC_SURFACE_GPU_CACHE_SIZE;i++)pc_surface_gpu_reset(cache->entries[i].gpu);
}
static inline void pc_surface_gpu_cache_clear(PcSurfaceGpuCache *cache) {
    if(cache)for(unsigned i=0;i<PC_SURFACE_GPU_CACHE_SIZE;i++)pc_surface_gpu_entry_clear(&cache->entries[i]);
}
static inline int pc_surface_gpu_config_equal(const PcSurfaceGpuConfig *a,const PcSurfaceGpuConfig *b) {
    return a->side==b->side && a->a==b->a && a->b==b->b && a->d==b->d && a->k==b->k && a->center==b->center;
}
static inline int pc_surface_gpu_cache_images(const PcSurfaceGpuConfig *config,RhiImage *const images[4]) {
    if(!config || !images)return 0;
    for(unsigned i=0;i<4;i++) {
        if(!images[i])return 0;
        for(unsigned j=0;j<i;j++)if(images[i]==images[j])return 0;
        RhiImageDesc desc;rhi_image_get_desc(images[i],&desc);
        if(desc.type!=RHI_IMAGE_2D || desc.width!=config->side || desc.height!=config->side ||
            desc.depth!=1 || desc.mip_levels!=1 || desc.samples!=1 ||
            !(desc.bind&RHI_BIND_RENDER_TARGET) || !(desc.bind&RHI_BIND_SAMPLED) ||
            (desc.format!=RHI_FORMAT_R8G8B8A8_UNORM && desc.format!=RHI_FORMAT_B8G8R8A8_UNORM))return 0;
    }
    return 1;
}
static inline PcSurfaceGpu *pc_surface_gpu_cache_acquire(PcSurfaceGpuCache *cache,uint32_t resource,
    uint64_t generation,const PcSurfaceGpuConfig *config,RhiImage *const images[4],char *errors,size_t capacity)
{
    if(!cache || !resource || !pc_surface_gpu_cache_images(config,images))return NULL;
    PcSurfaceGpuEntry *entry=NULL,*empty=NULL;
    for(unsigned i=0;i<PC_SURFACE_GPU_CACHE_SIZE;i++) {
        if(cache->entries[i].gpu && cache->entries[i].resource==resource)entry=&cache->entries[i];
        if(!cache->entries[i].gpu && !empty)empty=&cache->entries[i];
    }
    if(entry && entry->generation==generation && pc_surface_gpu_config_equal(&entry->gpu->config,config)) {
        unsigned same=1;for(unsigned i=0;i<4;i++)same&=entry->images[i]==images[i];
        if(same)return entry->gpu;
    }
    if(!entry)entry=empty;
    if(!entry)return NULL;
    /* Replacement is transactional: keep the old identity/owner on failure,
     * but return NULL so the caller cannot use it for the mismatched request. */
    PcSurfaceGpu *next=pc_surface_gpu_create(config,errors,capacity);if(!next)return NULL;
    PcSurfaceGpuEntry replacement={0};replacement.resource=resource;replacement.generation=generation;replacement.gpu=next;
    for(unsigned i=0;i<4;i++) {replacement.images[i]=images[i];rhi_image_retain(images[i]);}
    pc_surface_gpu_entry_clear(entry);*entry=replacement;return next;
}
#endif
