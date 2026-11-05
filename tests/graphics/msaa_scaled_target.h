/* A console-sized single-sample texture must resize before acquiring its
 * multisample draw companion. Guest sampling must remain Texture2D. */
static void test_msaa_scaled_target(void)
{
    unsigned exercised=0;
    for(unsigned samples=1;samples<=8;samples*=2) {
        if(!rhi_sample_count_supported(RHI_FORMAT_D24_UNORM_S8_UINT,samples) ||
           !rhi_sample_count_supported(RHI_FORMAT_B8G8R8A8_UNORM,samples))continue;
        RhiImageDesc d={0};d.type=RHI_IMAGE_2D;d.width=32;d.height=24;
        d.depth=d.mip_levels=1;d.samples=samples;
        d.format=RHI_FORMAT_D24_UNORM_S8_UINT;d.bind=RHI_BIND_DEPTH;
        RhiImage *depth=rhi_image_create(&d,NULL);CHECK(depth);
        IDirect3DSurface8 *ds=d3d8_surface_create(depth,0,0,32,24,D3DFMT_D24S8,
            D3DPOOL_DEFAULT,D3DUSAGE_DEPTHSTENCIL,D3DMULTISAMPLE_NONE,NULL,0);CHECK(ds);
        IDirect3DTexture8 *tex=NULL;
        CHECK(SUCCEEDED(d3d8_CreateTextureImpl(16,12,1,D3DUSAGE_RENDERTARGET,
            D3DFMT_A8R8G8B8,&tex)) && tex);
        D3D8Texture *t=(D3D8Texture *)tex;
        RhiView *old=rhi_view_create(t->image,RHI_VIEW_RENDER_TARGET,NULL);CHECK(old);
        const float yellow[4]={1,1,0,1};rhi_clear_color(old,yellow);
        rhi_view_destroy(old);rhi_set_render_target(NULL,NULL);
        copy_scene=t->image;copy_scene_srv=t->srv;copy_width=16;copy_height=12;
        CHECK(SUCCEEDED(xbox_D3D8PrepareSharedDepthTarget(tex,ds)));
        copy_scene=NULL;copy_scene_srv=NULL;
        rhi_image_get_desc(t->image,&d);
        CHECK(d.width==32 && d.height==24 && d.samples==1);
        CHECK(t->width==16 && t->height==12);
        unsigned char pixels[32*24*4];
        CHECK(rhi_image_readback(t->image,0,pixels,32*4)==0);
        for(unsigned i=0;i<32*24;i++)
            CHECK(pixels[i*4]==0 && pixels[i*4+1]==255 && pixels[i*4+2]==255);
        IDirect3DSurface8 *surface=NULL;
        CHECK(SUCCEEDED(tex->lpVtbl->GetSurfaceLevel(tex,0,&surface)) && surface);
        D3D8Surface *sf=(D3D8Surface *)surface;
        CHECK(d3d8_surface_msaa_prepare(sf,samples));
        rhi_image_get_desc(rhi_view_image(d3d8_surface_draw_view(sf)),&d);
        CHECK(d.width==32 && d.height==24 && d.samples==samples);
        CHECK(SUCCEEDED(xbox_D3D8PrepareSharedDepthTarget(tex,ds)));
        rhi_set_render_target(NULL,NULL);
        surface->lpVtbl->Release(surface);tex->lpVtbl->Release(tex);
        ds->lpVtbl->Release(ds);rhi_image_destroy(depth);exercised++;
    }
    CHECK(exercised>1);
    xbox_D3D8ScreenCopyShutdown();
    puts("PASS: resized guest color remains sampleable with shared MSAA depth");
}
