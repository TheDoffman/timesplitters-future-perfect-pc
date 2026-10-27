/* Actual common Xbox device contract. Caller supplies a 64x64 device with
 * XYZRHW/DIFFUSE, unlit fixed-function color and no blending/culling. */
#ifndef TSFP_INTERPOLATION_DEPTH_DEVICE_H
#define TSFP_INTERPOLATION_DEPTH_DEVICE_H
#include "d3d8_xbox.h"
#include <stdio.h>
#include <stdlib.h>
#define DEPTH_CHECK(x) do {if(!(x)){fprintf(stderr,"[interp-depth-test] FAIL line %d: %s\n",__LINE__,#x);return 0;}}while(0)
static int depth_pixel(IDirect3DDevice8 *dev,unsigned r,unsigned g,unsigned b) {
    IDirect3DSurface8 *surface=NULL;D3DLOCKED_RECT lock;int ok=0;
    if(FAILED(dev->lpVtbl->GetBackBuffer(dev,0,0,&surface)) || !surface)return 0;
    if(SUCCEEDED(surface->lpVtbl->LockRect(surface,&lock,NULL,D3DLOCK_READONLY))) {
        const unsigned char *p=(const unsigned char *)lock.pBits+8*lock.Pitch+8*4;
        ok=abs((int)p[0]-(int)r)<3 && abs((int)p[1]-(int)g)<3 && abs((int)p[2]-(int)b)<3;
        if(!ok)fprintf(stderr,"[interp-depth-test] pixel %u,%u,%u expected %u,%u,%u\n",p[0],p[1],p[2],r,g,b);
        surface->lpVtbl->UnlockRect(surface);
    }
    surface->lpVtbl->Release(surface);return ok;
}
static int depth_triangle(IDirect3DDevice8 *dev,float z,DWORD color) {
    struct {float x,y,z,w;DWORD color;} vertices[]={{0,0,z,1,color},{64,0,z,1,color},{0,64,z,1,color}};
    return SUCCEEDED(dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_TRIANGLELIST,1,vertices,sizeof vertices[0]));
}
static int interpolation_depth_device_test(IDirect3DDevice8 *dev) {
    IDirect3DSurface8 *automatic=NULL,*custom=NULL,*restored=NULL;
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->GetDepthStencilSurface(dev,&automatic)) && automatic);
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->CreateDepthStencilSurface(dev,64,64,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,&custom)) && custom);
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZENABLE,TRUE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZWRITEENABLE,TRUE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZFUNC,D3DCMP_LESS);
    dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILMASK,255);
    dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILWRITEMASK,255);
    dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILFAIL,1); /* KEEP */
    dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILZFAIL,1);
    dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILPASS,1);
    for(int target=0;target<2;target++) {
        IDirect3DSurface8 *ds=target?custom:automatic;
        DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,ds)));
        DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xFF0000FF,.25f,7)));
        if (target) {
            DEPTH_CHECK(!xbox_D3D8InterpCanPredict());
            xbox_D3D8InterpPredict(TRUE);
            DEPTH_CHECK(!xbox_D3D8InterpBegin());
            xbox_D3D8InterpPredict(FALSE);
            DEPTH_CHECK(depth_pixel(dev,0,0,255));
        }
        for(int replay=0;replay<(target?2:3);replay++) {
            if (replay==2) {
                DEPTH_CHECK(xbox_D3D8InterpCanPredict());
                xbox_D3D8InterpPredict(TRUE);
            }
            DEPTH_CHECK(xbox_D3D8InterpBegin());
            DEPTH_CHECK(!xbox_D3D8InterpBegin()); /* nested begin must preserve the active replay */
            dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILENABLE,TRUE);
            dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILFUNC,D3DCMP_EQUAL);
            dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILREF,0);
            dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF000000,0,0);
            DEPTH_CHECK(depth_triangle(dev,.5f,0xFF00FF00));
            DEPTH_CHECK(depth_pixel(dev,0,255,0)); /* fresh replay depth 1, stencil 0 */
            dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0,.1f,3);
            /* Rebinding the same surface must retain this replay's history. */
            DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,ds)));
            dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILENABLE,FALSE);
            DEPTH_CHECK(depth_triangle(dev,.5f,0xFFFF0000));
            DEPTH_CHECK(depth_pixel(dev,0,255,0));
            /* End must restore the original binding even if replay leaves no depth. */
            DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,NULL)));
            xbox_D3D8InterpEnd();xbox_D3D8InterpEnd();
            xbox_D3D8InterpPredict(FALSE);
            DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->GetDepthStencilSurface(dev,&restored)) && restored==ds);
            restored->lpVtbl->Release(restored);restored=NULL;
            DEPTH_CHECK(depth_pixel(dev,0,0,255)); /* guest color survived */
            DEPTH_CHECK(depth_triangle(dev,.5f,0xFFFF0000));
            DEPTH_CHECK(depth_pixel(dev,0,0,255)); /* guest depth .25 survived */
        }
        dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILENABLE,TRUE);
        dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILREF,7);
        DEPTH_CHECK(depth_triangle(dev,.1f,0xFF00FF00));
        DEPTH_CHECK(depth_pixel(dev,0,255,0)); /* guest stencil 7 survived */
    }
    dev->lpVtbl->SetRenderState(dev,D3DRS_STENCILENABLE,FALSE);
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZENABLE,FALSE);
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,automatic)));
    /* A custom target first selected inside replay must be isolated too. */
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xFF0000FF,.25f,7)));
    DEPTH_CHECK(xbox_D3D8InterpBegin());
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,custom)));
    dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0,.6f,9);
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,automatic)));
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZENABLE,TRUE);
    dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF000000,0,0);
    DEPTH_CHECK(depth_triangle(dev,.5f,0xFF00FF00) && depth_pixel(dev,0,255,0));
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,custom)));
    DEPTH_CHECK(depth_triangle(dev,.8f,0xFFFF0000) && depth_pixel(dev,0,255,0));
    xbox_D3D8InterpEnd();
    DEPTH_CHECK(SUCCEEDED(dev->lpVtbl->GetDepthStencilSurface(dev,&restored)) && restored==automatic);
    restored->lpVtbl->Release(restored);
    DEPTH_CHECK(depth_pixel(dev,0,0,255));
    dev->lpVtbl->SetRenderState(dev,D3DRS_ZENABLE,FALSE);
    custom->lpVtbl->Release(custom);automatic->lpVtbl->Release(automatic);
    fprintf(stderr,"[interp-depth-test] automatic/custom depth, fresh replay history, guest depth/stencil/color and bindings: PASS\n");
    return 1;
}
#undef DEPTH_CHECK
#endif
