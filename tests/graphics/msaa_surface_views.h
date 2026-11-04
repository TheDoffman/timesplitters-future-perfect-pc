#include "graphics/ssao_pass.h"
/* Production surface wrappers and sampled depth must agree on MSAA view type. */
static void test_msaa_surface_views(RhiView *output)
{
    float pixels[64*64*4];
    RhiRasterDesc raster_desc={0};raster_desc.fill=RHI_FILL_SOLID;raster_desc.cull=RHI_CULL_NONE;raster_desc.multisample=1;
    RhiRasterState *raster=rhi_raster_state_create(&raster_desc);CHECK(raster);
    RhiDepthDesc disabled_desc={0};RhiDepthState *disabled=rhi_depth_state_create(&disabled_desc);CHECK(disabled);
    RhiDepthDesc write_desc={0};write_desc.depth_enable=write_desc.depth_write=1;write_desc.depth_func=RHI_CMP_ALWAYS;
    RhiDepthState *write=rhi_depth_state_create(&write_desc);CHECK(write);
    RhiBlendDesc blend_desc={0};blend_desc.write_mask=RHI_WRITE_ALL;
    RhiBlendState *blend=rhi_blend_state_create(&blend_desc);CHECK(blend);
    for(unsigned samples=1;samples<=8;samples*=2) {
        if(!rhi_sample_count_supported(RHI_FORMAT_D24_UNORM_S8_UINT,samples) ||
           !rhi_sample_count_supported(RHI_FORMAT_B8G8R8A8_UNORM,samples))continue;
        RhiImageDesc desc={0};desc.type=RHI_IMAGE_2D;desc.width=desc.height=64;
        desc.depth=desc.mip_levels=1;desc.samples=samples;
        desc.format=RHI_FORMAT_B8G8R8A8_UNORM;desc.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *color=rhi_image_create(&desc,NULL);CHECK(color);
        IDirect3DSurface8 *color_surface=d3d8_surface_create(color,0,0,64,64,
            D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,D3DUSAGE_RENDERTARGET,
            D3DMULTISAMPLE_NONE,NULL,0);CHECK(color_surface);
        CHECK(((D3D8Surface *)color_surface)->rtv);
        desc.format=RHI_FORMAT_D24_UNORM_S8_UINT;
        desc.bind=RHI_BIND_DEPTH|RHI_BIND_SAMPLED;
        RhiImage *depth_image=rhi_image_create(&desc,NULL);CHECK(depth_image);
        IDirect3DSurface8 *depth_surface=d3d8_surface_create(depth_image,0,0,64,64,
            D3DFMT_D24S8,D3DPOOL_DEFAULT,D3DUSAGE_DEPTHSTENCIL,
            D3DMULTISAMPLE_NONE,NULL,0);CHECK(depth_surface);
        RhiView *dsv=((D3D8Surface *)depth_surface)->dsv;CHECK(dsv);
        RhiView *default_depth=rhi_view_create(depth_image,RHI_VIEW_DEPTH,NULL);CHECK(default_depth);
        RhiView *sampled=rhi_view_create(depth_image,RHI_VIEW_SAMPLED,NULL);CHECK(sampled);
        rhi_clear_depth(dsv,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,.375f,0);
        rhi_set_render_target(output,NULL);
        RhiViewport vp={0,0,64,64,0,1};rhi_set_viewports(1,&vp);
        RhiShader *vs=shader(RHI_STAGE_VERTEX,
            "float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id==2?3:-1,id==1?3:-1,0,1);}","vs_5_0");
        RhiShader *ps=shader(RHI_STAGE_PIXEL,samples==1 ?
            "Texture2D<float> d:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return d.Load(int3(p.xy,0)).xxxx;}" :
            "Texture2DMS<float> d:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{uint w,h,n;d.GetDimensions(w,h,n);float v=0;for(uint i=0;i<n;i++)v+=d.Load(int2(p.xy),i);return (v/n).xxxx;}","ps_5_0");
        rhi_set_raster_state(raster);rhi_set_depth_state(disabled,0);rhi_set_blend_state(blend,NULL,~0u);
        rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
        rhi_set_textures(0,1,&sampled);
        rhi_draw(3,0);
        CHECK(rhi_image_readback(target,0,pixels,64*4*sizeof(float))==0);
        for(unsigned i=0;i<64*64*4;i++)CHECK(fabsf(pixels[i]-.375f)<.0001f);
        RhiView *empty=NULL;rhi_set_textures(0,1,&empty);
        TsfpSsaoPass ao={0};
        const float near_plane=.1f,far_plane=100.f;
        float a=far_plane/(far_plane-near_plane),b=-near_plane*far_plane/(far_plane-near_plane);
        float inverse[16]={1,0,0,0,0,1,0,0,0,0,0,1/b,0,0,1,-a/b};
        CHECK(tsfp_ssao_render(&ao,sampled,64,64,inverse,1,.1f,.02f,2));
        CHECK(ao.depth_samples==samples);
        float ao_pixels[32*32*2];
        CHECK(rhi_image_readback(ao.image,0,ao_pixels,32*2*sizeof(float))==0);
        for(unsigned i=0;i<32*32;i++) {
            CHECK(fabsf(ao_pixels[i*2]-1)<.0001f);
            CHECK(fabsf(ao_pixels[i*2+1]-b/(.375f-a))<.0001f);
        }
        // Different depths in the samples must select the nearest, not average.
        char depth_code[256];snprintf(depth_code,sizeof depth_code,
            "float main(uint s:SV_SampleIndex):SV_Depth{return .25+.5*s/%u.0;}",samples>1?samples-1:1);
        RhiShader *depth_writer=shader(RHI_STAGE_PIXEL,depth_code,"ps_5_0");
        rhi_set_render_target(((D3D8Surface *)color_surface)->rtv,dsv);
        rhi_set_viewports(1,&vp);rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,depth_writer);
        rhi_set_raster_state(raster);rhi_set_depth_state(write,0);rhi_set_blend_state(blend,NULL,~0u);
        rhi_draw(3,0);
        CHECK(tsfp_ssao_render(&ao,sampled,64,64,inverse,1,.1f,.02f,2));
        CHECK(rhi_image_readback(ao.image,0,ao_pixels,32*2*sizeof(float))==0);
        for(unsigned i=0;i<32*32;i++)CHECK(fabsf(ao_pixels[i*2+1]-b/(.25f-a))<.0001f);
        rhi_set_render_target(output,NULL);rhi_set_raster_state(raster);
        rhi_set_depth_state(disabled,0);rhi_set_blend_state(blend,NULL,~0u);
        rhi_shader_destroy(depth_writer);tsfp_ssao_destroy(&ao);
        rhi_shader_destroy(vs);rhi_shader_destroy(ps);
        rhi_view_destroy(sampled);rhi_view_destroy(default_depth);
        depth_surface->lpVtbl->Release(depth_surface);
        color_surface->lpVtbl->Release(color_surface);
        rhi_image_destroy(depth_image);rhi_image_destroy(color);
    }
    rhi_set_render_target(output,NULL);
    rhi_raster_state_destroy(raster);rhi_depth_state_destroy(disabled);rhi_depth_state_destroy(write);rhi_blend_state_destroy(blend);
    puts("PASS: MSAA surface views, sampled depth and nearest-surface AO");
}

