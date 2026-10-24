/* Exercise diagnostic lifetime, skip-frame readback, failure and ABI behavior.
 * GPU/COM services are controlled doubles; live GPU evidence is separate. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t g_esp,frame,scene=4,tick=300,whole=1,resource=0x80001000,kind=1,match=7;
static float delta=1;
int g_tsfp_native_timing_active=1;
static uint32_t *word(uint32_t address) {
    switch(address) {
    case 0x1004:return &resource;
    case 0x79094c:return &scene;
    case 0x7de30c:return &match;
    case 0x7de318:return &tick;
    case 0x7497b8:return &frame;
    case 0x4e7944:return &whole;
    case 0x80001000:return &kind;
    default:assert(!"Unexpected guest read");return NULL;
    }
}
#define PC_SURFACE_WORD(address) (*word(address))
#define PC_SURFACE_FLOAT(address) delta
#define TSFP_PC_NATIVE_RATE_H
static int rate=144;
static int pc_native_rate(void){return rate;}
#include "pc_native_surface_trace.h"
static unsigned refs,locks,unlocks,draws,calls;
static int fail_lock;
/* Deliberate row padding: hashing/writing must ignore it. */
static unsigned char pixels[128*528];
static ULONG __stdcall release(IDirect3DSurface8 *self) {(void)self;assert(refs);return --refs;}
static HRESULT __stdcall describe(IDirect3DSurface8 *self,D3DSURFACE_DESC *desc) {
    (void)self;memset(desc,0,sizeof *desc);desc->Width=desc->Height=128;
    desc->Format=D3DFMT_A8R8G8B8;return S_OK;
}
static HRESULT __stdcall lock(IDirect3DSurface8 *self,D3DLOCKED_RECT *out,const RECT *rect,DWORD flags) {
    (void)self;assert(!rect && flags==D3DLOCK_READONLY);locks++;
    if(fail_lock)return E_FAIL;
    out->Pitch=528;out->pBits=pixels;return S_OK;
}
static HRESULT __stdcall unlock(IDirect3DSurface8 *self) {(void)self;unlocks++;return S_OK;}
static const IDirect3DSurface8Vtbl surface_vtable={.Release=release,.GetDesc=describe,.LockRect=lock,.UnlockRect=unlock};
static IDirect3DSurface8 surface={&surface_vtable};
static HRESULT __stdcall target(IDirect3DDevice8 *self,IDirect3DSurface8 **out) {(void)self;*out=&surface;refs++;return S_OK;}
static const IDirect3DDevice8Vtbl device_vtable={.GetRenderTarget=target};
static IDirect3DDevice8 device={&device_vtable};
IDirect3DDevice8 *d3d8_GetDevice(void) {return &device;}
static void final(void) {draws++;memset(pixels,(unsigned char)draws,sizeof pixels);g_esp+=4;}
static void original(void) {
    calls++;
    if(whole && delta>0) {g_esp-=4;pc_surface_final_guest(final);}
    g_esp+=4;
}
static void step(unsigned positive) {whole=positive;delta=g_tsfp_native_timing_active?60.0f/rate:1.0f;g_esp=0x1000;frame++;tick+=positive;pc_surface_dispatch_guest(original);assert(g_esp==0x1004);}
int main(int argc,char **argv) {
    assert(argc==2);
    if(!strcmp(argv[1],"disabled")) {
        step(1);assert(calls==1 && draws==1 && !refs && !locks);return 0;
    }
    assert(getenv("TSFP_SURFACE_TRACE"));
    if(!strcmp(argv[1],"switches")) {
        const int rates[4]={240,180,0,120};
        for(unsigned section=0;section<4;section++) {
            rate=rates[section];g_tsfp_native_timing_active=rate>60;
            unsigned paused_locks=locks,paused_rows=pc_surface_trace.rows;
            delta=0;g_esp=0x1000;frame++;pc_surface_dispatch_guest(original);
            assert(g_esp==0x1004 && locks==paused_locks && pc_surface_trace.rows==paused_rows);
            for(unsigned i=0;i<90;i++)step(1);
            assert(pc_surface_trace.rows==(section+1)*90);
            unsigned before=locks;for(unsigned i=0;i<10;i++)step(1);assert(locks==before);
        }
        assert(!refs && !pc_surface_trace.enabled && pc_surface_trace.segment_index==3);
        puts("PASS: four bounded rate segments, skipped tails and balanced references");return 0;
    }
    step(0);assert(!locks && !pc_surface_trace.rows); /* no image yet */
    step(1);assert(locks==1 && unlocks==1 && refs==1 && pc_surface_trace.rows==1);
    step(0);assert(locks==2 && draws==1 && pc_surface_trace.rows==2); /* read real retained image */
    g_esp=0x1000;pc_surface_dispatch_guest(original);assert(locks==2 && g_esp==0x1004); /* view alias */
    scene=101;step(1);assert(!refs && locks==2);scene=4;
    if(!strcmp(argv[1],"failure")) {
        fail_lock=1;step(1);assert(pc_surface_trace.failed && !refs && !pc_surface_trace.enabled);
        step(1);assert(calls==7 && draws==4);return 0;
    }
    while(pc_surface_trace.rows<PC_SURFACE_TRACE_LIMIT)step(frame%3==0);
    assert(!refs && !pc_surface_trace.enabled && !pc_surface_trace.csv && !pc_surface_trace.pixels);
    unsigned before=locks;step(1);assert(locks==before);
    printf("PASS: bounded read-only image trace; %u rows; balanced refs and unchanged stack\n",PC_SURFACE_TRACE_LIMIT);
    return 0;
}
