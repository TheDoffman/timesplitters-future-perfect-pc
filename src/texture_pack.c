/* Optional host-only replacements. Guest dimensions, texels, and game archives
 * remain unchanged. Verified compressed and swizzled 2D formats participate;
 * render targets and screen copies are excluded. Overrides use BC1/2/3 or
 * lossless BGRA8 for small gradients and masks.
 * Disk reads run in the shared bounded queue; original views stay available
 * while replacements load. F10 switches cached views live. Restart after
 * changing pack files on disk.
 * No file I/O when both pack and dump paths are off. */
#include "d3d8_internal.h"
#include "texture_pack.h"
#include "texture_pack_io.h"
#include "texture_pack_stream.h"
#include "texture_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static char pack_root[2048], dump_root[2048];
static int initialized,pack_trace;
static uint64_t resident_bytes;
static unsigned binding_revision;
#define PACK_BUDGET TSFP_PACK_RESIDENT_BUDGET
static void request_replacement(D3D8Texture *tex);

void tsfp_texture_frame_complete(void) {tsfp_stream_pump();}
unsigned tsfp_texture_binding_revision(void) {return binding_revision;}

RhiView *tsfp_texture_view(D3D8Texture *tex) {
    if(tex->pack_checked==3 && !tex->usage && !tex->screen_copy && tex->sys_mem)
        request_replacement(tex);
    /* A formerly static texture can become a screen copy. Never override it. */
    return tsfp_texture_is_4k() && !tex->usage && !tex->screen_copy && tex->pack_srv
        ? tex->pack_srv : tex->srv;
}

void tsfp_texture_release(D3D8Texture *tex) {
    if(tex->pack_checked>=2 || tex->pack_srv)binding_revision++;
    tsfp_stream_cancel(tex);tex->pack_checked=0;
    if(tex->pack_srv) rhi_view_destroy(tex->pack_srv);
    if(tex->pack_image) rhi_image_destroy(tex->pack_image);
    resident_bytes-=tex->pack_bytes;
    tex->pack_image=NULL;tex->pack_srv=NULL;tex->pack_bytes=0;
}

static void dump_original(const D3D8Texture *tex, uint64_t key, uint32_t bytes) {
    FILE *f;
    char path[2304];uint32_t header[32]={0};
    if(!dump_root[0]) return;
    if(tex->d3d8_format!=12 && tex->d3d8_format!=14 && tex->d3d8_format!=15) {
        /* TSFT: magic + little-endian format,width,height + raw swizzled
         * level zero. This avoids mislabelling uncompressed bytes as DXT. */
        uint32_t raw_header[4]={0x54465354,tex->d3d8_format,tex->width,tex->height};
        snprintf(path,sizeof(path),"%s/%016" PRIx64 ".tsft",dump_root,key);
        f=fopen(path,"rb");if(f) {fclose(f);return;}
        f=fopen(path,"wb");if(!f) return;
        fwrite(raw_header,1,sizeof(raw_header),f);fwrite(tex->sys_mem,1,bytes,f);fclose(f);
        return;
    }
    snprintf(path,sizeof(path),"%s/%016" PRIx64 ".dds",dump_root,key);
    f=fopen(path,"rb");if(f) {fclose(f);return;}
    f=fopen(path,"wb");if(!f) return;
    header[0]=0x20534444;header[1]=124;header[2]=0x81007;
    header[3]=tex->height;header[4]=tex->width;header[5]=bytes;
    header[19]=32;header[20]=4;header[21]=tex->d3d8_format==12?0x31545844:
                                            tex->d3d8_format==14?0x33545844:0x35545844;
    header[27]=0x1000;
    fwrite(header,1,sizeof(header),f);fwrite(tex->sys_mem,1,bytes,f);fclose(f);
}

