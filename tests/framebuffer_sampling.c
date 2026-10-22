/* Exercise the real HLE framebuffer cache with a deterministic screen copy.
 * No game data, GPU, saves or settings are used. */
#include "platform/xbox_winnt.h"
/* The adapter's diagnostic throttling must not require an OS clock/linker
 * shim in this standalone test. Keep diagnostics deterministic on both hosts. */
static DWORD test_tick_count(void) { return 0; }
#define GetTickCount test_tick_count
#include "hle/hle_d3d8_texture.c"
#undef GetTickCount
#include <assert.h>

static uint8_t memory[4096];
ptrdiff_t g_xbox_mem_offset;
static unsigned long swap;
static unsigned copies;
static int scene, sampled, fail_copy;
static IDirect3DTexture8 fake_texture;
static IDirect3DTexture8 white;
static IDirect3DDevice8 device;
RECOMP_TLS uint32_t g_eax, g_esp;
void (*const hle_original_D3DDevice_SetTexture)(void) = NULL;
void (*const hle_original_D3DDevice_SetPalette)(void) = NULL;

int xbox_EnvSwitch(const char *name, int value) { return value; }
UINT d3d8_format_bpp(D3DFORMAT format) { return 32; }
BOOL d3d8_format_is_compressed(D3DFORMAT format) { return FALSE; }
UINT d3d8_row_pitch(D3DFORMAT format, UINT width) { return width * 4; }
void d3d8_base_set_palette(IDirect3DBaseTexture8 *texture, UINT palette) {}
int hle_d3d8_defer_recording(void) { return 0; }
void hle_d3d8_defer_op(void (*fn)(const void *), const void *arg, size_t size) { assert(0); }
IDirect3DDevice8 *hle_d3d8_shadow_device(void) { return &device; }
int hle_d3d8_is_framebuffer(uint32_t phys) { return phys == 0x204000 || phys == 0x205000; }
void hle_d3d8_note_stage_texels(uint32_t stage, uint32_t phys) {}
int hle_d3d8_trace_on(void) { return 0; }
HRESULT host_SetTexture(IDirect3DDevice8 *dev, DWORD stage, IDirect3DBaseTexture8 *texture) { return S_OK; }
HRESULT host_SetPalette(IDirect3DDevice8 *dev, DWORD stage, const DWORD *entries) { return S_OK; }
HRESULT host_CreateCubeTexture(IDirect3DDevice8 *dev, UINT edge, UINT levels,
    DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DCubeTexture8 **out) { assert(0); return E_FAIL; }
HRESULT host_CubeLockRect(IDirect3DCubeTexture8 *cube, D3DCUBEMAP_FACES face,
    UINT level, D3DLOCKED_RECT *locked) { assert(0); return E_FAIL; }
HRESULT host_CubeUnlockRect(IDirect3DCubeTexture8 *cube, D3DCUBEMAP_FACES face, UINT level) { assert(0); return E_FAIL; }
HRESULT host_LockRect(IDirect3DTexture8 *texture, UINT level, D3DLOCKED_RECT *locked,
    const RECT *rect, DWORD flags) {
    static uint32_t pixel;
    assert(texture == &white); locked->Pitch = 4; locked->pBits = &pixel; return S_OK;
}
HRESULT host_UnlockRect(IDirect3DTexture8 *texture, UINT level) { return S_OK; }
/* This fixture samples the screen cache, never a precision-changing surface. */
HRESULT d3d8_PreserveFeedbackTexture(IDirect3DTexture8 *dst, IDirect3DTexture8 *src)
{ assert(0); return E_FAIL; }

unsigned long hle_d3d8_shadow_swaps(void) { return swap; }
HRESULT host_CreateTexture(IDirect3DDevice8 *dev, UINT w, UINT h, UINT levels,
                          DWORD usage, D3DFORMAT format, D3DPOOL pool,
                          IDirect3DTexture8 **out)
{
    *out = w == 1 ? &white : &fake_texture;
    return S_OK;
}
ULONG host_ReleaseTexture(IDirect3DTexture8 *texture) { return 0; }
HRESULT host_CopyBackBufferToTexture(IDirect3DTexture8 *texture)
{
    copies++;
    if (fail_copy) return E_FAIL;
    sampled = scene;
    return S_OK;
}

static void bind_screen(unsigned stage, uint32_t va)
{
    shadow_set_texture(stage, va);
    assert(g_bound[stage] == &fake_texture);
}

int main(void)
{
    const uint32_t va = 128;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    HLE_MEM32(va + 4) = 0x204000;
    HLE_MEM32(va) = COMMON_TYPE_TEXTURE;
    HLE_MEM32(va + 12) = (D3DFMT_LIN_X8R8G8B8 << 8) | 0x20;
    HLE_MEM32(va + 16) = 639 | (479 << 12) | (39 << 24);

    /* First use at swap zero must copy, not mistake a zeroed cache for valid. */
    scene = 10;
    bind_screen(0, va);
    assert(copies == 1 && sampled == 10);
    /* Repeated bindings retain the original image for adjacent blur taps. */
    scene = 11;
    bind_screen(0, va);
    assert(copies == 1 && sampled == 10);

    /* Background blur -> opaque train/weapon -> glow, within one frame. */
    shadow_set_texture(0, 0);
    scene = 20;
    bind_screen(0, va);
    assert(copies == 2 && sampled == 20);
    scene = 21;
    bind_screen(0, va);
    assert(copies == 2 && sampled == 20);

    swap++;
    bind_screen(0, va);
    assert(copies == 3 && sampled == 21);
    /* NULL/unbind and a nonzero sampler stage also invalidate the old copy. */
    g_bound_entry[2] = &g_textures[0];
    shadow_set_texture(2, 0);
    scene = 30;
    bind_screen(2, va);
    assert(copies == 4 && sampled == 30);
    /* Repointing the same guest texture must not reuse another image. */
    HLE_MEM32(va + 4) += 0x1000;
    scene = 40;
    bind_screen(2, va);
    assert(copies == 5 && sampled == 40);
    /* A failed copy stays invalid so the next bind retries. */
    shadow_set_texture(2, 0);
    fail_copy = 1;
    bind_screen(2, va);
    assert(copies == 6 && !g_textures[0].framebuffer_copy_valid);
    fail_copy = 0; scene = 50;
    bind_screen(2, va);
    assert(copies == 7 && sampled == 50);
    puts("PASS: framebuffer sampling passes, stable taps, frame zero and retry");
    return 0;
}
