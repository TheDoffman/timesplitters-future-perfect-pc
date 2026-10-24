/* Included after the HLE texture cache definitions. This diagnostic runs after
 * the original feedback draws in the SAME deferred command stream. Readback
 * synchronizes the GPU; never use captured runs as frame-time measurements. */
#include "pc_surface_capture.h"
static void pc_surface_capture_execute(const void *argument) {
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    static unsigned samples,stopped;
    if(stopped || samples>=16)return;
    const PcSurfaceCapture *sample=argument;
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
