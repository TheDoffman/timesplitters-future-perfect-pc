/* Execute the production cdecl range wrapper with the sound builder mocked. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "gameplay/pc_spatial_audio.h"
#define TSFP_PC_CONTROLS
static uint8_t memory[1024];
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_esp,g_eax;
#define SET32(a) (*(uint32_t *)(memory+(a)))
#define SETF(a) (*(float *)(memory+(a)))
static uint32_t MEM32(uint32_t a) {uint32_t v;memcpy(&v,memory+a,4);return v;}
static float MEMF(uint32_t a) {float v;memcpy(&v,memory+a,4);return v;}
static int enabled=1;
int pc_audio_enhanced_enabled(void) {return enabled;}
static float observed;
void sub_00021FB0_gen(void)
{observed=MEMF(g_esp+8);g_eax=17;g_esp+=4;}
#include "weapon_range.inc"
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)memory;
    const uint32_t callers[]={0x1b7216,0x1b71d6,0x123456};
    for(unsigned i=0;i<3;i++)for(unsigned repeat=0;repeat<2;repeat++) {
        g_esp=256;SET32(g_esp)=callers[i];SETF(g_esp+8)=50;
        sub_00021FB0();
        assert(observed==(i<2?150:50));
        assert(g_esp==260 && g_eax==17 && MEMF(264)==50);
    }
    g_esp=256;SET32(g_esp)=0x1b7216;SETF(g_esp+8)=-1;
    sub_00021FB0();assert(observed==-1 && MEMF(264)==-1);
    enabled=0;g_esp=256;SET32(g_esp)=0x1b7216;SETF(g_esp+8)=50;
    sub_00021FB0();assert(observed==50);
    puts("PASS: gunfire range, unrelated sounds, repeated calls and cdecl preservation");
    return 0;
}
