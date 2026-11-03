/* Production Xbox screen-copy passes on both backends. Only device-owned
 * scene discovery is supplied by this fixture; copies, shaders, resources,
 * output-state save/restore and GPU execution use the real implementation. */
#ifndef TSFP_RHI_SCREEN_COPY_H
#define TSFP_RHI_SCREEN_COPY_H
#include "d3d8_internal.h"
#include "d3d8_xbox.h"
static RhiImage *copy_scene;
static RhiView *copy_scene_srv, *copy_scene_rtv;
static UINT copy_width, copy_height;
UINT d3d8_GetBackBufferWidth(void) {return copy_width;}
UINT d3d8_GetBackBufferHeight(void) {return copy_height;}
UINT d3d8_GetGuestWidth(void) {return 16;}
UINT d3d8_GetGuestHeight(void) {return 12;}
RhiImage *d3d8_GetSceneImage(void) {return copy_scene;}
RhiView *d3d8_GetSceneView(void) {return copy_scene_srv;}
RhiView *d3d8_GetDefaultTargetView(void) {return copy_scene_rtv;}

#include "rhi_surface_preservation.h"

static void copy_texel(unsigned x,unsigned y,unsigned char rgba[4]) {
    rgba[0]=(unsigned char)(20+x*5);rgba[1]=(unsigned char)(30+y*7);
    rgba[2]=(unsigned char)(40+(x+y)*2);rgba[3]=255;
}
static void copy_expect(IDirect3DTexture8 *texture,const RECT *src,const POINT *at,int copied) {
    unsigned char pixels[8*6*4];
    CHECK(rhi_image_readback(((D3D8Texture *)texture)->image,0,pixels,8*4)==0);
    for(int y=0;y<6;y++)for(int x=0;x<8;x++) {
        int ax=at?at->x:0,ay=at?at->y:0;
        int inside=copied && (!src || (x>=ax && y>=ay && x<ax+src->right-src->left && y<ay+src->bottom-src->top));
        unsigned char expected[4]={0,0,0,255};
        if(inside) {
            float sx=src?(src->left+x+.5f-ax)*copy_width/16:(x+.5f)*copy_width/8;
            float sy=src?(src->top+y+.5f-ay)*copy_height/12:(y+.5f)*copy_height/6;
            if(sx>=0 && sy>=0 && sx<copy_width && sy<copy_height)copy_texel((unsigned)sx,(unsigned)sy,expected);
        }
        const unsigned order[4]={2,1,0,3}; /* destination is Xbox BGRA */
        for(unsigned c=0;c<4;c++)if(pixels[(y*8+x)*4+c]!=expected[order[c]]) {
            fprintf(stderr,"screen-copy pixel (%d,%d) channel %u: %u != %u (scene %ux%u)\n",x,y,c,pixels[(y*8+x)*4+c],expected[order[c]],copy_width,copy_height);
            CHECK(0);
        }
    }
}
static void rhi_screen_copy_test(RhiView *restore_target) {
    const struct {RECT rect;POINT at;int copies;} cases[]={
        {{2,3,6,6},{1,2},1}, {{2,3,7,7},{-2,-1},1},
        {{2,3,8,8},{6,4},1}, {{2,3,4,5},{-5,-4},0},
        {{4,3,4,5},{1,1},0}, {{-2,-1,4,4},{0,0},1},
        {{14,10,20,15},{1,1},1}, {{-10,-8,-1,-1},{0,0},0},
        {{0,0,4,4},{8,6},0}, {{0,0,4,4},{INT32_MIN,INT32_MIN},0}
    };
    RhiViewport original={7,9,21,23,.2f,.8f};
    for(unsigned scale=1;scale<=2;scale++) {
        copy_width=16*scale;copy_height=12*scale;
        unsigned char source[32*24*4];
        for(unsigned y=0;y<copy_height;y++)for(unsigned x=0;x<copy_width;x++)copy_texel(x,y,source+(y*copy_width+x)*4);
        RhiImageDesc image={0};image.width=copy_width;image.height=copy_height;
        image.depth=image.mip_levels=image.samples=1;image.format=RHI_FORMAT_R8G8B8A8_UNORM;
        image.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
        RhiSubresourceData data={source,copy_width*4,copy_width*copy_height*4};
        copy_scene=rhi_image_create(&image,&data);CHECK(copy_scene);
        copy_scene_srv=rhi_view_create(copy_scene,RHI_VIEW_SAMPLED,NULL);
        copy_scene_rtv=rhi_view_create(copy_scene,RHI_VIEW_RENDER_TARGET,NULL);CHECK(copy_scene_srv&&copy_scene_rtv);
        IDirect3DTexture8 *texture=NULL;
        CHECK(d3d8_CreateTextureImpl(8,6,1,D3DUSAGE_RENDERTARGET,D3DFMT_LIN_A8R8G8B8,&texture)==S_OK);
        RhiView *dest=rhi_view_create(((D3D8Texture *)texture)->image,RHI_VIEW_RENDER_TARGET,NULL);CHECK(dest);
        const float black[4]={0,0,0,1};
        for(unsigned test=0;test<=sizeof cases/sizeof cases[0];test++) {
            rhi_clear_color(dest,black);
            rhi_set_render_target(restore_target,NULL);rhi_set_viewports(1,&original);
            unsigned long before=xbox_D3D8ScreenCopyCount();
            if(test==0) CHECK(xbox_D3D8CopyBackBufferToTexture(texture)==S_OK);
            else CHECK(xbox_D3D8CopyBackBufferRectToTexture(texture,&cases[test-1].rect,&cases[test-1].at)==S_OK);
            RhiOutputState restored;rhi_output_save(&restored);
            CHECK(rhi_output_color_is(&restored,restore_target));
            CHECK(restored.viewport_count==1 && memcmp(restored.viewports,&original,sizeof original)==0);
            rhi_output_restore(&restored);
            copy_expect(texture,test?&cases[test-1].rect:NULL,test?&cases[test-1].at:NULL,1);
            CHECK(((D3D8Texture *)texture)->screen_copy==1);
            CHECK(xbox_D3D8ScreenCopyCount()==before+(test?cases[test-1].copies:1));
        }
        /* The Uplink composes its map before a clear, then copies a region
         * from that saved image into the small display after the clear. */
        IDirect3DTexture8 *saved=NULL;
        CHECK(d3d8_CreateTextureImpl(16,12,1,D3DUSAGE_RENDERTARGET,
                                      D3DFMT_LIN_A8R8G8B8,&saved)==S_OK);
        CHECK(xbox_D3D8CopyBackBufferToTexture(saved)==S_OK);
        rhi_clear_color(copy_scene_rtv,black);
        rhi_clear_color(dest,black);
        RECT map_region={4,2,12,10};
        CHECK(xbox_D3D8CopyTextureRegionScaled(texture,saved,&map_region)==S_OK);
        unsigned char map_pixels[8*6*4];
        CHECK(rhi_image_readback(((D3D8Texture *)texture)->image,0,map_pixels,8*4)==0);
        CHECK(map_pixels[0]!=0 && map_pixels[8*6*4-2]!=0);
        saved->lpVtbl->Release(saved);
        /* Explicit offscreen source differs from the current scene and size. */
        image.width=4;image.height=2;
        RhiImage *offscreen=rhi_image_create(&image,NULL);CHECK(offscreen);
        RhiView *offscreen_rtv=rhi_view_create(offscreen,RHI_VIEW_RENDER_TARGET,NULL);
        RhiView *offscreen_srv=rhi_view_create(offscreen,RHI_VIEW_SAMPLED,NULL);CHECK(offscreen_rtv&&offscreen_srv);
        const float blue[4]={0,0,1,1};rhi_clear_color(offscreen_rtv,blue);
        IDirect3DSurface8 *copy_surface=NULL;
        CHECK(texture->lpVtbl->GetSurfaceLevel(texture,0,&copy_surface)==S_OK);
        unsigned copy_samples=4;
        while(copy_samples>1&&!rhi_sample_count_supported(RHI_FORMAT_B8G8R8A8_UNORM,copy_samples))copy_samples/=2;
        CHECK(copy_samples>1);
        RhiView *copy_ms=d3d8_surface_msaa_prepare((D3D8Surface *)copy_surface,copy_samples);CHECK(copy_ms);
        rhi_clear_color(copy_ms,black);
        IDirect3DSurface8 *source_surface=d3d8_surface_create(offscreen,0,0,4,2,
            D3DFMT_A8B8G8R8,D3DPOOL_DEFAULT,D3DUSAGE_RENDERTARGET,
            D3DMULTISAMPLE_NONE,NULL,0);CHECK(source_surface);
        rhi_clear_color(offscreen_rtv,black);
        RhiView *source_ms=d3d8_surface_msaa_prepare((D3D8Surface *)source_surface,copy_samples);CHECK(source_ms);
        rhi_clear_color(source_ms,blue);

        CHECK(d3d8_CopyTextureView(texture,offscreen_srv)==S_OK && !((D3D8Texture *)texture)->screen_copy);
        CHECK(d3d8_surface_msaa_resolve((D3D8Surface *)copy_surface));
        unsigned char pixels[8*6*4];CHECK(rhi_image_readback(((D3D8Texture *)texture)->image,0,pixels,8*4)==0);
        for(unsigned i=0;i<8*6;i++)CHECK(pixels[4*i]==255 && pixels[4*i+1]==0 && pixels[4*i+2]==0 && pixels[4*i+3]==255);
        /* Partial copy changes only the requested area of an active target.
         * Its untouched green pixels exist only in multisampled coverage. */
        const float green[4]={0,1,0,1};rhi_clear_color(copy_ms,green);
        RECT partial={0,0,2,2};POINT offset={3,2};
        CHECK(xbox_D3D8CopyBackBufferRectToTexture(texture,&partial,&offset)==S_OK);
        CHECK(d3d8_surface_msaa_resolve((D3D8Surface *)copy_surface));
        CHECK(rhi_image_readback(((D3D8Texture *)texture)->image,0,pixels,8*4)==0);
        for(unsigned y=0;y<6;y++)for(unsigned x=0;x<8;x++){
            unsigned char *pixel=pixels+(y*8+x)*4;
            int inside=x>=3&&x<5&&y>=2&&y<4;
            CHECK(pixel[0]==0&&pixel[1]==(inside?0:255)&&pixel[2]==0&&pixel[3]==255);
        }
        D3DLOCKED_RECT upload;
        CHECK(texture->lpVtbl->LockRect(texture,0,&upload,NULL,0)==S_OK);
        for(unsigned y=0;y<6;y++)memset((unsigned char *)upload.pBits+y*upload.Pitch,99,8*4);
        CHECK(texture->lpVtbl->UnlockRect(texture,0)==S_OK);
        CHECK(d3d8_surface_msaa_resolve((D3D8Surface *)copy_surface));
        CHECK(rhi_image_readback(((D3D8Texture *)texture)->image,0,pixels,8*4)==0);
        for(unsigned i=0;i<sizeof pixels;i++)CHECK(pixels[i]==99);
        copy_surface->lpVtbl->Release(copy_surface);
        source_surface->lpVtbl->Release(source_surface);
        rhi_view_destroy(offscreen_rtv);rhi_view_destroy(offscreen_srv);rhi_image_destroy(offscreen);
        CHECK(xbox_D3D8CopyBackBufferToTexture(NULL)==E_INVALIDARG);
        CHECK(d3d8_CopyTextureView(texture,NULL)==E_INVALIDARG);
        if(scale==1)rhi_surface_preservation_test(restore_target);
        xbox_D3D8ScreenCopyShutdown();
        rhi_view_destroy(dest);texture->lpVtbl->Release(texture);
        rhi_view_destroy(copy_scene_srv);rhi_view_destroy(copy_scene_rtv);rhi_image_destroy(copy_scene);
        copy_scene=NULL;copy_scene_srv=copy_scene_rtv=NULL;
    }
    puts("PASS: shared Xbox full/partial screen copies, clipping, scaled sources, offscreen snapshots and output restoration");
}
#endif
