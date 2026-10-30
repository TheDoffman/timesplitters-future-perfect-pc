/* Exercise production HLE clear delivery with fake host depth images. */
#include <stdio.h>
#include <stdlib.h>
#include "terminal_depth_impl.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while (0)

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp;
static uint32_t memory[32];
static IDirect3DSurface8 images[8];
static unsigned created, clears;
static DWORD last_flags, last_stencil;
static float last_z;
static int fail_clear;
static void original_clear(void) { }
void (*const hle_original_D3DDevice_Clear)(void)=original_clear;
HRESULT host_CreateDepthStencilSurface(IDirect3DDevice8 *dev, UINT w, UINT h,
                                       D3DFORMAT format, IDirect3DSurface8 **out)
{
    CHECK(created<8); *out=&images[created++]; return S_OK;
}
HRESULT host_Clear(IDirect3DDevice8 *dev,DWORD count,const D3DRECT *rects,
                   DWORD flags,D3DCOLOR color,float z,DWORD stencil)
{
    if (fail_clear) return E_FAIL;
    ++clears; last_flags=flags;last_z=z;last_stencil=stencil;return S_OK;
}
static void guest_clear(DWORD flags,float z,DWORD stencil)
{
    /* Xbox uses bits 0/1 for Z/stencil (host uses 1/2). */
    memory[3]=flags;memory[4]=0;memcpy(&memory[5],&z,4);memory[6]=stencil;
    hle_D3DDevice_Clear();
}
int main(void)
{
    HleDepthKey scene={0x330000,0x2e00,0x1234}, other={0x440000,0x2e00,0x1234};
    IDirect3DDevice8 device={0};
    IDirect3DSurface8 *camera,*small,*unrelated;
    unsigned before;
    g_xbox_mem_offset=(ptrdiff_t)memory;g_esp=0;g_shadow=&device;
    g_depth_key=scene;g_depth_key_valid=1;
    /* Clear the guest image BEFORE allocating its camera-sized host copy. */
    guest_clear(3,0.75f,7);
    camera=depth_surface(256,256,scene);g_bound_depth=camera;
    shadow_depth_sync(camera);
    CHECK(last_flags==6 && last_z==0.75f && last_stencil==7);
    before=clears;shadow_depth_sync(camera);CHECK(clears==before);
    CHECK(depth_surface(256,256,scene)==camera && created==1);
    unrelated=depth_surface(256,256,other);CHECK(unrelated!=camera);
    shadow_depth_sync(unrelated);CHECK(last_z==1.0f && last_stencil==0);
    small=depth_surface(128,128,scene);shadow_depth_sync(small);
    CHECK(last_z==0.75f && last_stencil==7);
    /* A real clear on the main target must also reach existing replicas. */
    g_bound_depth=NULL;guest_clear(1,1.0f,99);
    before=clears;shadow_depth_sync(unrelated);CHECK(clears==before);
    shadow_depth_sync(camera);CHECK(last_flags==2 && last_z==1.0f);
    shadow_depth_sync(small);CHECK(last_flags==2 && last_z==1.0f);
    /* Colour-only clears and repeated binds preserve camera occlusion. */
    before=clears;guest_clear(0xf0,0.0f,0);CHECK(clears==before+1);
    shadow_depth_sync(camera);CHECK(clears==before+1);
    /* Stencil and Z are independent, and the bound replica was already cleared. */
    g_bound_depth=camera;guest_clear(2,0.0f,3);before=clears;
    shadow_depth_sync(camera);CHECK(clears==before);
    shadow_depth_sync(small);CHECK(last_flags==4 && last_stencil==3);
    /* Failed host delivery remains pending for the next successful bind. */
    g_bound_depth=NULL;guest_clear(1,0.5f,0);before=clears;
    fail_clear=1;shadow_depth_sync(camera);CHECK(clears==before);
    fail_clear=0;shadow_depth_sync(camera);CHECK(clears==before+1 && last_z==0.5f);
    /* History recycling cannot mistake an old generation for the new image. */
    for(unsigned i=0;i<40;i++) {
        HleDepthKey key={0x500000+i*4096,0x2e00,0x1234};
        hle_depth_clear_find(&g_depth_clears,key);
    }
    before=clears;shadow_depth_sync(camera);CHECK(clears==before+1 && last_flags==6);
    puts("PASS: terminal depth clear propagation, identity, independent channels and retained occlusion");
    return 0;
}
