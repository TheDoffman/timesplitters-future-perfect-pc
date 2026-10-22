#ifndef PC_NATIVE_SURFACE_TRACE_H
#define PC_NATIVE_SURFACE_TRACE_H
/* Optional US procedural-texture GPU observation. Uses the shared D3D surface
 * readback interface, including native Mac; never treats a CPU stamp as pixels.
 * Capturing synchronizes the GPU and therefore is NOT a performance benchmark. */
#include "d3d/d3d8_xbox.h"
#include "pc_native_rate.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
extern IDirect3DDevice8 *d3d8_GetDevice(void);
#ifndef PC_SURFACE_WORD
#define PC_SURFACE_WORD(va) (*(volatile uint32_t *)((uintptr_t)(va)+g_xbox_mem_offset))
#endif
#ifndef PC_SURFACE_FLOAT
#define PC_SURFACE_FLOAT(va) (*(const float *)((uintptr_t)(va)+g_xbox_mem_offset))
#endif
#define PC_SURFACE_TRACE_LIMIT 360u
static struct {
    int checked,enabled,failed;
    unsigned rows,segment_rows,segment_index;
    int segmented,segment_rate;
    uint32_t resource,scene,last_frame;
    int have_frame,executed;
    IDirect3DSurface8 *surface;
    FILE *csv,*pixels;
} pc_surface_trace;
static void pc_surface_trace_close(void)
{
    if(pc_surface_trace.surface) {
        pc_surface_trace.surface->lpVtbl->Release(pc_surface_trace.surface);
        pc_surface_trace.surface=NULL;
    }
    if(pc_surface_trace.csv)fclose(pc_surface_trace.csv);
    if(pc_surface_trace.pixels)fclose(pc_surface_trace.pixels);
    pc_surface_trace.csv=pc_surface_trace.pixels=NULL;
    pc_surface_trace.enabled=0;
}
static int pc_surface_trace_enabled(void)
{
    if(!pc_surface_trace.checked) {
        pc_surface_trace.checked=1;
        const char *segments=getenv("TSFP_SURFACE_TRACE_SWITCHES");
        pc_surface_trace.segmented=segments && segments[0]=='1';
        const char *path=getenv("TSFP_SURFACE_TRACE");
        if(path && *path && strlen(path)<480) {
            char pixels[512];snprintf(pixels,sizeof pixels,"%s.rgba",path);
            pc_surface_trace.csv=fopen(path,"w");
            pc_surface_trace.pixels=fopen(pixels,"wb");
            if(pc_surface_trace.csv && pc_surface_trace.pixels) {
                pc_surface_trace.enabled=1;atexit(pc_surface_trace_close);
                fputs("sample,frame,scene,level_ticks,whole_ticks,delta,native,resource,executed,format,hash,rate,segment\n",pc_surface_trace.csv);
            } else {
                fprintf(stderr,"[SURFACE-TRACE] Cannot open requested files\n");
                pc_surface_trace_close();
            }
        }
    }
    return pc_surface_trace.enabled;
}
static void pc_surface_trace_error(const char *reason)
{
    fprintf(stderr,"[SURFACE-TRACE] Failed: %s\n",reason);
    pc_surface_trace.failed=1;pc_surface_trace_close();
}
/* Called after the final image pass, before the dispatcher restores its target.
 * Retain only this owned COM reference; scene/resource changes release it. */
