/* Exercise the production Xbox factories on each real RHI backend, not mock
 * resource implementations. Palette and device queries are outside
 * this fixture and must fail loudly if accidentally reached. */
#ifndef TSFP_RHI_XBOX_TEXTURES_H
#define TSFP_RHI_XBOX_TEXTURES_H
#include "d3d8_internal.h"
const DWORD *d3d8_GetPalette(DWORD stage) {(void)stage;abort();}
IDirect3DDevice8 *xbox_GetD3DDevice(void) {abort();}

static void xbox_texture_color(unsigned x,unsigned y,unsigned z,BYTE color[4]) {
    color[0]=(BYTE)(32*(z+1));color[1]=(BYTE)(32*(y+1));
    color[2]=(BYTE)(32*(x+1));color[3]=255; /* Xbox ARGB stored BGRA */
}
/* Morton positions for the explicit 4x2x2, 2x1x1 and 1x1x1 fixtures.
 * Fixed expected layouts are independent of the production swizzle helper. */
static unsigned xbox_volume_position(unsigned mip,unsigned x,unsigned y,unsigned z) {
    return mip ? x : ((x&1) | (y<<1) | (z<<2) | ((x&2)<<2));
}
static void xbox_texture_expect(RhiBuffer *queries,const float query[4],const BYTE color[4]) {
    float expected[4]={color[2]/255.0f,color[1]/255.0f,color[0]/255.0f,color[3]/255.0f};
    texture_expect(queries,query,expected);
}
static void rhi_xbox_textures_test(void) {
    RhiBufferDesc bd={16,RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    RhiBuffer *queries=rhi_buffer_create(&bd,NULL);CHECK(queries);
    RhiSamplerDesc sd={0};sd.filter=RHI_FILTER_POINT;sd.max_lod=1000;sd.max_anisotropy=1;
    sd.address_u=sd.address_v=sd.address_w=RHI_ADDRESS_CLAMP;sd.compare=RHI_CMP_ALWAYS;
    RhiSampler *sampler=rhi_sampler_create(&sd);CHECK(sampler);
    RhiShader *cube_ps=shader(RHI_STAGE_PIXEL,
        "TextureCube<float4> t:register(t0);SamplerState s:register(s0);"
        "cbuffer Q:register(b0){float4 q;}float4 main():SV_TARGET{return t.SampleLevel(s,q.xyz,q.w);}","ps_5_0");
    RhiShader *volume_ps=shader(RHI_STAGE_PIXEL,
        "Texture3D<float4> t:register(t0);SamplerState s:register(s0);"
        "cbuffer Q:register(b0){float4 q;}float4 main():SV_TARGET{return t.SampleLevel(s,q.xyz,q.w);}","ps_5_0");
    rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&queries);rhi_set_samplers(0,1,&sampler);
    const float directions[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for(unsigned swizzled=0;swizzled<2;swizzled++) {
        D3DFORMAT format=swizzled?D3DFMT_A8R8G8B8:D3DFMT_LIN_A8R8G8B8;
        IDirect3DCubeTexture8 *cube=NULL;
        CHECK(d3d8_CreateCubeTextureImpl(4,0,0,format,&cube)==S_OK && cube);
        CHECK(cube->lpVtbl->GetLevelCount(cube)==3);
        D3D8CubeInfo info={0};CHECK(d3d8_cube_info((IDirect3DBaseTexture8 *)cube,&info));
        CHECK(info.edge==4 && info.levels==3 && info.format==format);
        for(unsigned face=0;face<6;face++)for(unsigned mip=0;mip<3;mip++) {
            unsigned size=4>>mip;D3DSURFACE_DESC desc;D3DLOCKED_RECT lock;BYTE color[4];
            CHECK(cube->lpVtbl->GetLevelDesc(cube,mip,&desc)==S_OK);
            CHECK(desc.Width==size && desc.Height==size && desc.Format==format);
            CHECK(cube->lpVtbl->LockRect(cube,(D3DCUBEMAP_FACES)face,mip,&lock,NULL,0)==S_OK);
            xbox_texture_color(face,mip,0,color);
            for(unsigned p=0;p<size*size;p++)memcpy((BYTE *)lock.pBits+p*4,color,4);
            CHECK(cube->lpVtbl->UnlockRect(cube,(D3DCUBEMAP_FACES)face,mip)==S_OK);
        }
        RhiView *srv=d3d8_base_srv((IDirect3DBaseTexture8 *)cube);CHECK(srv);
        rhi_set_shader(RHI_STAGE_PIXEL,cube_ps);rhi_set_textures(0,1,&srv);
        for(unsigned face=0;face<6;face++)for(unsigned mip=0;mip<3;mip++) {
            BYTE color[4];xbox_texture_color(face,mip,0,color);
            float q[4]={directions[face][0],directions[face][1],directions[face][2],(float)mip};
            xbox_texture_expect(queries,q,color);
        }
        IDirect3DSurface8 *surface=NULL;
        CHECK(cube->lpVtbl->GetCubeMapSurface(cube,(D3DCUBEMAP_FACES)4,1,&surface)==S_OK);
        RhiView *none=NULL;rhi_set_textures(0,1,&none);
        CHECK(cube->lpVtbl->Release(cube)==0); /* surface retains its GPU image */
        D3DLOCKED_RECT lock;BYTE color[4];xbox_texture_color(4,1,0,color);
        CHECK(surface->lpVtbl->LockRect(surface,&lock,NULL,D3DLOCK_READONLY)==S_OK);
        for(unsigned y=0;y<2;y++)for(unsigned x=0;x<2;x++)
            CHECK(memcmp((BYTE *)lock.pBits+y*lock.Pitch+x*4,color,4)==0);
        CHECK(surface->lpVtbl->UnlockRect(surface)==S_OK);
        CHECK(surface->lpVtbl->Release(surface)==0);

        IDirect3DVolumeTexture8 *volume=NULL;
        CHECK(d3d8_CreateVolumeTextureImpl(4,2,2,0,0,format,&volume)==S_OK && volume);
        CHECK(volume->lpVtbl->GetLevelCount(volume)==3);
        for(unsigned mip=0;mip<3;mip++) {
            unsigned w=4>>mip,h=mip?1:2,d=h;D3DVOLUME_DESC desc;D3DLOCKED_BOX box;
            CHECK(volume->lpVtbl->GetLevelDesc(volume,mip,&desc)==S_OK);
            CHECK(desc.Width==w && desc.Height==h && desc.Depth==d && desc.Format==format);
            CHECK(volume->lpVtbl->LockBox(volume,mip,&box,NULL,0)==S_OK);
            for(unsigned z=0;z<d;z++)for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
                unsigned offset=swizzled?4*xbox_volume_position(mip,x,y,z):z*box.SlicePitch+y*box.RowPitch+x*4;
                xbox_texture_color(x,y,z+2*mip,(BYTE *)box.pBits+offset);
            }
            CHECK(volume->lpVtbl->UnlockBox(volume,mip)==S_OK);
        }
        srv=d3d8_base_srv((IDirect3DBaseTexture8 *)volume);CHECK(srv);
        rhi_set_shader(RHI_STAGE_PIXEL,volume_ps);rhi_set_textures(0,1,&srv);
        for(unsigned mip=0;mip<3;mip++) {
            unsigned w=4>>mip,h=mip?1:2,d=h;
            for(unsigned z=0;z<d;z++)for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
                BYTE color[4];xbox_texture_color(x,y,z+2*mip,color);
                float q[4]={(x+.5f)/w,(y+.5f)/h,(z+.5f)/d,(float)mip};
                xbox_texture_expect(queries,q,color);
            }
        }
        IDirect3DVolume8 *level=NULL;
        CHECK(volume->lpVtbl->GetVolumeLevel(volume,1,&level)==S_OK);
        rhi_set_textures(0,1,&none);
        CHECK(volume->lpVtbl->Release(volume)==1); /* level retains its parent */
        D3DVOLUME_DESC desc;CHECK(level->lpVtbl->GetDesc(level,&desc)==S_OK);
        CHECK(desc.Width==2 && desc.Height==1 && desc.Depth==1);
        CHECK(level->lpVtbl->Release(level)==0);
    }
    RhiBuffer *none=NULL;rhi_set_uniform_buffers(RHI_STAGE_PIXEL,0,1,&none);
    RhiSampler *no_sampler=NULL;rhi_set_samplers(0,1,&no_sampler);
    rhi_set_shader(RHI_STAGE_PIXEL,NULL);rhi_buffer_destroy(queries);rhi_sampler_destroy(sampler);
    rhi_shader_destroy(cube_ps);rhi_shader_destroy(volume_ps);
    puts("PASS: shared Xbox cube/volume factories, linear/swizzled mip uploads, sampling and child lifetime");
}
#endif
