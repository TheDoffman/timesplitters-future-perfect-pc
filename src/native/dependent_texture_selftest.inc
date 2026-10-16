/* Known quadrant colors distinguish dependent inputs, signed mappings and stage
 * order. Exercise the same fragment path with fixed, GPU and CPU vertices. */
static int native_test_dependent_textures(IDirect3DDevice8 *dev,DWORD shader) {
    NATIVE_GL_SCOPE;
    struct Vertex {float p[4];DWORD color;float uv[4][4];} v[4]={
        {{0,0,0,1},0xffffffff,{{0}}},{{64,0,0,1},0xffffffff,{{0}}},
        {{64,64,0,1},0xffffffff,{{0}}},{{0,64,0,1},0xffffffff,{{0}}}};
    IDirect3DTexture8 *textures[4]={0};DWORD saved[4][4];int ok=1;
    const DWORD quadrants[]={0xffff0000,0xff00ff00,0xff0000ff,0xffffff00};
    const DWORD states[]={D3DTSS_MINFILTER,D3DTSS_MAGFILTER,D3DTSS_ADDRESSU,D3DTSS_ADDRESSV};
    for(int stage=0;stage<4;stage++) {
        for(int j=0;j<4;j++) {saved[stage][j]=g.tss[stage][states[j]];
            dev->lpVtbl->SetTextureStageState(dev,stage,states[j],j<2?D3DTEXF_POINT:D3DTADDRESS_CLAMP);}
        ok &= SUCCEEDED(dev->lpVtbl->CreateTexture(dev,4,4,1,0,stage==3?D3DFMT_LIN_A8R8G8B8:D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&textures[stage]));
        if(!textures[stage])continue;
        D3DLOCKED_RECT lock;
        if(FAILED(textures[stage]->lpVtbl->LockRect(textures[stage],0,&lock,NULL,0))){ok=0;continue;}
        for(int y=0;y<4;y++)for(int x=0;x<4;x++)
            ((DWORD*)lock.pBits)[stage==3?y*4+x:swizzle_offset(x,y,4,4)]=quadrants[(x>=2)+2*(y>=2)];
        ok &= SUCCEEDED(textures[stage]->lpVtbl->UnlockRect(textures[stage],0));
        dev->lpVtbl->SetTexture(dev,stage,(IDirect3DBaseTexture8*)textures[stage]);
    }
    DWORD fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE|(4<<D3DFVF_TEXCOUNT_SHIFT);
    for(int i=0;i<4;i++)fvf|=2u<<(16+2*i);
    for(int path=0;path<3;path++) {
        if(path==2)setenv("TSFP_NATIVE_CPU_VERTEX","1",1);else unsetenv("TSFP_NATIVE_CPU_VERTEX");
        dev->lpVtbl->SetVertexShader(dev,path?shader:fvf);
        for(int test=0;test<12;test++) {
            unsigned modes[4]={4,15,0,0},output=1,input=0,mapping=0;DWORD expected=quadrants[1];
            float q[4][4]={{.25f,.25f,.75f,.75f},{1,0,0,1},{0,1,0,1},{0,1,0,1}};
            if(test==1){modes[1]=16;expected=quadrants[2];}
            if(test==2 || test==3){modes[1]=15;modes[2]=16;output=2;input=(test==2?1u:15u)<<16;expected=quadrants[test==2?1:2];}
            if(test>=4 && test<=8) {
                modes[1]=17;modes[2]=9;output=2;unsigned dm=test-4;mapping=dm|(dm<<4);
                q[0][0]=.25f;q[0][1]=.75f;expected=quadrants[2];
                if(dm==1){q[0][0]=191.0f/255;q[0][1]=64.0f/255;q[2][1]=-1;}
                if(dm==2 || dm==3){q[0][0]=64.0f/255;q[0][1]=(dm==2?191.0f:192.0f)/255;q[2][1]=-1;expected=quadrants[3];}
            }
            if(test==9){modes[1]=0;modes[2]=17;modes[3]=9;output=3;
                q[0][0]=.75f;q[0][1]=.25f;q[2][0]=4;q[2][1]=0;q[3][1]=4;expected=quadrants[1];}
            /* Illegal stage combinations read zero, as on the PC path. */
            if(test==10){modes[0]=15;output=0;expected=0;}
            if(test==11){modes[3]=17;output=3;expected=0;}
            for(int i=0;i<4;i++)memcpy(v[i].uv,q,sizeof q);
            DWORD tm=D3D8_PSTEXTUREMODES_XBOX;for(int i=0;i<4;i++)tm|=modes[i]<<(5*i);
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSTEXTUREMODES,tm);
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSINPUTTEXTURE,input);
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSDOTMAPPING,mapping);
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSFINALCOMBINERINPUTSABCD,8+output);
            dev->lpVtbl->SetRenderState(dev,D3DRS_PSFINALCOMBINERINPUTSEFG,(24+output)<<8);
            dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xff808080,1,0);
            int pass=SUCCEEDED(dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,v,sizeof v[0])) &&
                native_test_pixel(dev,32,32,(expected>>16)&255,(expected>>8)&255,expected&255);
            if(!pass)fprintf(stderr,"[native-dependent-test] path %d case %d failed\n",path,test);
            ok &= pass;
        }
    }
    unsetenv("TSFP_NATIVE_CPU_VERTEX");
    dev->lpVtbl->SetRenderState(dev,D3DRS_PSINPUTTEXTURE,0);
    dev->lpVtbl->SetRenderState(dev,D3DRS_PSDOTMAPPING,0);
    for(int stage=0;stage<4;stage++) {
        dev->lpVtbl->SetTexture(dev,stage,NULL);if(textures[stage])textures[stage]->lpVtbl->Release(textures[stage]);
        for(int j=0;j<4;j++)dev->lpVtbl->SetTextureStageState(dev,stage,states[j],saved[stage][j]);
    }
    fprintf(stderr,"[native-dependent-test] AR/GB chains, invalid sources/stages, dot mappings, linear DOT_ST, FVF/GPU/CPU: %s\n",ok?"PASS":"FAIL");return ok;
}
