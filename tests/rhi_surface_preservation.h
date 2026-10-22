/* Real GPU precision conversion and capture-storage preservation. */
#ifndef TSFP_RHI_SURFACE_PRESERVATION_H
#define TSFP_RHI_SURFACE_PRESERVATION_H
static float preservation_half(uint16_t v) {
    unsigned exponent=(v>>10)&31,mantissa=v&1023;
    CHECK(!(v&0x8000) && exponent!=31);
    return exponent?ldexpf(1.f+mantissa/1024.f,(int)exponent-15):ldexpf((float)mantissa,-24);
}
static void rhi_surface_preservation_test(RhiView *restore_target) {
    for(unsigned full=0;full<2;full++)for(unsigned swizzled=0;swizzled<2;swizzled++) {
        D3DFORMAT precise_format=full?D3DFMT_A32B32G32R32F:D3DFMT_A16B16G16R16F;
        unsigned bpp=full?16:8;
        D3DFORMAT format=swizzled?D3DFMT_A8R8G8B8:D3DFMT_LIN_A8R8G8B8;
        IDirect3DTexture8 *source=NULL,*wide=NULL,*restored=NULL,*clone=NULL;
        CHECK(d3d8_CreateTextureImpl(16,16,1,D3DUSAGE_RENDERTARGET,format,&source)==S_OK);
        CHECK(d3d8_CreateTextureImpl(16,16,1,D3DUSAGE_RENDERTARGET,precise_format,&wide)==S_OK);
        CHECK(d3d8_CreateTextureImpl(16,16,1,D3DUSAGE_RENDERTARGET,format,&restored)==S_OK);
        CHECK(d3d8_CreateTextureImpl(16,16,1,D3DUSAGE_RENDERTARGET,precise_format,&clone)==S_OK);
        BYTE pixels[256*4],actual[256*4];uint32_t precise[256*4],clone_pixels[256*4];
        for(unsigned i=0;i<256;i++) {
            pixels[4*i]=(BYTE)i;pixels[4*i+1]=(BYTE)(255-i);
            pixels[4*i+2]=(BYTE)(i*73);pixels[4*i+3]=(BYTE)(i*19);
        }
        /* Deliberately stale CPU bytes: only the GPU contains the live pattern. */
        memset(((D3D8Texture *)source)->sys_mem,0,16*16*4);
        rhi_image_update(((D3D8Texture *)source)->image,0,NULL,pixels,16*4,sizeof pixels);
        RhiViewport original={3,5,19,23,.2f,.8f};
        rhi_set_render_target(restore_target,NULL);rhi_set_viewports(1,&original);
        CHECK(d3d8_PreserveFeedbackTexture(wide,source)==S_OK);
        RhiOutputState output;rhi_output_save(&output);
        CHECK(rhi_output_color_is(&output,restore_target) && output.viewport_count==1);
        CHECK(memcmp(output.viewports,&original,sizeof original)==0);rhi_output_restore(&output);
        CHECK(rhi_image_readback(((D3D8Texture *)wide)->image,0,precise,16*bpp)==0);
        /* WARP's byte-to-half render conversion can truncate (e.g. 254/255
         * becomes 0x3bf7). Permit one half ULP, then require exact byte
         * recovery below; this still rejects lost channels or state. */
        const unsigned channels[4]={2,1,0,3};
        for(unsigned i=0;i<256;i++)for(unsigned c=0;c<4;c++) {
            float actual_value=full?((float *)precise)[4*i+c]:preservation_half(((uint16_t *)precise)[4*i+c]);
            float expected=pixels[4*i+channels[c]]/255.f,tolerance=full?.0000002f:.000489f;
            if(fabsf(actual_value-expected)>tolerance)
                fprintf(stderr,"preserve swizzled=%u pixel=%u channel=%u full=%u value=%.9g expected=%.9g\n",swizzled,i,c,full,actual_value,expected);
            CHECK(fabsf(actual_value-expected)<=tolerance);
        }
        /* Replay initially uploads this CPU storage. Verify both its channel
         * order and swizzle by rebuilding a GPU texture from those bytes. */
        D3DLOCKED_RECT lock;const BYTE *stored;UINT pitch,rows;
        CHECK(d3d8_texture_level((IDirect3DBaseTexture8 *)wide,0,&stored,&pitch,&rows));
        CHECK(clone->lpVtbl->LockRect(clone,0,&lock,NULL,0)==S_OK);
        CHECK((UINT)lock.Pitch==pitch);memcpy(lock.pBits,stored,pitch*rows);
        CHECK(clone->lpVtbl->UnlockRect(clone,0)==S_OK);
        CHECK(rhi_image_readback(((D3D8Texture *)clone)->image,0,clone_pixels,16*bpp)==0);
        CHECK(memcmp(precise,clone_pixels,256*bpp)==0);
        CHECK(d3d8_PreserveFeedbackTexture(restored,wide)==S_OK);
        CHECK(rhi_image_readback(((D3D8Texture *)restored)->image,0,actual,16*4)==0);
        CHECK(memcmp(pixels,actual,sizeof actual)==0); /* every byte value round trips */
        CHECK(rhi_image_readback(((D3D8Texture *)source)->image,0,actual,16*4)==0);
        CHECK(memcmp(pixels,actual,sizeof actual)==0); /* source survives */
        for(unsigned i=0;i<256*4;i++)CHECK(((D3D8Texture *)source)->sys_mem[i]==0);
        /* Capture bytes of the reverse conversion also reconstruct exactly. */
        CHECK(d3d8_texture_level((IDirect3DBaseTexture8 *)restored,0,&stored,&pitch,&rows));
        CHECK(source->lpVtbl->LockRect(source,0,&lock,NULL,0)==S_OK);
        memcpy(lock.pBits,stored,pitch*rows);CHECK(source->lpVtbl->UnlockRect(source,0)==S_OK);
        CHECK(rhi_image_readback(((D3D8Texture *)source)->image,0,actual,16*4)==0);
        CHECK(memcmp(pixels,actual,sizeof actual)==0);
        CHECK(d3d8_PreserveFeedbackTexture(NULL,source)==E_INVALIDARG);
        CHECK(d3d8_PreserveFeedbackTexture(wide,NULL)==E_INVALIDARG);
        CHECK(d3d8_PreserveFeedbackTexture(source,source)==E_INVALIDARG);
        CHECK(source->lpVtbl->LockRect(source,0,&lock,NULL,0)==S_OK);
        CHECK(d3d8_PreserveFeedbackTexture(wide,source)==E_INVALIDARG);
        CHECK(source->lpVtbl->UnlockRect(source,0)==S_OK);
        source->lpVtbl->Release(source);wide->lpVtbl->Release(wide);
        restored->lpVtbl->Release(restored);clone->lpVtbl->Release(clone);
    }
    puts("PASS: feedback byte/half/full-float conversion, live GPU source, capture storage, source and output-state preservation");
}
#endif