static void apply_replacement(void *owner,uint64_t key,const unsigned char *data,const TsfpDDS *dds) {
    D3D8Texture *tex=owner;
    uint32_t i;
    RhiImageDesc desc={0};RhiViewDesc view={0};RhiSubresourceData initial[13];
    if(tex->pack_key!=key || tex->pack_checked<2)return;
    tex->pack_checked=1;
    /* Refresh persistent stage bindings, including queue-full retries. */
    binding_revision++;
    if(!data || tex->usage || tex->screen_copy || !tex->sys_mem ||
       resident_bytes>PACK_BUDGET || dds->bytes>PACK_BUDGET-resident_bytes)return;
    if(!tsfp_texture_alpha_compatible(tex->d3d8_format,dds->format)) {
        if(pack_trace)fprintf(stderr,"[TEXTURE-PACK] rejected incompatible alpha: %016" PRIx64 "\n",key);
        return;
    }
    desc.type=RHI_IMAGE_2D;desc.width=dds->width;desc.height=dds->height;desc.depth=1;
    desc.mip_levels=dds->levels;desc.format=d3d8_to_dxgi_format((D3DFORMAT)dds->format);
    desc.samples=1;desc.usage=RHI_USAGE_DEFAULT;desc.bind=RHI_BIND_SAMPLED;
    for(i=0;i<dds->levels;i++) {initial[i].data=data+dds->offset[i];initial[i].row_pitch=dds->pitch[i];initial[i].slice_pitch=dds->size[i];}
    tex->pack_image=rhi_image_create(&desc,initial);
    if(!tex->pack_image)return;
    view.format=desc.format;view.dim=RHI_VIEW_DIM_2D;view.mip_count=dds->levels;
    tex->pack_srv=rhi_view_create(tex->pack_image,RHI_VIEW_SAMPLED,&view);
    if(!tex->pack_srv) {rhi_image_destroy(tex->pack_image);tex->pack_image=NULL;return;}
    tex->pack_bytes=dds->bytes;resident_bytes+=dds->bytes;
    if(pack_trace)fprintf(stderr,"[TEXTURE-PACK] loaded %016" PRIx64 " %ux%u -> %ux%u mips=%u resident=%" PRIu64 "\n",
        key,tex->width,tex->height,dds->width,dds->height,dds->levels,resident_bytes);
}

static void request_replacement(D3D8Texture *tex) {
    char path[2304];
    snprintf(path,sizeof(path),"%s/%016" PRIx64 ".dds",pack_root,tex->pack_key);
    int queued=tsfp_stream_submit(tex,tex->pack_key,path,tex->width,tex->height,apply_replacement);
    tex->pack_checked=queued==0?3:2;
    if(queued<0) {
        TsfpDDS dds;
        unsigned char *data=tsfp_pack_read(path,tex->width,tex->height,PACK_BUDGET-resident_bytes,&dds);
        apply_replacement(tex,tex->pack_key,data,&dds);free(data);
    }
}

void tsfp_texture_upload(D3D8Texture *tex) {
    uint32_t bytes;uint64_t key;
    if(!initialized) {
        const char *v=getenv("TSFP_TEXTURE_PACK_DIR");
        if(v && strlen(v)<sizeof(pack_root)) strcpy(pack_root,v);
        v=getenv("TSFP_TEXTURE_DUMP_DIR");
        if(v && strlen(v)<sizeof(dump_root)) strcpy(dump_root,v);
        v=getenv("TSFP_TEXTURE_PACK_TRACE");pack_trace=v && *v && strcmp(v,"0");
        initialized=1;
        if(pack_root[0]) fprintf(stderr,"[TEXTURE-PACK] directory=%s budget=512MiB\n",pack_root);
    }
    if(!pack_root[0] && !dump_root[0]) return;
    if(tex->usage || tex->screen_copy || !tex->sys_mem) return;
    bytes=tsfp_texture_base_bytes(tex->d3d8_format,tex->width,tex->height);
    if(!bytes) return;
    key=tsfp_texture_key(tex->d3d8_format,tex->width,tex->height,tex->sys_mem,bytes);
    if(tex->pack_checked && tex->pack_key==key) return;
    tsfp_texture_release(tex);tex->pack_key=key;tex->pack_checked=1;
    dump_original(tex,key,bytes);
    if(!pack_root[0]) return;
    request_replacement(tex);
}
