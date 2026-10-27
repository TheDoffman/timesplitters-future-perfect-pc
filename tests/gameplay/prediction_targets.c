/* Run the actual replay operations; count all writes to shared GPU images. */
#include <stdio.h>
#include <stdlib.h>
#include "prediction_targets_impl.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned writes[6];
HRESULT host_Clear(IDirect3DDevice8 *dev, DWORD count, const D3DRECT *rects,
                   DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{ ++writes[0]; return S_OK; }
HRESULT host_DrawPrimitiveUP(IDirect3DDevice8 *dev, D3DPRIMITIVETYPE type,
                             UINT prims, const void *vertices, UINT stride)
{ ++writes[1]; return S_OK; }
HRESULT host_DrawIndexedPrimitiveUP(IDirect3DDevice8 *dev, D3DPRIMITIVETYPE type,
    UINT min_index, UINT num_vertices, UINT prims, const void *indices,
    D3DFORMAT format, const void *vertices, UINT stride)
{ ++writes[2]; return S_OK; }
HRESULT host_CopyBackBufferToTexture(IDirect3DTexture8 *dst)
{ ++writes[3]; return S_OK; }
HRESULT host_CopyBackBufferRectToTexture(IDirect3DTexture8 *dst, const RECT *rect, const POINT *at)
{ ++writes[4]; return S_OK; }
HRESULT host_CopyTextureRegionScaled(IDirect3DTexture8 *dst, IDirect3DTexture8 *src, const RECT *rect)
{ ++writes[5]; return S_OK; }
static void replay(unsigned draws, unsigned copies)
{
    unsigned before[5];
    union { uint64_t align; unsigned char bytes[512]; } data = {0};
    memcpy(before, writes, sizeof writes);
    op_clear(data.bytes); op_draw_up(data.bytes); op_draw_indexed_up(data.bytes);
    op_screen_copy(data.bytes);
    ((dq_screen_copy *)data.bytes)->has_rect = 1; op_screen_copy(data.bytes);
    ((dq_screen_copy *)data.bytes)->has_rect = 2; op_screen_copy(data.bytes);
    for (unsigned i=0; i<6; ++i) CHECK(writes[i] == before[i] + (i<3 ? draws : copies));
}
int main(void)
{
    IDirect3DSurface8 main_depth={0}, effect_depth={0};
    g_device_depth=&main_depth;
    for (int predicting=0; predicting<=1; ++predicting) {
        hle_d3d8_interp_predicting=predicting;
        /* Main scene writes use the isolated prediction image and depth. */
        g_target_texture=NULL; g_target_depth=NULL; g_target_lost=0;
        replay(1, !predicting);
        g_target_depth=&main_depth; replay(1, !predicting);
        /* Effects stay at their last real pose until the guest updates them. */
        g_target_texture=(IDirect3DBaseTexture8 *)&effect_depth;
        replay(!predicting, !predicting);
        g_target_texture=NULL; g_target_depth=&effect_depth;
        replay(!predicting, !predicting);
        g_target_depth=NULL; g_target_lost=1;
        replay(!predicting, !predicting);
        g_target_lost=0; replay(1, !predicting);
    }
    puts("PASS: prediction target preservation, main scene continuation and legacy writes");
    return 0;
}
