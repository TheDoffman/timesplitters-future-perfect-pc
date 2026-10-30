#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "d3d8_xbox.h"
#include "pc_surface_precision.h"
typedef struct { uint32_t va,data,format,size;IDirect3DTexture8 *host;int rendered;
    uint32_t render_format;unsigned long used_swap,checked_swap; } texture_entry;
typedef struct { UINT width,height,levels,fmt; } texture_layout;
static texture_entry g_textures[1];static int g_texture_count=1;
#define MAX_STAGES 4
static IDirect3DTexture8 old_object,new_object,white_object;
static IDirect3DTexture8 *g_bound[MAX_STAGES];
static PcSurfacePrecision g_surface_precision;
static unsigned g_skip_create,creates,copies,releases,bindings,flushed,old_released,new_released;
static int deferred,fail_create,fail_copy,layout_ok=1;
static uint32_t desired=6,gpu_state;
static int live,g_surface_high_rate,check_cache_pin=1;
static int pc_surface_live_requested(void){return live;}
static uint32_t mem32(uint32_t va) {
    if(va==0x10004)return 0x123456;
    if(va==0x1000c)return 6<<8;
    assert(va==0x10010);return 0;
}
#define HLE_MEM32(va) mem32(va)
static unsigned long hle_d3d8_shadow_swaps(void){return 50;}
static int hle_d3d8_defer_recording(void){return deferred;}
static void hle_d3d8_defer_flush(void){if(check_cache_pin)assert(g_textures[0].used_swap==50);flushed++;deferred=0;gpu_state=123;}
static int read_layout(uint32_t va,texture_layout *t) {
    assert(va==0x10000);*t=(texture_layout){128,128,1,6};return layout_ok;
}
static texture_entry *cache_slot(IDirect3DDevice8 *dev,unsigned long now)
{ (void)dev;(void)now;assert(0);return NULL; }
static IDirect3DTexture8 *white_texture(IDirect3DDevice8 *dev)
{ (void)dev;assert(0);return &white_object; }
static HRESULT host_CreateTexture(IDirect3DDevice8 *dev,UINT w,UINT h,UINT levels,DWORD usage,
    D3DFORMAT fmt,D3DPOOL pool,IDirect3DTexture8 **out) {
    (void)dev;(void)pool;creates++;assert(!deferred && w==128 && h==128 && levels==1);
    assert(usage==D3DUSAGE_RENDERTARGET && (uint32_t)fmt==desired);
    *out=fail_create==1?NULL:&new_object;return fail_create?E_FAIL:S_OK;
}
static HRESULT d3d8_PreserveFeedbackTexture(IDirect3DTexture8 *dst,IDirect3DTexture8 *src) {
    copies++;assert(dst==&new_object && src==&old_object && gpu_state==123);
    assert(!old_released && g_textures[0].host==&old_object);
    return fail_copy?E_FAIL:S_OK;
}
static HRESULT host_SetTexture(IDirect3DDevice8 *dev,DWORD stage,IDirect3DBaseTexture8 *object) {
    (void)dev;bindings++;assert(stage==0 || stage==2);
    assert(object==(IDirect3DBaseTexture8 *)&new_object && copies && !old_released);
    assert(g_bound[stage]==&new_object && g_textures[0].host==&new_object);return S_OK;
}
static ULONG host_ReleaseTexture(IDirect3DTexture8 *object) {
    releases++;
    if(object==&old_object){old_released++;assert(bindings==2 && copies);}
    else {assert(object==&new_object && !bindings);new_released++;}
    return 0;
}
/* PRODUCTION */
static IDirect3DTexture8 *sample_cached(void) {
    texture_entry *e=&g_textures[0];uint32_t va=e->va,format=e->format;
    IDirect3DDevice8 *dev=NULL;
/* READ_GUARD */
    return e->host;
}
static void setup(int wide) {
    g_textures[0]=(texture_entry){0x10000,0x123456,6<<8,0,&old_object,1,wide?0x4bu:6u,1,2};
    memset(g_bound,0,sizeof g_bound);g_bound[0]=g_bound[2]=&old_object;
    pc_surface_precision_reset(&g_surface_precision);
    if(!wide)assert(pc_surface_precision_set(&g_surface_precision,0,0x10000));
    desired=wide?6u:0x4bu;gpu_state=0;deferred=1;
    creates=copies=releases=bindings=flushed=old_released=new_released=g_skip_create=0;
    fail_create=fail_copy=0;layout_ok=1;
}
int main(void) {
    for(int wide=0;wide<2;wide++)for(int failure=0;failure<4;failure++) {
        setup(wide);fail_create=failure<3?failure:0;fail_copy=failure==3;
        texture_entry before=g_textures[0];
        IDirect3DTexture8 *result=hle_d3d8_render_texture(NULL,0x10000);
        assert(flushed==1 && creates==1 && gpu_state==123);
        if(failure) {
            assert(result==&old_object && !old_released && !bindings && g_skip_create==1);
            assert(g_textures[0].host==before.host && g_textures[0].render_format==before.render_format);
            assert(g_bound[0]==&old_object && g_bound[2]==&old_object);
            assert(new_released==(failure!=1));
        } else {
            assert(result==&new_object && old_released==1 && !new_released && bindings==2);
            assert(g_textures[0].render_format==desired && g_textures[0].used_swap==50);
            assert(g_textures[0].va==before.va && g_textures[0].data==before.data && g_textures[0].format==before.format);
            assert(hle_d3d8_render_texture(NULL,0x10000)==result && creates==1 && copies==1);
        }
    }
    setup(0);layout_ok=0;
    assert(!hle_d3d8_render_texture(NULL,0x10000) && !creates && !releases && deferred);
    /* Read-first transitions use the actual host_texture precision guard. */
    for(int high=0;high<2;high++) {
        setup(!high);live=1;check_cache_pin=0;g_surface_high_rate=!high;
        assert(pc_surface_precision_set(&g_surface_precision,0,0x10000));
        assert(sample_cached()==&old_object && !creates);
        hle_d3d8_surface_precision_rate(high);assert(flushed==1 && g_surface_high_rate==high);
        assert(sample_cached()==&new_object && creates==1 && copies==1);
        hle_d3d8_surface_precision_rate(high);assert(flushed==1); /* 120 <-> 144 */
        assert(sample_cached()==&new_object && creates==1);
    }
    puts("PASS: precision promotion/demotion, prior-draw flush, rebinding, rollback and stable cache reuse");
    return 0;
}
