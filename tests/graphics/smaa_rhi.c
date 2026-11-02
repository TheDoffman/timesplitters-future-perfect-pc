/* Production display resolve + SMAA on real RHI resources. Synthetic pixels
 * exercise the full lookup-table algorithm without game assets or saves. */
#define COBJMACROS
#include <d3d11.h>
#include "rhi.h"
#include "d3d8_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int fail_shader, fail_image, fail_map;
static RhiShader *smaa_test_shader_create(unsigned stage,const RhiShaderSource *s,char *e,size_t n)
{
    if(fail_shader && !strcmp(s->name,"smaa_1x")) return NULL;
    return rhi_shader_create(stage,s,e,n);
}
static RhiImage *smaa_test_image_create(const RhiImageDesc *d,const RhiSubresourceData *s)
{
    if(fail_image && --fail_image==0) return NULL;
    return rhi_image_create(d,s);
}
static void *smaa_test_buffer_map(RhiBuffer *b,unsigned mode)
{
    if(fail_map && --fail_map==0) return NULL;
    return rhi_buffer_map(b,mode);
}
#define rhi_shader_create smaa_test_shader_create
#define rhi_image_create smaa_test_image_create
#define rhi_buffer_map smaa_test_buffer_map
#include "d3d8_display.c"
#undef rhi_shader_create
#undef rhi_image_create
#undef rhi_buffer_map

