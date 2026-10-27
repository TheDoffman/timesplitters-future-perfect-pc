/* Full common module: changing guest state, layout, memory base and effects. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "platform/xbox_winnt.h"
static DWORD test_tick_count(void) { return 0; }
#define GetTickCount test_tick_count
#include "hle/hle_d3d8_state.c"
#undef GetTickCount
static uint32_t memory[2][8192];
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp;
const uint32_t hle_var_D3D_g_RenderState=256;
const uint32_t hle_var_D3D_g_DeferredRenderState=256+92*4;
const uint32_t hle_var_D3D_g_ComplexRenderState=256+136*4;
const uint32_t hle_var_D3D_g_DeferredTextureState=2048;
void (*const hle_original_D3DDevice_SetRenderState_Simple)(void)=NULL;
static unsigned long long digest=1469598103934665603ull;
static unsigned calls,active_bank,feedback;
static void record(unsigned kind,unsigned stage,unsigned state,unsigned value)
{
    digest=(digest^kind)*1099511628211ull;
    digest=(digest^stage)*1099511628211ull;
    digest=(digest^state)*1099511628211ull;
    digest=(digest^value)*1099511628211ull;
    ++calls;
    /* Later states must still be loaded after earlier host calls. */
    if(feedback)memory[active_bank][256/4+148]^=0x01010101u;
}
HRESULT host_SetRenderState(IDirect3DDevice8 *d,D3DRENDERSTATETYPE s,DWORD v)
{ (void)d;record(1,0,s,v);return S_OK; }
HRESULT host_SetTextureStageState(IDirect3DDevice8 *d,DWORD stage,D3DTEXTURESTAGESTATETYPE s,DWORD v)
{ (void)d;record(2,stage,s,v);return S_OK; }
void host_combiners_set_pixel_shader(DWORD token) {record(3,0,0,token);}
void host_combiners_set_float_constant(unsigned bank,unsigned stage,const float *value)
{record(4,bank,stage,value!=NULL);}
static uint32_t rng=0x12345678u;
static uint32_t next_value(void) {rng=rng*1664525u+1013904223u;return rng;}
int main(void)
{
    IDirect3DDevice8 device;
    memset(&device,0,sizeof device);
    g_state_ready=1;g_ps_def_ok=1;g_ps_def=0;
    /* Resetting readiness is controlled: all three recognized layouts must use
     * their original mapping, including absent render-state slots returning0. */
    for(unsigned round=0;round<60;round++) {
        active_bank=round%2;
        g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory[active_bank];
        g_layout=(enum rs_layout)(round%3);
        g_prev_valid=0;
        for(unsigned i=0;i<8192;i++)memory[active_bank][i]=next_value();
        for(unsigned draw=0;draw<64;draw++) {
            /* A mixture of unchanged, one-field and full-state writes. */
            if(draw%3==0)for(unsigned i=0;i<256;i++)memory[active_bank][256/4+i]=next_value();
            if(draw%5==0)for(unsigned i=0;i<4*STAGE_SIZE;i++)memory[active_bank][2048/4+i]=next_value();
            memory[active_bank][256/4+92]^=draw;
            feedback=draw%7==0;
            if(draw%11==0)hle_d3d8_effect_override((draw/11)%4);
            hle_d3d8_shadow_apply_states(&device);
            hle_d3d8_shadow_apply_states(NULL);
        }
    }
    g_state_ready=0;hle_d3d8_shadow_apply_states(&device);
    printf("%llu %u\n",digest,calls);
    return 0;
}
