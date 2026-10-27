/*
 * d3d8_final_default - an unprogrammed final combiner passes r0 through.
 *
 * A pixel shader definition that never programs the final combiner leaves
 * PSFINALCOMBINERINPUTSABCD and ...EFG zero. Read literally that is every
 * input ZERO: out = 0 + 0*0 + (1-0)*0, black with alpha 0, and TimeSplitters 2's
 * whole front end -- one stage writing r0, final combiner untouched -- drew
 * as a black frame. The console's D3D gives that case the assembler's default
 * final combiner, `xfc r0.a, zero, zero, zero, zero, zero, r0`: colour D = r0,
 * alpha G = r0.a. The parser must synthesise the same, and must leave a final
 * combiner the title did program alone.
 *
 * Pure parsing; no device is created.
 */
#include "d3d8_internal.h"
#include <d3d11.h>   /* this test drives D3D11 itself */
#include <stdio.h>
#include <string.h>

/* The combiner sources link against the device layer for state this test
 * never touches; parsing needs no device. Same stubs as test_a8.c. */
static DWORD states[512], stages[4][64];
static const D3DMATRIX identity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
ID3D11Device *d3d8_GetD3D11Device(void) { return NULL; }
ID3D11DeviceContext *d3d8_GetD3D11Context(void) { return NULL; }
ID3D11RenderTargetView *d3d8_GetDefaultRTV(void) { return NULL; }
IDirect3DDevice8 *xbox_GetD3DDevice(void) { return NULL; }
const DWORD *d3d8_GetPalette(DWORD stage) { (void)stage; return states; }
const DWORD *d3d8_GetRenderStates(void) { return states; }
const DWORD *d3d8_GetTSS(DWORD stage) { return stages[stage]; }
IDirect3DBaseTexture8 *d3d8_GetStageTexture(DWORD stage) { (void)stage; return NULL; }
/* The size the guest's pre-transformed geometry is measured in; the host
 * back buffer's size is the fallback, and here they are the same. */
UINT d3d8_GetGuestWidth(void) { return 1; }
UINT d3d8_GetGuestHeight(void) { return 1; }
UINT d3d8_GetBackbufferWidth(void) { return 1; }
UINT d3d8_GetBackbufferHeight(void) { return 1; }
const D3DMATRIX *d3d8_GetTransform(D3DTRANSFORMSTATETYPE type) { (void)type; return &identity; }
const D3DLIGHT8 *d3d8_GetLight(DWORD index) { (void)index; return NULL; }
BOOL d3d8_GetLightEnable(DWORD index) { (void)index; return FALSE; }
const D3DMATERIAL8 *d3d8_GetMaterial(void) { return NULL; }
UINT d3d8_GetNumLights(void) { return 0; }
BOOL d3d8_vsh_prepare_draw(DWORD handle) { (void)handle; return FALSE; }
int  d3d8_vsh_bound_uses_projection(void) { return 1; }
int  d3d8_vsh_bound_projection_is_ortho(void) { return 0; }
DWORD d3d8_GetCurrentFVF(void) { return 0; }
void d3d8_SetTwoDSqueeze(BOOL on) { (void)on; }

static int failures;
#define CHECK(name, cond) \
    do { if (cond) {} else { printf("FAIL: %s\n", name); failures++; } } while (0)

int main(void)
{
    DWORD rs[512];
    NV2ACombinerState state;

    /* One stage, r0 = v0 * t0 (TimeSplitters 2's front-end shader): count 1,
     * final combiner words zero. */
    memset(rs, 0, sizeof rs);
    memset(&state, 0, sizeof state);
    rs[D3DRS_PSCOMBINERCOUNT] = 0x00011101u;
    rs[D3DRS_PSRGBINPUTS0] = 0xC4C80000u;
    rs[D3DRS_PSRGBOUTPUTS0] = 0x000100C0u;
    d3d8_combiners_parse_token(1u, rs, &state);

    CHECK("unprogrammed final combiner: D is r0", state.final_input[3].reg == NV2A_REG_R0);
    CHECK("unprogrammed final combiner: D is colour, not alpha", state.final_input[3].alpha_rep == 0);
    CHECK("unprogrammed final combiner: G is r0", state.final_input[6].reg == NV2A_REG_R0);
    CHECK("unprogrammed final combiner: G is the alpha channel", state.final_input[6].alpha_rep == 1);
    CHECK("unprogrammed final combiner: A stays zero", state.final_input[0].reg == NV2A_REG_ZERO);
    CHECK("unprogrammed final combiner: E stays zero", state.final_input[4].reg == NV2A_REG_ZERO);

    /* Burnout 2's fog lerp: A = fog alpha, B = r0, C = fog colour, D = zero;
     * G = r0 alpha with CLAMP_SUM. Programmed, so read as written. */
    memset(&state, 0, sizeof state);
    rs[D3DRS_PSFINALCOMBINERINPUTSABCD] = 0x130C0300u;
    rs[D3DRS_PSFINALCOMBINERINPUTSEFG] = 0x00001C80u;
    d3d8_combiners_parse_token(1u, rs, &state);

    CHECK("programmed final combiner: A is fog alpha",
          state.final_input[0].reg == NV2A_REG_FOG && state.final_input[0].alpha_rep == 1);
    CHECK("programmed final combiner: B is r0", state.final_input[1].reg == NV2A_REG_R0);
    CHECK("programmed final combiner: C is fog", state.final_input[2].reg == NV2A_REG_FOG);
    CHECK("programmed final combiner: D is zero, as written", state.final_input[3].reg == NV2A_REG_ZERO);
    CHECK("programmed final combiner: G is r0 alpha",
          state.final_input[6].reg == NV2A_REG_R0 && state.final_input[6].alpha_rep == 1);

    /* A title that wants black writes ZERO into D explicitly, with something
     * else non-zero: that is a programmed combiner and must not be rewritten. */
    memset(&state, 0, sizeof state);
    rs[D3DRS_PSFINALCOMBINERINPUTSABCD] = 0x00000000u;
    rs[D3DRS_PSFINALCOMBINERINPUTSEFG] = 0x00001C00u;      /* only G = r0.a */
    d3d8_combiners_parse_token(1u, rs, &state);
    CHECK("explicit zero colour with programmed alpha keeps D zero",
          state.final_input[3].reg == NV2A_REG_ZERO);

    if (failures == 0)
        printf("d3d8_final_default: all checks passed\n");
    return failures ? 1 : 0;
}