static void run_case(unsigned w,unsigned h,int pattern,int enabled,int expect_change)
{
    size_t size=(size_t)w*h*4;
    unsigned char *input=malloc(size),*output=malloc(size),*reference=malloc(size);
    CHECK(input && output && reference);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        unsigned char *p=input+((size_t)y*w+x)*4;
        unsigned value=pattern==0?83:pattern==1?(x>y*3/5+w/8?240:16):(x>w/2?240:16);
        p[0]=p[1]=p[2]=(unsigned char)value;p[3]=255;
    }
    RhiImageDesc d={0};d.width=w;d.height=h;d.depth=d.mip_levels=d.samples=1;
    d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData data={input,w*4,(unsigned)size};
    RhiImage *scene=rhi_image_create(&d,&data);CHECK(scene);
    RhiView *srv=rhi_view_create(scene,RHI_VIEW_SAMPLED,NULL);CHECK(srv);
    d.bind=RHI_BIND_RENDER_TARGET;
    RhiImage *target=rhi_image_create(&d,NULL);CHECK(target);
    RhiView *rtv=rhi_view_create(target,RHI_VIEW_RENDER_TARGET,NULL);CHECK(rtv);
    RhiViewport vp={2,3,5,7,0,1};
    rhi_set_render_target(rtv,NULL);rhi_set_viewports(1,&vp);
    D3D8DisplayFit fit={0,0,w,h};
    _putenv_s("TSFP_SMAA","0");
    CHECK(SUCCEEDED(d3d8_display_resolve(srv,w,h,rtv,fit)));
    CHECK(rhi_image_readback(target,0,reference,w*4)==0);
    if(pattern==0) CHECK(!memcmp(input,reference,size));
    _putenv_s("TSFP_SMAA",enabled?"1":"0");
    CHECK(SUCCEEDED(d3d8_display_resolve(srv,w,h,rtv,fit)));
    CHECK(rhi_image_readback(target,0,output,w*4)==0);
    unsigned changed=0,intermediate=0;
    for(size_t i=0;i<size;i+=4) {
        if(output[i]!=reference[i])changed++;
        if(output[i]>16 && output[i]<240)intermediate++;
        CHECK(output[i]==output[i+1] && output[i]==output[i+2]);
        CHECK(output[i+3]==255);
    }
    if(expect_change) CHECK(changed>0 && intermediate>0 && changed<w*h/3);
    else CHECK(!changed);
    RhiOutputState saved;rhi_output_save(&saved);
    CHECK(saved.viewport_count==1 && saved.viewports[0].x==vp.x && saved.viewports[0].y==vp.y);
    rhi_output_restore(&saved);
    /* A fitted picture must leave black bars and retain its flat interior. */
    if(pattern==0 && enabled) {
        fit.x=4;fit.y=3;fit.w=w-8;fit.h=h-6;
        CHECK(SUCCEEDED(d3d8_display_resolve(srv,w,h,rtv,fit)));
        CHECK(rhi_image_readback(target,0,output,w*4)==0);
        CHECK(output[0]==0 && output[((h/2)*w+w/2)*4]==83);
    }
    CHECK(FAILED(d3d8_display_resolve(NULL,w,h,rtv,fit)));
    rhi_set_render_target(NULL,NULL);
    rhi_view_destroy(srv);rhi_view_destroy(rtv);
    rhi_image_destroy(scene);rhi_image_destroy(target);
    free(input);free(output);free(reference);
}
int main(void)
{
    RecompSettings settings, roundtrip;
    recomp_settings_defaults(&settings); CHECK(!settings.smaa);
    const char *config=getenv("RECOMP_DISPLAY_CONFIG");CHECK(config);
    settings.smaa=1;settings.anisotropy=16;
    CHECK(recomp_settings_write(config,&settings));
    CHECK(recomp_settings_read(config,&roundtrip) && roundtrip.smaa && roundtrip.anisotropy==16);
    settings.smaa=0;CHECK(recomp_settings_write(config,&settings));
    CHECK(recomp_settings_read(config,&roundtrip) && !roundtrip.smaa);
    ID3D11Device *device=NULL;ID3D11DeviceContext *context=NULL;D3D_FEATURE_LEVEL feature;
    D3D_DRIVER_TYPE driver=getenv("SMAA_TEST_HARDWARE") ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;
    HRESULT device_result=D3D11CreateDevice(NULL,driver,NULL,0,NULL,0,
        D3D11_SDK_VERSION,&device,&feature,&context);
    if(FAILED(device_result) && driver==D3D_DRIVER_TYPE_HARDWARE) {
        puts("SKIP: hardware D3D11 device unavailable; WARP remains required");return 77;
    }
    CHECK(SUCCEEDED(device_result));
    CHECK(rhi_d3d11_adopt(device,context)==0);
    run_case(64,64,1,0,0); CHECK(!smaa.tried);
    run_case(64,64,1,1,1);
    run_case(64,64,0,1,0);
    run_case(64,64,2,1,0);
    run_case(97,53,1,1,1); /* odd resize */
    run_case(97,53,1,0,0); /* disabling restores original */
    d3d8_display_shutdown();
    fail_shader=1;run_case(64,64,1,1,0);CHECK(smaa.failed);
    fail_shader=0;d3d8_display_shutdown();
    /* Fail each immutable/intermediate image; original resolve must survive. */
    for(int i=1;i<=5;i++) {
        fail_image=i;run_case(64,64,1,1,0);CHECK(smaa.failed);
        fail_image=0;d3d8_display_shutdown();
    }
    fail_map=2;run_case(64,64,1,1,0);CHECK(smaa.failed);
    d3d8_display_shutdown();
    run_case(64,64,1,1,1); /* device-pass resources can be recreated */
    /* The game menu publishes live only after a selective, atomic save. */
    FILE *preferences=fopen(config,"wb");CHECK(preferences);
    fputs("# retained comment\r\nfuture_graphics = keep\r\nsmaa = 0\r\nanisotropy = 16\r\n",preferences);
    CHECK(!fclose(preferences));_putenv_s("TSFP_SMAA","0");
    CHECK(pc_smaa_set_enabled(1) && pc_smaa_enabled());
    CHECK(pc_smaa_prepare(97,53));
    CHECK(recomp_settings_read(config,&roundtrip) && roundtrip.smaa && roundtrip.anisotropy==16);
    char preserved[256]={0};preferences=fopen(config,"rb");CHECK(preferences);
    fread(preserved,1,sizeof preserved-1,preferences);CHECK(!fclose(preferences));
    CHECK(strstr(preserved,"# retained comment\r\nfuture_graphics = keep\r\n"));
    CHECK(SetFileAttributesA(config,FILE_ATTRIBUTE_READONLY));
    CHECK(!pc_smaa_set_enabled(0) && pc_smaa_enabled());
    CHECK(SetFileAttributesA(config,FILE_ATTRIBUTE_NORMAL));
    CHECK(pc_smaa_set_enabled(0) && !pc_smaa_enabled() && !pc_smaa_prepare(97,53));
    CHECK(recomp_settings_read(config,&roundtrip) && !roundtrip.smaa);
    d3d8_display_shutdown();rhi_device_destroy();
    puts("PASS: SMAA edges, flat/axis pixels, alpha, resize, bars, state, Off, failures and recreation");
}