static void test_guest_surface_companion(RhiView *output)
{
    RhiImageDesc d={0};d.type=RHI_IMAGE_2D;d.width=d.height=16;
    d.depth=d.mip_levels=d.samples=1;d.format=RHI_FORMAT_B8G8R8A8_UNORM;
    d.bind=RHI_BIND_RENDER_TARGET|RHI_BIND_SAMPLED;
    RhiImage *image=rhi_image_create(&d,NULL);CHECK(image);
    IDirect3DSurface8 *iface=d3d8_surface_create(image,0,0,16,16,D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,D3DUSAGE_RENDERTARGET,D3DMULTISAMPLE_NONE,NULL,0);CHECK(iface);
    D3D8Surface *surface=(D3D8Surface *)iface;
    CHECK(d3d8_surface_msaa_prepare(surface,1)==surface->rtv);
    const float green[4]={0,1,0,1};rhi_clear_color(surface->rtv,green);
    unsigned samples=4;while(samples>1&&!rhi_sample_count_supported(d.format,samples))samples/=2;
    CHECK(samples>1);
    RhiView *ms=d3d8_surface_msaa_prepare(surface,samples);CHECK(ms&&ms!=surface->rtv);
    CHECK(d3d8_surface_msaa_resolve(surface));
    unsigned char pixels[16*16*4];CHECK(rhi_image_readback(image,0,pixels,16*4)==0);
    CHECK(pixels[1]==255&&pixels[0]==0);
    const float black[4]={0,0,0,1};rhi_clear_color(ms,black);
    CHECK(d3d8_surface_msaa_prepare(surface,1)==surface->rtv);
    CHECK(rhi_image_readback(image,0,pixels,16*4)==0&&pixels[1]==0);
    rhi_clear_color(surface->rtv,green); /* simulate a newer copy/CPU write */
    CHECK(d3d8_surface_msaa_prepare(surface,samples)==ms);
    CHECK(d3d8_surface_msaa_resolve(surface));
    CHECK(rhi_image_readback(image,0,pixels,16*4)==0&&pixels[1]==255);
    /* Locking an active companion reads its newest GPU contents, then a
     * partial CPU edit must survive the next resolve without losing neighbors. */
    rhi_clear_color(ms,black);
    D3DLOCKED_RECT locked;RECT region={2,3,4,5};
    CHECK(SUCCEEDED(iface->lpVtbl->LockRect(iface,&locked,&region,D3DLOCK_READONLY)));
    CHECK(((unsigned char *)locked.pBits)[1]==0);
    CHECK(SUCCEEDED(iface->lpVtbl->UnlockRect(iface)));
    CHECK(SUCCEEDED(iface->lpVtbl->LockRect(iface,&locked,&region,0)));
    for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++){
        unsigned char *pixel=(unsigned char *)locked.pBits+y*locked.Pitch+x*4;
        pixel[0]=0;pixel[1]=255;pixel[2]=0;pixel[3]=255;
    }
    CHECK(SUCCEEDED(iface->lpVtbl->UnlockRect(iface)));
    CHECK(d3d8_surface_msaa_resolve(surface));
    CHECK(rhi_image_readback(image,0,pixels,16*4)==0);
    for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)
        CHECK(pixels[(y*16+x)*4+1]==((x>=2&&x<4&&y>=3&&y<5)?255:0));
    IDirect3DSurface8 *alias=d3d8_surface_create(image,0,0,16,16,D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,D3DUSAGE_RENDERTARGET,D3DMULTISAMPLE_NONE,NULL,0);CHECK(alias);
    rhi_clear_color(ms,green);
    CHECK(SUCCEEDED(alias->lpVtbl->LockRect(alias,&locked,&region,0)));
    CHECK(((unsigned char *)locked.pBits)[1]==255);
    ((unsigned char *)locked.pBits)[1]=83;
    CHECK(SUCCEEDED(alias->lpVtbl->UnlockRect(alias)));
    alias->lpVtbl->Release(alias);
    CHECK(d3d8_surface_msaa_resolve(surface));
    CHECK(rhi_image_readback(image,0,pixels,16*4)==0);
    CHECK(pixels[(3*16+2)*4+1]==83&&pixels[1]==255);
    surface->locked=TRUE;CHECK(!d3d8_surface_msaa_prepare(surface,samples));surface->locked=FALSE;
    rhi_clear_color(ms,black);rhi_set_render_target(output,NULL);
    iface->lpVtbl->Release(iface); /* final release flushes pending color */
    CHECK(rhi_image_readback(image,0,pixels,16*4)==0&&pixels[1]==0);
    iface=d3d8_surface_create(image,0,0,16,16,D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,D3DUSAGE_RENDERTARGET,D3DMULTISAMPLE_NONE,NULL,0);CHECK(iface);
    surface=(D3D8Surface *)iface;
    CHECK(d3d8_surface_msaa_prepare(surface,samples));
    CHECK(d3d8_surface_msaa_prepare(surface,1));
    rhi_clear_color(surface->rtv,green);
    iface->lpVtbl->Release(iface); /* inactive companion must not overwrite newer original */
    CHECK(rhi_image_readback(image,0,pixels,16*4)==0&&pixels[1]==255);
    rhi_image_destroy(image);
    puts("PASS: guest surface MSAA companion bind, reseed, resolve and release");
}
