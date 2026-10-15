/* Exercise the real loader against tiny DDS fixtures and counted RHI stubs.
 * No GPU, game data, personal settings or saves are needed. */
#include "d3d8_internal.h"
#include "texture_pack.h"
#include "texture_pack_format.h"
#include "texture_mode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int images, views, allocations, fail_view;
RhiFormat d3d8_to_dxgi_format(D3DFORMAT fmt) { return (RhiFormat)fmt; }
RhiImage *rhi_image_create(const RhiImageDesc *desc,const RhiSubresourceData *data)
{
    if(desc->width!=4 || desc->height!=4 || desc->mip_levels!=3 || !data[2].data) abort();
    images++; allocations++;
    return (RhiImage *)malloc(1);
}
void rhi_image_destroy(RhiImage *image) { if(image) { images--; free(image); } }
RhiView *rhi_view_create(RhiImage *image,uint32_t kind,const RhiViewDesc *desc)
{
    if(!image || kind!=RHI_VIEW_SAMPLED || desc->mip_count!=3) abort();
    if(fail_view) return NULL;
    views++; return (RhiView *)malloc(1);
}
void rhi_view_destroy(RhiView *view) { if(view) { views--; free(view); } }
static void put(unsigned char *p,uint32_t x)
{ p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24); }
static int fixture(const char *path,int valid)
{
    unsigned char dds[152]={0}; FILE *f;
    put(dds,valid?0x20534444:0); put(dds+4,124);put(dds+12,4);put(dds+16,4);
    put(dds+28,3);put(dds+76,32);put(dds+80,4);put(dds+84,0x31545844);
    f=fopen(path,"wb");if(!f) return 0;
    int ok=fwrite(dds,1,sizeof dds,f)==sizeof dds;
    return fclose(f)==0 && ok;
}
int main(int argc,char **argv)
{
    unsigned char data[8]={0,1,2,3,4,5,6,7}, changed[8]={9,1,2,3,4,5,6,7};
    D3D8Texture texture={0}, missing={0}, bad={0};
    char path[4096]; int initial, cached_allocations;
    RhiView *cached;
    CHECK(argc==3);
    _putenv_s("TSFP_TEXTURE_DUMP_DIR","");
    _putenv_s("TSFP_TEXTURE_PACK_DIR",!strcmp(argv[2],"missing") ? "" : argv[1]);
    _putenv_s("TSFP_TEXTURE_MODE",!strcmp(argv[2],"implicit") ? "" : argv[2]);
    /* getenv returns an empty string as unset with _putenv_s on Windows. */
    initial=!strcmp(argv[2],"4k") || !strcmp(argv[2],"implicit");
    CHECK(tsfp_texture_is_4k()==initial);
    texture.width=texture.height=4;texture.d3d8_format=12;texture.sys_mem=data;
    texture.srv=(RhiView *)(uintptr_t)1;
    snprintf(path,sizeof path,"%s/%016" PRIx64 ".dds",argv[1],tsfp_texture_key(12,4,4,data,8));
    CHECK(fixture(path,1));
    tsfp_texture_upload(&texture);
    if(!strcmp(argv[2],"missing")) {
        CHECK(!images && !views && !texture.pack_srv);
        CHECK(tsfp_texture_toggle());
        CHECK(tsfp_texture_view(&texture)==texture.srv);
        CHECK(remove(path)==0);
        puts("PASS: absent pack falls back to Classic in both modes"); return 0;
    }
    CHECK(images==1 && views==1 && texture.pack_bytes==24);
    cached=texture.pack_srv;cached_allocations=allocations;
    CHECK(tsfp_texture_view(&texture)==(initial?cached:texture.srv));
    for(int i=0;i<20;i++) {
        int mode=tsfp_texture_toggle();
        tsfp_texture_upload(&texture);
        CHECK(tsfp_texture_view(&texture)==(mode?cached:texture.srv));
        CHECK(allocations==cached_allocations && images==1 && views==1);
        CHECK(texture.width==4 && texture.height==4 && texture.sys_mem==data && data[0]==0);
    }
    if(!tsfp_texture_is_4k()) tsfp_texture_toggle();
    texture.screen_copy=1;CHECK(tsfp_texture_view(&texture)==texture.srv);
    texture.screen_copy=0;texture.usage=1;CHECK(tsfp_texture_view(&texture)==texture.srv);
    texture.usage=0;CHECK(tsfp_texture_view(&texture)==cached);
    missing=texture;missing.pack_srv=NULL;missing.pack_image=NULL;missing.pack_bytes=0;
    missing.pack_checked=0;missing.sys_mem=changed;
    tsfp_texture_upload(&missing);CHECK(tsfp_texture_view(&missing)==missing.srv);
    bad=missing;bad.usage=1;bad.sys_mem=data;
    tsfp_texture_upload(&bad);CHECK(!bad.pack_srv);
    bad.usage=0;bad.screen_copy=1;tsfp_texture_upload(&bad);CHECK(!bad.pack_srv);
    bad.screen_copy=0;bad.pack_checked=0;
    CHECK(fixture(path,0));tsfp_texture_upload(&bad);CHECK(!bad.pack_srv);
    CHECK(fixture(path,1));bad.pack_checked=0;fail_view=1;
    tsfp_texture_upload(&bad);CHECK(!bad.pack_srv && !bad.pack_image && images==1);
    fail_view=0;bad.pack_checked=0;tsfp_texture_upload(&bad);
    CHECK(bad.pack_srv && images==2 && views==2);
    tsfp_texture_release(&bad);CHECK(images==1 && views==1);
    texture.sys_mem=changed;tsfp_texture_upload(&texture);
    CHECK(!texture.pack_srv && !texture.pack_bytes && !images && !views);
    CHECK(tsfp_texture_view(&texture)==texture.srv);
    tsfp_texture_release(&texture);tsfp_texture_release(&texture);
    CHECK(!images && !views);CHECK(remove(path)==0);
    puts("PASS: live cached view switches, original preservation, missing/invalid pack fallback, screen-copy exclusion, content changes and cleanup");
    return 0;
}
