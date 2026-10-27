/* Deferred render-thread integration. Inputs keep their original byte storage;
 * only the final ripple shader temporarily borrows the continuously evolved
 * field. Never read back, flush, or keep guest-memory pointers in this path. */
#include "pc_surface_native.h"
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
#include "graphics/surface_gpu_cache.h"
static PcSurfaceGpuCache pc_surface_native_cache;
static unsigned pc_surface_native_failed;
static PcSurfaceNativeClock pc_surface_native_clocks[PC_SURFACE_GPU_CACHE_SIZE];
static void pc_surface_native_clear(void) {
    d3d8_surface_field(NULL,NULL);
    /* Original60, before any native use, must not alter renderer bindings. */
    unsigned owned=0;
    for(unsigned i=0;i<PC_SURFACE_GPU_CACHE_SIZE;i++)owned|=pc_surface_native_cache.entries[i].gpu!=NULL;
    if(owned){RhiView *none[4]={0};rhi_set_textures(0,4,none);}
    pc_surface_native_failed=0;
    pc_surface_gpu_cache_clear(&pc_surface_native_cache);
    memset(pc_surface_native_clocks,0,sizeof pc_surface_native_clocks);
}
static D3D8Texture *pc_surface_native_texture(const uint32_t identity[4]) {
    for(int i=0;i<g_texture_count;i++) {
        texture_entry *e=&g_textures[i];D3D8TextureInfo info;
        if(e->host && e->rendered && e->va==identity[0] && e->data==identity[1] &&
           e->format==identity[2] && e->size==identity[3] &&
           d3d8_texture_info((IDirect3DBaseTexture8*)e->host,&info))return (D3D8Texture*)e->host;
    }
    return NULL;
}
#endif
static void pc_surface_native_reset_execute(const void *unused) {
    (void)unused;
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    pc_surface_native_clear();
#endif
}
static void pc_surface_native_finish_execute(const void *unused) {
    (void)unused;
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    d3d8_surface_field(NULL,NULL);
    RhiView *none[4]={0};rhi_set_textures(0,4,none);
#endif
}
static void pc_surface_native_suspend_execute(const void *unused) {
    pc_surface_native_finish_execute(unused);
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    pc_surface_gpu_cache_suspend(&pc_surface_native_cache);
    memset(pc_surface_native_clocks,0,sizeof pc_surface_native_clocks);
#endif
}
static void pc_surface_native_prepare_execute(const void *argument) {
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    if(pc_surface_native_failed)return;
    const PcSurfaceNativeSample *sample=argument;
    D3D8Texture *textures[4];RhiImage *images[4];
    d3d8_surface_field(NULL,NULL);
    for(unsigned i=0;i<4;i++) {
        textures[i]=pc_surface_native_texture(i<3?sample->feedback.textures[i]:sample->final_texture);
        if(!textures[i] || !textures[i]->image || !textures[i]->srv)return;
        images[i]=textures[i]->image;
    }
    float authored[4];memcpy(authored,sample->feedback.coefficient_bits,sizeof authored);
    double c[4];for(unsigned i=0;i<4;i++) {
        if(!isfinite(authored[i]))return;
        c[i]=(int32_t)(fminf(1.f,fmaxf(0.f,authored[i]))*255.f)/255.;
    }
    PcSurfaceGpuConfig config={128,c[1],c[2],c[3],c[0],128./255.};
    PcSurfaceGpuEntry *entry=NULL;
    for(unsigned i=0;i<PC_SURFACE_GPU_CACHE_SIZE;i++)
        if(pc_surface_native_cache.entries[i].resource==sample->feedback.resource)entry=&pc_surface_native_cache.entries[i];
    if(!sample->feedback.whole && !entry)return; /* Wait for an authored boundary. */
    PcSurfaceGpu *previous=entry?entry->gpu:NULL;
    char errors[1024]={0};
    d3d8_surface_cleanup(pc_surface_native_clear);
    PcSurfaceGpu *gpu=pc_surface_gpu_cache_acquire(&pc_surface_native_cache,sample->feedback.resource,
        ((uint64_t)sample->feedback.scene<<32)|sample->feedback.level,&config,images,errors,sizeof errors);
    if(!gpu) {
        pc_surface_native_failed=1;
        fprintf(stderr,"[SURFACE-NATIVE] Original-path fallback until scene/device reset: %s\n",errors);
        return;
    }
    if(previous!=gpu)fprintf(stderr,"[SURFACE-NATIVE] Created owner resource=%08X frame=%u\n",sample->feedback.resource,sample->feedback.frame);
    for(unsigned i=0;i<PC_SURFACE_GPU_CACHE_SIZE;i++)if(pc_surface_native_cache.entries[i].gpu==gpu) {
        PcSurfaceNativeClock *clock=&pc_surface_native_clocks[i],next;
        if(previous!=gpu)memset(clock,0,sizeof *clock);
        double delta;unsigned action=pc_surface_native_plan(clock,sample,&next,&delta);
        if(!action){clock->ready=0;return;}
        if(action==1 && !pc_surface_gpu_begin_bytes(gpu,textures[0]->srv,textures[1]->srv)){clock->ready=0;return;}
        if(!pc_surface_gpu_advance(gpu,delta)){clock->ready=0;return;}
        RhiView *packed=pc_surface_gpu_pack(gpu);
        if(!packed){clock->ready=0;return;}
        *clock=next;
        d3d8_surface_field((IDirect3DBaseTexture8*)textures[2],packed);
        static unsigned notices;
        if(notices<12) {
            fprintf(stderr,"[SURFACE-NATIVE] frame=%u resource=%08X action=%u whole=%u delta=%.9f\n",
                sample->feedback.frame,sample->feedback.resource,action,sample->feedback.whole,delta);notices++;
        }
        return;
    }
#else
    (void)argument;
#endif
}
void hle_d3d8_surface_native_prepare(const PcSurfaceNativeSample *sample) {
    if(!sample)return;
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(pc_surface_native_prepare_execute,sample,sizeof *sample);
    else pc_surface_native_prepare_execute(sample);
}
void hle_d3d8_surface_native_finish(void) {
    unsigned unused=0;
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(pc_surface_native_finish_execute,&unused,sizeof unused);
    else pc_surface_native_finish_execute(NULL);
}
void hle_d3d8_surface_native_reset(void) {
    unsigned unused=0;
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(pc_surface_native_reset_execute,&unused,sizeof unused);
    else pc_surface_native_reset_execute(NULL);
}
void hle_d3d8_surface_native_suspend(void) {
    unsigned unused=0;
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(pc_surface_native_suspend_execute,&unused,sizeof unused);
    else pc_surface_native_suspend_execute(NULL);
}
