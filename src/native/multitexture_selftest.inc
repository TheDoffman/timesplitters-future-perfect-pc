#include "dependent_texture_selftest.inc"
static int native_test_multitexture(IDirect3DDevice8 *dev) {
    NATIVE_GL_SCOPE;
    struct Vertex {float p[4];DWORD color;float uv[4][4];} v[4]={
        {{0,0,0,1},0xffffffff},{{64,0,0,1},0xffffffff},
        {{64,64,0,1},0xffffffff},{{0,64,0,1},0xffffffff}};
    const DWORD colors[4]={0xffff0000,0xff00ff00,0xff0000ff,0xffffff00};
    IDirect3DTexture8 *textures[4]={0};int ok=1;
    for(int stage=0;stage<4;stage++) {
        ok &= SUCCEEDED(dev->lpVtbl->CreateTexture(dev,4,4,1,0,stage==3?D3DFMT_LIN_A8R8G8B8:D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&textures[stage]));
        if(!textures[stage])continue;
        D3DLOCKED_RECT lock;
        ok &= SUCCEEDED(textures[stage]->lpVtbl->LockRect(textures[stage],0,&lock,NULL,0));
        for(int y=0;y<4;y++)for(int x=0;x<4;x++)
            ((DWORD*)lock.pBits)[stage==3?y*4+x:swizzle_offset(x,y,4,4)]=(x<2)==(stage%2==0)?colors[stage]:0xff000000;
        ok &= SUCCEEDED(textures[stage]->lpVtbl->UnlockRect(textures[stage],0));
        dev->lpVtbl->SetTexture(dev,stage,(IDirect3DBaseTexture8*)textures[stage]);
        for(int i=0;i<4;i++){v[i].uv[stage][0]=stage==3?3.0f:stage%2?.75f:.25f;v[i].uv[stage][1]=.5f;v[i].uv[stage][3]=1;}
    }
    DWORD shader=0;const DWORD microcode[]={0,0x0020001B,0x0836106C,0x2070F801};
    ok &= SUCCEEDED(d3d8_vsh_create_shader(microcode,1,&shader));
    if(!shader)return 0;
    /* Six MOVs route position, diffuse and each distinct coordinate. */
    NV2AVshProgram *p=&native_programs[shader-0x10000];memset(p,0,sizeof *p);p->length=6;
    D3D8VshInput decl[6]={{0,RHI_FORMAT_R32G32B32A32_FLOAT,0},{1,RHI_FORMAT_B8G8R8A8_UNORM,16}};
    for(int i=0;i<6;i++) {
        NV2AVshInstruction *in=&p->insns[i];in->mac_op=NV2A_VSH_MAC_MOV;
        in->mac_src[0]=native_test_source(NV2A_VSH_REG_INPUT,i);
        in->mac_dst=(NV2AVshDstOperand){-1,i==0?0:i==1?3:9+i-2,0,15};
        in->ilu_dst.temp_reg=-1;in->ilu_dst.output_reg=NV2A_VSH_OUT_NONE;in->is_final=i==5;
        if(i>=2)decl[i]=(D3D8VshInput){i,RHI_FORMAT_R32G32B32A32_FLOAT,20+(i-2)*16};
    }
    ok &= SUCCEEDED(d3d8_vsh_set_declaration(shader,decl,6));
    const float scale[]={32,-32,1,1},offset[]={32,32,0,0};d3d8_vsh_set_screenspace(scale,offset);
    d3d8_combiners_set_pixel_shader(1);
    dev->lpVtbl->SetRenderState(dev,D3DRS_PSCOMBINERCOUNT,0);
    dev->lpVtbl->SetRenderState(dev,D3DRS_PSTEXTUREMODES,0);
    for(int path=0;path<3;path++) {
        if(path==1) {
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSTEXTUREMODES,D3D8_PSTEXTUREMODES_XBOX|0x8421);
            for(int i=0;i<4;i++)for(int j=0;j<4;j++){v[i].uv[j][0]*=2;v[i].uv[j][1]*=2;v[i].uv[j][3]=2;}
        }
        if(path==2)setenv("TSFP_NATIVE_CPU_VERTEX","1",1);
        DWORD fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|(4<<D3DFVF_TEXCOUNT_SHIFT);
        for(int i=0;i<4;i++)fvf|=2u<<(16+2*i);
        dev->lpVtbl->SetVertexShader(dev,path?shader:fvf);
        for(int stage=0;stage<4;stage++) {
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSFINALCOMBINERINPUTSABCD,8+stage);
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSFINALCOMBINERINPUTSEFG,(24+stage)<<8);
            dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xff000000,1,0);
            ok &= SUCCEEDED(dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,v,sizeof v[0]));
            ok &= native_test_pixel(dev,32,32,(colors[stage]>>16)&255,(colors[stage]>>8)&255,colors[stage]&255);
        }
    }
    unsetenv("TSFP_NATIVE_CPU_VERTEX");
    for(int stage=0;stage<4;stage++){dev->lpVtbl->SetTexture(dev,stage,NULL);if(textures[stage])textures[stage]->lpVtbl->Release(textures[stage]);}
    ok &= native_test_dependent_textures(dev,shader);
    d3d8_combiners_set_pixel_shader(0);d3d8_vsh_delete_shader(shader);
    dev->lpVtbl->SetRenderState(dev,D3DRS_PSTEXTUREMODES,0);
    dev->lpVtbl->SetVertexShader(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    fprintf(stderr,"[native-gl-test] four texture stages, distinct coordinates, FVF/GPU/CPU: %s\n",ok?"PASS":"FAIL");return ok;
}
