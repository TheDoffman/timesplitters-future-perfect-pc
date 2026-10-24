/* Included after the HLE texture cache definitions. This diagnostic runs after
 * the original feedback draws in the SAME deferred command stream. Readback
 * synchronizes the GPU; never use captured runs as frame-time measurements. */
#include "pc_surface_capture.h"
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
#include "graphics/surface_gpu.h"
/* A bounded integration probe, not the final persistent runtime owner. Create
 * and release within the callback so no diagnostic resources outlive a device
 * or scene. The eventual normal path must use the retained resource registry. */
static int pc_surface_capture_gpu(const PcSurfaceCapture *sample,RhiView **views,
    const char *path,unsigned record) {
    const char *enabled=getenv("TSFP_SURFACE_GPU_CAPTURE");
    if(!enabled || enabled[0]!='1')return 1;
    float authored[4];memcpy(authored,sample->coefficient_bits,sizeof authored);
    double coefficients[4];
    for(unsigned i=0;i<4;i++) {
        if(!isfinite(authored[i]))return 0;
        coefficients[i]=(int32_t)(fminf(1.f,fmaxf(0.f,authored[i]))*255.f)/255.;
    }
    PcSurfaceGpuConfig config={128,coefficients[1],coefficients[2],coefficients[3],coefficients[0],128./255.};
    char errors[1024]={0};PcSurfaceGpu *owner=pc_surface_gpu_create(&config,errors,sizeof errors);
    float *pixels=malloc(2u*128u*128u*16u);
    int valid=owner && pixels && pc_surface_gpu_begin_bytes(owner,views[0],views[1]);
    if(valid)valid=pc_surface_gpu_advance(owner,5./12.) &&
        !rhi_image_readback(owner->world.image,0,pixels,128*16);
    if(valid)valid=pc_surface_gpu_advance(owner,5./12.) && pc_surface_gpu_advance(owner,1./6.) &&
        !rhi_image_readback(owner->world.image,0,pixels+128*128*4,128*16);
    /* Destruction also happens on failure, while the renderer is still alive. */
    pc_surface_gpu_destroy(owner);
    if(valid && path && *path) {
        size_t length=strlen(path);char *destination=malloc(length+5);
        if(!destination)valid=0;
        else {
            memcpy(destination,path,length);memcpy(destination+length,".gpu",5);
            FILE *file=fopen(destination,record?"ab":"wb");free(destination);
            valid=file!=NULL;
            if(file) {
                if(!record)valid=fwrite("TSWGPU1\0",1,8,file)==8;
                valid=valid && fwrite(sample,1,sizeof *sample,file)==sizeof *sample;
                valid=valid && fwrite(pixels,1,2u*128u*128u*16u,file)==2u*128u*128u*16u;
                if(fclose(file))valid=0;
            }
        }
    } else valid=0;
    free(pixels);
    if(!valid)fprintf(stderr,"[SURFACE-CAPTURE] GPU solver failed: %s\n",errors);
    return valid;
}
#endif
static void pc_surface_capture_execute(const void *argument) {
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    static unsigned samples,stopped;
    if(stopped || samples>=16)return;
    const PcSurfaceCapture *sample=argument;
    RhiView *views[3]={0};
    unsigned char *pixels=malloc(3u*128u*128u*4u);
    if(!pixels){stopped=1;return;}
    int valid=sizeof *sample==92;
    for(unsigned i=0;valid && i<3;i++) {
        IDirect3DTexture8 *host=NULL;
        for(int j=0;j<g_texture_count;j++) {
            texture_entry *e=&g_textures[j];
            if(e->host && e->rendered && e->va==sample->textures[i][0] &&
               e->data==sample->textures[i][1] && e->format==sample->textures[i][2] &&
               e->size==sample->textures[i][3]) {host=e->host;break;}
        }
        D3D8TextureInfo info;
        if(!host || !d3d8_texture_info((IDirect3DBaseTexture8*)host,&info)){valid=0;break;}
        D3D8Texture *texture=(D3D8Texture*)host;RhiImageDesc desc;
        views[i]=texture->srv;
        if(!texture->image){valid=0;break;}
        rhi_image_get_desc(texture->image,&desc);
        if(desc.width!=128 || desc.height!=128 || desc.mip_levels!=1 || desc.samples!=1 ||
           (desc.format!=RHI_FORMAT_R8G8B8A8_UNORM && desc.format!=RHI_FORMAT_B8G8R8A8_UNORM)) {valid=0;break;}
        unsigned char *dst=pixels+i*128u*128u*4u;
        if(rhi_image_readback(texture->image,0,dst,128*4)){valid=0;break;}
        if(desc.format==RHI_FORMAT_B8G8R8A8_UNORM)for(unsigned p=0;p<128*128;p++) {
            unsigned char r=dst[p*4];dst[p*4]=dst[p*4+2];dst[p*4+2]=r;
        }
    }
    if(valid) {
        const char *path=getenv("TSFP_SURFACE_CAPTURE");
        valid=pc_surface_capture_gpu(sample,views,path,samples);
        if(!valid){free(pixels);stopped=1;return;}
        FILE *file=path && *path?fopen(path,samples?"ab":"wb"):NULL;
        valid=file!=NULL;
        if(file) {
            if(!samples)valid=fwrite("TSWATER1",1,8,file)==8;
            valid=valid && fwrite(sample,1,sizeof *sample,file)==sizeof *sample;
            valid=valid && fwrite(pixels,1,3u*128u*128u*4u,file)==3u*128u*128u*4u;
            if(fclose(file))valid=0;
        }
    }
    free(pixels);
    if(valid) {
        samples++;
        fprintf(stderr,"[SURFACE-CAPTURE] %u/16 frame=%u resource=%08X completed GPU feedback\n",samples,sample->frame,sample->resource);
    } else {stopped=1;fprintf(stderr,"[SURFACE-CAPTURE] Stopped: unsupported identity/storage or read/write failure\n");}
#else
    (void)argument; /* Legacy GL has no common RHI image access. */
#endif
}
void hle_d3d8_surface_capture(const PcSurfaceCapture *sample) {
    if(!sample)return;
    if(hle_d3d8_defer_recording())hle_d3d8_defer_op(pc_surface_capture_execute,sample,sizeof *sample);
    else pc_surface_capture_execute(sample);
}
