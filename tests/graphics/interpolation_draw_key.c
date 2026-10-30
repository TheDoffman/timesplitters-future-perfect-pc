/* The production recorder must recognize combined transforms, not just P. */
#include "hle/hle_d3d8_record.h"
#include <assert.h>
#include <stdio.h>
#define CAPTURE_STAGES 4
static DWORD g_bound_vs=0x10001;
static IDirect3DBaseTexture8 *g_bound_tex[4];
static IDirect3DBaseTexture8 *g_target_texture;
static int g_target_lost, programmable=1, read_projection, read_transform;
static int first=64,count=12,projection=60;
BOOL d3d8_vsh_is_programmable(DWORD vs){(void)vs;return programmable;}
int d3d8_vsh_bound_uses_projection(void){return read_projection;}
void d3d8_vsh_interp_registers(int *p,int *f,int *n){*p=projection;*f=first;*n=count;}
uint32_t d3d8_vsh_bound_reads(int start,int n){
    if(start==60 && n==4)return read_projection;
    if(start==64 && n==12)return read_transform ? 2 : 0; /* c68, not c60 */
    return 0;
}
#include "draw_key.inc"
int main(void)
{
    HleInterpDrawKey key={0};
    draw_key(&key,4,2,28,101);assert(!key.uses_proj); /* HUD */
    read_projection=1;draw_key(&key,4,2,28,101);assert(key.uses_proj);
    read_projection=0;read_transform=1;
    draw_key(&key,4,2,28,101);assert(key.uses_proj); /* combined world/sky */
    first=-1;count=0;draw_key(&key,4,2,28,101);assert(!key.uses_proj); /* saved override */
    first=64;count=12;programmable=0;
    draw_key(&key,4,2,28,101);assert(!key.uses_proj); /* fixed-function unchanged */
    puts("PASS: production interpolation draw classification and disabled-register override");
    return 0;
}
