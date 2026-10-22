"""Execute the production guest wrapper with isolated camera and stack memory."""
import re

import pytest

from test_cutscene_letterbox import ROOT, RUNTIME, compile_and_run


@pytest.mark.parametrize('camera_available', [True, False])
@pytest.mark.parametrize('platform_define', ['TSFP_PC_CONTROLS', 'TSFP_NATIVE_DISPLAY'])
@pytest.mark.parametrize('mutation', ['none', 'retained-snapshot'])
def test_decal_projection(tmp_path, camera_available, platform_define, mutation):
    manual = (RUNTIME / 'titles/futureperfect/src/recomp_manual.c').read_text()
    wrapper = re.search(r'void sub_00151CF0\(void\)\n\{.*?\n\}', manual, re.S)
    assert wrapper, 'Apply the complete patch stack before testing'
    camera = (ROOT / 'src/pc_display_menu.c').read_text()
    projection = re.search(r'void pc_display_perspective_projection\(.*?\n\}', camera, re.S)
    assert projection, 'Production perspective projection function missing'
    function = projection.group()
    if mutation == 'retained-snapshot':
        assert 'PcGuestRegion region={0,0,0};' in function
        function = function.replace('PcGuestRegion region={0,0,0};', 'static PcGuestRegion region={0,0,0};')
    source = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PLATFORM_DEFINE
static unsigned char memory[0x800000];
static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp, g_eax;
#include "display_memory_mock.h"
static float factor=1.f;
static float pc_display_camera_factor(void) { return factor; }
#define WORD_AT(a) (*(uint32_t *)(memory+(uint32_t)(a)))
static int guest_writable(uint32_t a,size_t n)
{ return a && a<sizeof memory && n<=sizeof memory-a; }
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL: %s\n",#x); exit(1); } } while(0)
static float original[16];
void sub_00151CF0_gen(void)
{
    memcpy(memory+WORD_AT(g_esp+4),original,sizeof original);
    g_eax=0x12345678;g_esp+=4;
}
/* FUNCTION */
/* WRAPPER */
int main(void)
{
    const uint32_t callers[]={0x112F7F,0x115B0C,0xC7832,0x2DA21F,0x2E4B5B};
    const float apertures[][2]={{1.44f,1.92f},{1.08f,1.92f},{.8f,1.92f},{1.44f,2.56f}};
    size_t i,j,k;
    g_xbox_mem_offset=(uintptr_t)memory;
    WORD_AT(0x7B0CC0)=CAMERA_AVAILABLE ? 0x4000 : 0;
    WORD_AT(0x4204)=0x5000;
    for(k=0;k<16;k++)original[k]=(float)k;
    original[0]=1.44f;original[5]=1.92f;original[10]=-1.00005555f;original[14]=-.10050558f;
    for(i=0;i<sizeof apertures/sizeof apertures[0];i++) {
        factor=1.f+(float)i*.25f;
        float *active=(float *)(memory+0x5000);
        active[0]=apertures[i][0];active[5]=apertures[i][1];
        for(j=0;j<sizeof callers/sizeof callers[0];j++) {
            float *decal=(float *)(memory+0x6000);
            g_esp=0x10000;WORD_AT(g_esp)=callers[j];WORD_AT(g_esp+4)=0x6000;
            unsigned before=display_region_queries;
            sub_00151CF0();
            CHECK(display_region_queries-before==(CAMERA_AVAILABLE && j<2 ? 1u : 0u));
            CHECK(g_esp==0x10004 && g_eax==0x12345678);
            for(k=0;k<16;k++) {
                float expected=CAMERA_AVAILABLE && j<2 && (k==0 || k==5) ? active[k] : original[k];
                CHECK(decal[k]==expected);
            }
        }
    }
    /* Both preview and weapon builders retain their own lens and depth.
       Rebuild alternating per-frame buffers repeatedly, as the character
       picker does while models load; never reuse a corrected matrix as input.
       Cover original/4:3, 16:9, ultrawide and narrower-than-4:3 outputs. */
    {
        const float factors[]={1.f,4.f/3.f,1.75f,8.f/3.f,15.f/16.f};
        const uint32_t models[]={0x1B7482,0x2C46EF};
        for(i=0;i<sizeof factors/sizeof factors[0];i++) {
            factor=factors[i];
            for(j=0;j<2;j++)for(size_t frame=0;frame<48;frame++) {
                uint32_t destination=0x6000+(frame%2)*0x100;
                float *model=(float *)(memory+destination);
                g_esp=0x10000;WORD_AT(g_esp)=models[j];WORD_AT(g_esp+4)=destination;
                unsigned before=display_region_queries;
                sub_00151CF0();
                CHECK(display_region_queries-before==1);
                CHECK(g_esp==0x10004 && g_eax==0x12345678);
                for(k=0;k<16;k++)CHECK(model[k]==(k==0?original[k]/factor:original[k]));
            }
        }
    }
    puts("PASS: decal aperture, preview/weapon proportions, repeated builds, other callers and ABI preserved");
    return 0;
}
'''
    source = source.replace('/* FUNCTION */', function).replace('/* WRAPPER */', wrapper.group())
    source = source.replace('CAMERA_AVAILABLE', str(int(camera_available)))
    source = source.replace('PLATFORM_DEFINE', platform_define)
    compile_and_run(tmp_path, source, fails=mutation!='none')


def test_display_memory_protection(tmp_path):
    compile_and_run(tmp_path, (ROOT/'tests/display_memory.c').read_text())