static void pc_surface_final_guest(void (*original)(void))
{
    original();
    if(!pc_surface_trace_enabled() || !pc_surface_trace.resource)return;
    IDirect3DDevice8 *device=d3d8_GetDevice();
    IDirect3DSurface8 *surface=NULL;
    if(!device || FAILED(device->lpVtbl->GetRenderTarget(device,&surface)) || !surface) {
        pc_surface_trace_error("missing final render target");return;
    }
    if(pc_surface_trace.surface)pc_surface_trace.surface->lpVtbl->Release(pc_surface_trace.surface);
    pc_surface_trace.surface=surface;pc_surface_trace.executed=1;
}
static void pc_surface_dispatch_guest(void (*original)(void))
{
    if(!pc_surface_trace_enabled()){original();return;}
    uint32_t resource=PC_SURFACE_WORD(g_esp+4),scene=PC_SURFACE_WORD(0x79094cu);
    /* Reproducible shoreline resource in Scotland; skip intro/menu and loading.
     * A bounded diagnostic, deliberately not a generic full-game census. */
    int eligible=scene==4u && PC_SURFACE_WORD(0x7de30cu)==7u && PC_SURFACE_WORD(0x7de318u)>=300u &&
        resource>=0x80000000u && resource<=0x83fff000u && PC_SURFACE_WORD(resource)==1u;
    if(!eligible || resource!=pc_surface_trace.resource || scene!=pc_surface_trace.scene) {
        if(pc_surface_trace.surface)pc_surface_trace.surface->lpVtbl->Release(pc_surface_trace.surface);
        pc_surface_trace.surface=NULL;pc_surface_trace.have_frame=0;
    }
    pc_surface_trace.resource=eligible?resource:0;
    pc_surface_trace.scene=scene;pc_surface_trace.executed=0;
    original();
    if(!pc_surface_trace.enabled || !eligible || !pc_surface_trace.surface)return;
    uint32_t frame=PC_SURFACE_WORD(0x7497b8u);
    if(pc_surface_trace.have_frame && pc_surface_trace.last_frame==frame)return;
    int rate=pc_native_rate();if(rate<=60)rate=60;
    if(pc_surface_trace.segmented) {
        if(!(PC_SURFACE_FLOAT(0x4e794cu)>0))return;
        if(pc_surface_trace.segment_rows && pc_surface_trace.segment_rate!=rate) {
            pc_surface_trace.segment_rows=0;pc_surface_trace.segment_index++;
        }
        if(pc_surface_trace.segment_rows>=90)return;
        pc_surface_trace.segment_rate=rate;
    }
    D3DSURFACE_DESC desc;D3DLOCKED_RECT locked;
    IDirect3DSurface8 *surface=pc_surface_trace.surface;
    if(FAILED(surface->lpVtbl->GetDesc(surface,&desc)) || desc.Width!=128 || desc.Height!=128 ||
       (desc.Format!=D3DFMT_A8R8G8B8 && desc.Format!=D3DFMT_X8R8G8B8 &&
        desc.Format!=D3DFMT_LIN_A8R8G8B8 && desc.Format!=D3DFMT_LIN_X8R8G8B8)) {
        pc_surface_trace_error("unexpected target dimensions/format");return;
    }
    if(FAILED(surface->lpVtbl->LockRect(surface,&locked,NULL,D3DLOCK_READONLY))) {
        pc_surface_trace_error("GPU readback");return;
    }
    int ok=locked.pBits && locked.Pitch>=512;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned y=0;ok && y<128;y++) {
        const unsigned char *row=(const unsigned char *)locked.pBits+(size_t)y*locked.Pitch;
        for(unsigned x=0;x<512;x++) { hash^=row[x];hash*=UINT64_C(1099511628211); }
        ok=fwrite(row,1,512,pc_surface_trace.pixels)==512;
    }
    if(FAILED(surface->lpVtbl->UnlockRect(surface)))ok=0;
    if(!ok){pc_surface_trace_error("pixel read/write");return;}
    fprintf(pc_surface_trace.csv,"%u,%u,%u,%u,%u,%.9g,%d,%u,%d,%u,%016llx,%d,%u\n",
        pc_surface_trace.rows,frame,scene,PC_SURFACE_WORD(0x7de318u),PC_SURFACE_WORD(0x4e7944u),
        PC_SURFACE_FLOAT(0x4e794cu),g_tsfp_native_timing_active,resource,pc_surface_trace.executed,
        (unsigned)desc.Format,(unsigned long long)hash,rate,pc_surface_trace.segment_index);
    pc_surface_trace.segment_rows++;
    pc_surface_trace.last_frame=frame;pc_surface_trace.have_frame=1;
    if(++pc_surface_trace.rows==PC_SURFACE_TRACE_LIMIT) {
        fprintf(stderr,"[SURFACE-TRACE] Completed %u GPU images\n",pc_surface_trace.rows);
        pc_surface_trace_close();
    }
}
#endif
