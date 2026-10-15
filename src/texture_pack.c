/* Optional host-only replacements. Guest dimensions, texels, and game archives
 * remain unchanged. Only compressed, non-render-target 2D textures participate.
 * F10 switches cached views live. Restart after changing pack files on disk.
 * No file I/O when both options are off. */
#include "d3d8_internal.h"
#include "texture_pack.h"
#include "texture_mode.h"
#include "texture_pack_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static char pack_root[2048], dump_root[2048];
static int initialized;
static uint64_t resident_bytes;
#define PACK_BUDGET (UINT64_C(512)*1024*1024)

RhiView *tsfp_texture_view(D3D8Texture *tex) {
    /* A formerly static texture can become a screen copy. Never override it. */
    return tsfp_texture_is_4k() && !tex->usage && !tex->screen_copy && tex->pack_srv
        ? tex->pack_srv : tex->srv;
}

void tsfp_texture_release(D3D8Texture *tex) {
    if(tex->pack_srv) rhi_view_destroy(tex->pack_srv);
    if(tex->pack_image) rhi_image_destroy(tex->pack_image);
    resident_bytes-=tex->pack_bytes;
    tex->pack_image=NULL;tex->pack_srv=NULL;tex->pack_bytes=0;
}

static void dump_original(const D3D8Texture *tex, uint64_t key, uint32_t bytes) {
    char path[2304];FILE *f;uint32_t header[32]={0};
    if(!dump_root[0]) return;
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

void tsfp_texture_upload(D3D8Texture *tex) {
    uint32_t bytes,i;uint64_t key;char path[2304];FILE *f;
    unsigned char *data;long length;TsfpDDS dds;
    RhiImageDesc desc={0};RhiViewDesc view={0};RhiSubresourceData initial[13];
    if(!initialized) {
        const char *v=getenv("TSFP_TEXTURE_PACK_DIR");
        if(v && strlen(v)<sizeof(pack_root)) strcpy(pack_root,v);
        v=getenv("TSFP_TEXTURE_DUMP_DIR");
        if(v && strlen(v)<sizeof(dump_root)) strcpy(dump_root,v);
        initialized=1;
        if(pack_root[0]) fprintf(stderr,"[TEXTURE-PACK] directory=%s budget=512MiB\n",pack_root);
    }
    if(!pack_root[0] && !dump_root[0]) return;
    if(tex->usage || tex->screen_copy ||
       (tex->d3d8_format!=12 && tex->d3d8_format!=14 && tex->d3d8_format!=15)) return;
    bytes=((tex->width+3)/4)*((tex->height+3)/4)*(tex->d3d8_format==12?8:16);
    key=tsfp_texture_key(tex->d3d8_format,tex->width,tex->height,tex->sys_mem,bytes);
    if(tex->pack_checked && tex->pack_key==key) return;
    tsfp_texture_release(tex);tex->pack_key=key;tex->pack_checked=1;
    dump_original(tex,key,bytes);
    if(!pack_root[0]) return;
    snprintf(path,sizeof(path),"%s/%016" PRIx64 ".dds",pack_root,key);
    f=fopen(path,"rb");if(!f) return;
    if(fseek(f,0,SEEK_END) || (length=ftell(f))<128 || length>24*1024*1024 || fseek(f,0,SEEK_SET)) {fclose(f);return;}
    data=(unsigned char *)malloc((size_t)length);if(!data) {fclose(f);return;}
    if(fread(data,1,(size_t)length,f)!=(size_t)length) {fclose(f);free(data);return;}
    fclose(f);
    if(!tsfp_dds_parse(data,(size_t)length,&dds) ||
       dds.width*tex->height!=dds.height*tex->width ||
       dds.width<tex->width || dds.height<tex->height) {
        fprintf(stderr,"[TEXTURE-PACK] rejected invalid DDS/aspect: %s\n",path);free(data);return;
    }
    if(resident_bytes+dds.bytes>PACK_BUDGET) {
        fprintf(stderr,"[TEXTURE-PACK] budget reached; original retained: %016" PRIx64 "\n",key);free(data);return;
    }
    desc.type=RHI_IMAGE_2D;desc.width=dds.width;desc.height=dds.height;desc.depth=1;
    desc.mip_levels=dds.levels;desc.format=d3d8_to_dxgi_format((D3DFORMAT)dds.format);
    desc.samples=1;desc.usage=RHI_USAGE_DEFAULT;desc.bind=RHI_BIND_SAMPLED;
    for(i=0;i<dds.levels;i++) {initial[i].data=data+dds.offset[i];initial[i].row_pitch=dds.pitch[i];initial[i].slice_pitch=dds.size[i];}
    tex->pack_image=rhi_image_create(&desc,initial);free(data);
    if(!tex->pack_image) return;
    view.format=desc.format;view.dim=RHI_VIEW_DIM_2D;view.mip_count=dds.levels;
    tex->pack_srv=rhi_view_create(tex->pack_image,RHI_VIEW_SAMPLED,&view);
    if(!tex->pack_srv) {rhi_image_destroy(tex->pack_image);tex->pack_image=NULL;return;}
    tex->pack_bytes=dds.bytes;resident_bytes+=dds.bytes;
    fprintf(stderr,"[TEXTURE-PACK] loaded %016" PRIx64 " %ux%u -> %ux%u mips=%u resident=%" PRIu64 "\n",
        key,tex->width,tex->height,dds.width,dds.height,dds.levels,resident_bytes);
}
