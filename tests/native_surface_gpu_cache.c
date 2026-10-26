/* Cache reuse and retained source identity, independent of actual GPU work. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "surface_gpu_fake_rhi.h"
#include "graphics/surface_gpu_cache.h"
int main(void) {
    PcSurfaceGpuCache cache={0};char errors[256];
    PcSurfaceGpuConfig config={128,26./255,165./255,249./255,43./255,128./255};
    RhiImageDesc desc={0};desc.width=desc.height=128;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
    RhiImage *images[4];for(unsigned i=0;i<4;i++)images[i]=rhi_image_create(&desc,NULL);
    PcSurfaceGpu *first=pc_surface_gpu_cache_acquire(&cache,100,1,&config,images,errors,sizeof errors);assert(first);
    first->ready=1;unsigned allocations=attempt;
    for(unsigned i=0;i<50;i++) {
        PcSurfaceGpu *same=pc_surface_gpu_cache_acquire(&cache,100,1,&config,images,errors,sizeof errors);
        assert(same==first && same->ready && attempt==allocations);
    }
    pc_surface_gpu_cache_suspend(&cache);
    assert(!first->ready && !pc_surface_gpu_output(first) && attempt==allocations);
    assert(pc_surface_gpu_cache_acquire(&cache,100,1,&config,images,errors,sizeof errors)==first);
    assert(!first->ready && attempt==allocations); /* A new begin is required. */
    first->ready=1;
    for(unsigned i=0;i<4;i++)assert(lookup(images[i],IMAGE)->refs==2);
    /* Failed replacement retains old identity, but must not return its owner. */
    unsigned original_live=live,creation_allocations=attempt-4;
    for(unsigned stage=1;stage<=creation_allocations;stage++) {
        fail_at=attempt+stage;
        assert(!pc_surface_gpu_cache_acquire(&cache,100,2,&config,images,errors,sizeof errors));
        assert(cache.entries[0].gpu==first && cache.entries[0].generation==1 && first->ready);
        assert(live==original_live);
        for(unsigned i=0;i<4;i++)assert(lookup(images[i],IMAGE)->refs==2);
    }
    fail_at=0;
    PcSurfaceGpu *second=pc_surface_gpu_cache_acquire(&cache,100,2,&config,images,errors,sizeof errors);
    assert(second && second!=first && !second->ready);
    first=second;config.a*=.9;
    second=pc_surface_gpu_cache_acquire(&cache,100,2,&config,images,errors,sizeof errors);
    assert(second && second!=first && !second->ready);first=second;
    RhiImage *replacement=rhi_image_create(&desc,NULL),*original=images[0];images[0]=replacement;
    second=pc_surface_gpu_cache_acquire(&cache,100,2,&config,images,errors,sizeof errors);
    assert(second && second!=first && lookup(original,IMAGE)->refs==1);
    for(unsigned key=101;key<105;key++)assert(pc_surface_gpu_cache_acquire(&cache,key,1,&config,images,errors,sizeof errors));
    allocations=attempt;unsigned held=live;
    assert(!pc_surface_gpu_cache_acquire(&cache,105,1,&config,images,errors,sizeof errors));
    assert(attempt==allocations && live==held);
    RhiImage *saved=images[1];images[1]=images[0];
    assert(!pc_surface_gpu_cache_acquire(&cache,100,2,&config,images,errors,sizeof errors));images[1]=saved;
    lookup(images[0],IMAGE)->image.format=RHI_FORMAT_R32G32B32A32_FLOAT;
    assert(!pc_surface_gpu_cache_acquire(&cache,100,2,&config,images,errors,sizeof errors));
    lookup(images[0],IMAGE)->image.format=RHI_FORMAT_R8G8B8A8_UNORM;
    assert(!pc_surface_gpu_cache_acquire(&cache,0,2,&config,images,errors,sizeof errors));
    assert(attempt==allocations && live==held);
    /* Release the caller's references first: the cache still owns the images. */
    for(unsigned i=0;i<4;i++) {assert(lookup(images[i],IMAGE)->refs==6);rhi_image_destroy(images[i]);}
    for(unsigned i=0;i<4;i++)assert(lookup(images[i],IMAGE)->refs==5);
    pc_surface_gpu_cache_clear(&cache);assert(live==1); /* unused original source */
    pc_surface_gpu_cache_clear(&cache);rhi_image_destroy(original);assert(!live);
    for(unsigned i=0;i<4;i++)images[i]=rhi_image_create(&desc,NULL);
    assert(pc_surface_gpu_cache_acquire(&cache,100,3,&config,images,errors,sizeof errors));
    pc_surface_gpu_cache_clear(&cache);for(unsigned i=0;i<4;i++)rhi_image_destroy(images[i]);assert(!live);
    puts("PASS: five-resource GPU cache reuse, replacement, rollback, image retention and scene reset");
    return 0;
}
