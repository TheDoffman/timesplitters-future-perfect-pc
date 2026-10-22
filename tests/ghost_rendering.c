#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "platform/xbox_winnt.h"
/* Only diagnostic throttling uses time here. Keep the standalone harness
 * independent of the OS clock and the macOS compatibility library. */
static DWORD test_tick_count(void) { return 0; }
#define GetTickCount test_tick_count
#include "hle/hle_d3d8_state.c"
#undef GetTickCount

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint32_t memory[8192];
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp;
const uint32_t hle_var_D3D_g_RenderState=256;
const uint32_t hle_var_D3D_g_DeferredRenderState=256+92*4;
const uint32_t hle_var_D3D_g_ComplexRenderState=256+136*4;
const uint32_t hle_var_D3D_g_DeferredTextureState=2048;
void (*const hle_original_D3DDevice_SetRenderState_Simple)(void)=NULL;
static DWORD observed[512];
static IDirect3DDevice8 device;
HRESULT host_SetRenderState(IDirect3DDevice8 *d,D3DRENDERSTATETYPE s,DWORD v)
{ CHECK((unsigned)s<512); observed[s]=v; return S_OK; }
HRESULT host_SetTextureStageState(IDirect3DDevice8 *d,DWORD stage,D3DTEXTURESTAGESTATETYPE s,DWORD v)
{ return S_OK; }
void host_combiners_set_pixel_shader(DWORD token) { }
void host_combiners_set_float_constant(unsigned bank,unsigned stage,const float *value)
{ CHECK(bank==1 && stage==6 && value==NULL); }

#define RECOMP_MANUAL_TLS RECOMP_TLS
#include "pc_ghost_render_guest.inc"
static unsigned expected_flags, calls;
static void draw(void)
{
    hle_d3d8_shadow_apply_states(&device);
    CHECK(observed[D3DRS_CULLMODE] == ((expected_flags&1) ? D3DCULL_CW : D3DCULL_NONE));
    CHECK(observed[D3DRS_FOGENABLE] == !(expected_flags&2));
    CHECK(observed[D3DRS_ZWRITEENABLE]==0);
    CHECK(observed[D3DRS_ALPHABLENDENABLE]==1);
    ++calls; g_eax=0xAABBCCDD; g_esp+=4;
}
static void setup(uint32_t caller)
{ g_esp=4096; memory[g_esp/4]=caller; expected_flags=0; }
static void begin_eye(void)
{
    CHECK(memory[(g_esp+24)/4]==0);
    draw(); /* Begin must flush the OLD batch before disabling fog. */
}
static void exercise_effect(void)
{
    unsigned type;
    /* The head is rigid; every skinned node class remains two sided. */
    for(type=0;type<4;type++) {
        setup(0); memory[(g_esp+4)/4]=5000;memory[(g_esp+8)/4]=0;
        memory[5004/4]=6000;memory[6000/4]=1;
        ((uint8_t *)memory)[6000-0xC8+0x48]=(uint8_t)type;
        expected_flags=type==0 ? 1:0;
        pc_ghost_model_node(draw);
        CHECK(g_eax==0xAABBCCDD && g_esp==4100);
        expected_flags=0; draw(); /* Unchanged guest state must be restored. */
    }
    setup(0x24A5DC);memory[(g_esp+24)/4]=0x43480000;
    pc_ghost_billboard_begin(begin_eye);
    CHECK(memory[(4096+24)/4]==0x43480000);
    CHECK(g_eax==0xAABBCCDD && g_esp==4100);
    expected_flags=2; draw(); pc_ghost_billboard_end(draw);
    expected_flags=0; draw();
    /* The second call site draws the eye trails. */
    setup(0x24AA5B);memory[(g_esp+24)/4]=0x43480000;
    pc_ghost_billboard_begin(begin_eye);expected_flags=2;draw();
    /* Deliberately omit End to verify renderer-exit cleanup. */
}
static void ordinary_begin(void)
{ CHECK(memory[(g_esp+24)/4]==0x43480000);draw(); }
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    memory[(256+92*4)/4]=1;memory[(256+59*4)/4]=1;
    g_ps_def_ok=1;g_ps_def=0; /* No pixel shader for this state-boundary test. */
    setup(0);draw();
    pc_ghost_render(exercise_effect);
    expected_flags=0;draw();
    CHECK(spirit_render_depth==0 && !spirit_billboard_active);
    setup(0x24A5DC);memory[(g_esp+24)/4]=0x43480000;
    pc_ghost_billboard_begin(ordinary_begin); /* Same PC outside spirit renderer. */
    spirit_render_depth=1;
    setup(0x123456);memory[(g_esp+24)/4]=0x43480000;
    pc_ghost_billboard_begin(ordinary_begin); /* Unrelated billboard caller. */
    spirit_render_depth=0;
    setup(0);memory[(g_esp+4)/4]=5000;memory[(g_esp+8)/4]=0;
    ((uint8_t *)memory)[6000-0xC8+0x48]=0;
    pc_ghost_model_node(draw); /* Rigid geometry outside spirit renderer. */
    CHECK(calls>=17);
    puts("PASS: spirit node/eye scope, batch ordering, ABI and state restoration");
    return 0;
}
