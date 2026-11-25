/* Exercise the real HLE framebuffer cache with a deterministic screen copy.
 * No game data, GPU, saves or settings are used. */
#include "platform/xbox_winnt.h"
/* The adapter's diagnostic throttling must not require an OS clock/linker
 * shim in this standalone test. Keep diagnostics deterministic on both hosts. */
static DWORD test_tick_count(void) { return 0; }
#define GetTickCount test_tick_count
#include "hle/hle_d3d8_texture.c"
#include "framebuffer_draw_coverage.h"
#undef GetTickCount
#include <assert.h>
#include "framebuffer_surface_stubs.h"

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
static int recording;
static struct { void (*fn)(const void *); unsigned char args[64]; } queue[32];
static unsigned queued;
int hle_d3d8_defer_recording(void) { return recording; }
void hle_d3d8_defer_op(void (*fn)(const void *), const void *arg, size_t size)
{
    assert(queued<32 && size<=64);
    queue[queued].fn=fn;memcpy(queue[queued++].args,arg,size);
}
static void drain(void)
{
    recording=0;
    for(unsigned i=0;i<queued;i++)queue[i].fn(queue[i].args);
    queued=0;
}
void hle_d3d8_defer_flush(void) { drain(); }
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

static void paint_half(const void *unused) { scene=sampled/2; }
static void record_tint(void)
{
    float quad[4][2]={{0,0},{640,0},{0,480},{640,480}};
    hle_d3d8_framebuffer_prepare_draw(draw_covers_target(quad,sizeof quad[0],4));
    unsigned dummy=0;hle_d3d8_defer_op(paint_half,&dummy,sizeof dummy);
    hle_d3d8_framebuffer_drawn();
}

/* Cache aliases may disagree about framebuffer status. Compare both outputs
 * against the original two independent searches, including a full cache. */
static void check_bound_lookup(void)
{
    IDirect3DTexture8 hosts[3];
    texture_entry saved[TEXTURE_CACHE];
    unsigned state = 17;
    for (int count = 0; count <= TEXTURE_CACHE; count++) {
        g_texture_count = count;
        for (int i = 0; i < count; i++) {
            state = state * 1664525u + 1013904223u;
            g_textures[i].host = &hosts[(state >> 16) % 3];
            g_textures[i].framebuffer = (state >> 29) == 0;
        }
        memcpy(saved, g_textures, sizeof(saved));
        for (int target = -1; target < 4; target++) {
            IDirect3DTexture8 *host = target < 0 ? NULL :
                target == 3 ? &white : &hosts[target];
            texture_entry *expected = NULL;
            int expected_fb = 0, actual_fb = -1;
            for (int i = 0; host && i < count; i++) {
                if (g_textures[i].host == host) {
                    expected = &g_textures[i];
                    break;
                }
            }
            for (int i = 0; host && i < count; i++) {
                if (g_textures[i].host == host && g_textures[i].framebuffer) {
                    expected_fb = 1;
                    break;
                }
            }
            assert(bound_host_entry(host, &actual_fb) == expected);
            assert(actual_fb == expected_fb);
            assert(bound_host_entry(host, NULL) == expected);
            assert(memcmp(saved, g_textures, sizeof(saved)) == 0);
        }
    }
    memset(g_textures, 0, sizeof(g_textures));
    g_texture_count = 0;
}

int main(void)
{
    const uint32_t va = 128;
    check_bound_lookup();
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
    /* Offscreen copies may use an existing GPU image only while its guest
     * descriptor still names the same allocation and layout. */
    assert(hle_d3d8_rendered_texture(va)==g_textures[0].host);
    g_textures[0].rendered=0;assert(!hle_d3d8_rendered_texture(va));
    g_textures[0].rendered=1;
    HLE_MEM32(va+4)++;assert(!hle_d3d8_rendered_texture(va));HLE_MEM32(va+4)--;
    HLE_MEM32(va+12)++;assert(!hle_d3d8_rendered_texture(va));HLE_MEM32(va+12)--;
    HLE_MEM32(va+16)++;assert(!hle_d3d8_rendered_texture(va));HLE_MEM32(va+16)--;
    assert(hle_d3d8_rendered_texture(va)==g_textures[0].host);
    /* Whole-frame tint passes see preceding output even without a rebind. */
    g_fb_target=0x205000;g_bound_entry[0]=g_bound_entry[2];
    scene=100;hle_d3d8_framebuffer_drawn();unsigned before=copies;
    hle_d3d8_framebuffer_refresh_bound();
    assert(copies==before+1 && sampled==100);
    scene=50;hle_d3d8_framebuffer_drawn();
    hle_d3d8_framebuffer_refresh_bound();
    assert(copies==before+2 && sampled==50);
    /* The same entry bound twice still needs only one refresh per epoch. */
    hle_d3d8_framebuffer_refresh_bound();assert(copies==before+2);
    /* Switching buffers preserves the completed image from the old buffer. */
    scene=75;hle_d3d8_framebuffer_target(0x204000);before=copies;
    assert(g_textures[0].held && sampled==75);
    scene=5;shadow_set_texture(2,0);bind_screen(2,va);
    hle_d3d8_framebuffer_drawn();hle_d3d8_framebuffer_refresh_bound();
    assert(copies==before && sampled==75);
    hle_d3d8_framebuffer_target(0x205000);
    assert(!g_textures[0].held);
    fail_copy=1;unsigned long epoch=g_textures[0].framebuffer_epoch;
    hle_d3d8_framebuffer_refresh_bound();
    assert(g_textures[0].framebuffer_epoch==epoch);
    fail_copy=0;hle_d3d8_framebuffer_refresh_bound();
    assert(sampled==5 && g_textures[0].framebuffer_epoch==g_fb_epoch);
    /* Bindings start unbound when recording; replay must sample each preceding
     * tint output, not the previous frame's binding or one initial snapshot. */
    shadow_set_texture(0,0);shadow_set_texture(2,0);
    scene=100;before=copies;g_fb_target=0x204000;
    unsigned long prior_epoch=g_fb_epoch;uint32_t prior_target=g_fb_target;
    recording=1;
    hle_d3d8_framebuffer_target(0x205000);
    uint32_t bind[2]={0,va};hle_d3d8_defer_op(op_set_texture,bind,sizeof bind);
    record_tint();record_tint();
    assert(copies==before && g_fb_epoch==prior_epoch && g_fb_target==prior_target);
    drain();
    assert(scene==25 && sampled==50 && copies==before+2);
    assert(g_fb_epoch==prior_epoch+2);
    assert(g_fb_target==0x205000);
    /* Partial glow taps deliberately retain the bind snapshot. */
    float partial[4][2]={{0,0},{320,0},{0,240},{320,240}};
    assert(!draw_covers_target(partial,sizeof partial[0],4));
    puts("PASS: framebuffer sampling passes, stable taps, frame zero and retry");
    return 0;
}
