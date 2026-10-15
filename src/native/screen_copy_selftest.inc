/* Synthetic real-GPU pixels, independent of game assets and personal saves. */
static int native_test_screen_copy(IDirect3DDevice8 *dev) {
    NATIVE_GL_SCOPE;
    struct CopyVertex {float x,y,z,w;DWORD color;float u,v;} v[4]={
        {0,0,0,1,0xFFFFFFFF,0,0},{64,0,0,1,0xFFFFFFFF,1,0},
        {64,64,0,1,0xFFFFFFFF,1,1},{0,64,0,1,0xFFFFFFFF,0,1}};
    int ok=1;
    for(int large=0;large<2;large++) {
        ok &= pc_display_apply(large?(PcDisplayMode){1920,1080}:(PcDisplayMode){0,0});
        dev->lpVtbl->SetTexture(dev,0,NULL);
        dev->lpVtbl->SetVertexShader(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF0000FF,1,0);
        D3DRECT clip={16,8,48,32};xbox_D3D8SetScissors(1,FALSE,&clip);
        dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,v,sizeof v[0]);
        ok &= native_test_pixel(dev,24,16,255,255,255);
        ok &= native_test_pixel(dev,8,16,0,0,255);
        ok &= native_test_pixel(dev,24,48,0,0,255);
        /* Clear ignores drawing scissors, as D3D requires. */
        dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF0000FF,1,0);
        D3DRECT top={0,0,64,32};dev->lpVtbl->Clear(dev,1,&top,D3DCLEAR_TARGET,0xFFFF0000,1,0);
        IDirect3DTexture8 *tex=NULL;
        ok &= SUCCEEDED(dev->lpVtbl->CreateTexture(dev,64,64,1,0,D3DFMT_LIN_A8R8G8B8,D3DPOOL_DEFAULT,&tex));
        if(!tex){ok=0;break;}
        GLint read,draw,after;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        ok &= SUCCEEDED(xbox_D3D8CopyBackBufferToTexture(tex));
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&after);ok &= after==read;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&after);ok &= after==draw;
        ok &= glIsEnabled(GL_SCISSOR_TEST);
        xbox_D3D8SetScissors(0,FALSE,NULL);
        dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF00FF00,1,0);
        dev->lpVtbl->SetTexture(dev,0,(IDirect3DBaseTexture8*)tex);
        dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,v,sizeof v[0]);
        ok &= native_test_pixel(dev,24,16,255,0,0);
        ok &= native_test_pixel(dev,24,48,0,0,255);
        dev->lpVtbl->SetTexture(dev,0,NULL);
        /* Partial copies preserve the rest and pair clipping on both ends. */
        dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xFF00FF00,1,0);
        RECT source={0,0,32,32};POINT at={-8,40};
        ok &= SUCCEEDED(xbox_D3D8CopyBackBufferRectToTexture(tex,&source,&at));
        dev->lpVtbl->SetTexture(dev,0,(IDirect3DBaseTexture8*)tex);
        dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,v,sizeof v[0]);
        ok &= native_test_pixel(dev,8,48,0,255,0);
        ok &= native_test_pixel(dev,40,48,0,0,255);
        ok &= native_test_pixel(dev,8,16,255,0,0);
        dev->lpVtbl->SetTexture(dev,0,NULL);tex->lpVtbl->Release(tex);
        /* Disabling clipping restores the entire viewport. */
        dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,v,sizeof v[0]);
        ok &= native_test_pixel(dev,8,48,255,255,255);
    }
    IDirect3DSurface8 *image=NULL,*old_depth=NULL;
    ok &= SUCCEEDED(dev->lpVtbl->GetDepthStencilSurface(dev,&old_depth));
    ok &= SUCCEEDED(dev->lpVtbl->CreateImageSurface(dev,16,16,D3DFMT_LIN_A8R8G8B8,&image));
    if(image) {
        ok &= SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,image,NULL));
        dev->lpVtbl->Clear(dev,0,NULL,D3DCLEAR_TARGET,0xff0000ff,1,0);
        D3DLOCKED_RECT lock;RECT region={4,2,12,6};
        ok &= SUCCEEDED(image->lpVtbl->LockRect(image,&lock,&region,0));
        for(int y=0;y<4;y++)for(int x=0;x<8;x++)((DWORD*)((BYTE*)lock.pBits+y*lock.Pitch))[x]=0xff00ff00;
        D3DLOCKED_RECT second;ok &= FAILED(image->lpVtbl->LockRect(image,&second,NULL,0));
        ok &= SUCCEEDED(image->lpVtbl->UnlockRect(image));
        ok &= SUCCEEDED(image->lpVtbl->LockRect(image,&lock,NULL,D3DLOCK_READONLY));
        ok &= ((DWORD*)((BYTE*)lock.pBits+3*lock.Pitch))[6]==0xff00ff00;
        ok &= ((DWORD*)((BYTE*)lock.pBits+10*lock.Pitch))[6]==0xff0000ff;
        ok &= SUCCEEDED(image->lpVtbl->UnlockRect(image));
        RECT invalid={-1,0,16,16};ok &= FAILED(image->lpVtbl->LockRect(image,&lock,&invalid,0));
        ok &= SUCCEEDED(dev->lpVtbl->SetRenderTarget(dev,NULL,old_depth));
        image->lpVtbl->Release(image);
    }
    if(old_depth)old_depth->lpVtbl->Release(old_depth);
    ok &= pc_display_apply((PcDisplayMode){0,0});
    ok &= xbox_D3D8ScreenCopyCount()==4;
    ok &= glGetError()==GL_NO_ERROR;
    fprintf(stderr,"[native-gl-test] scissor/screen-copy: %s\n",ok?"PASS":"FAIL");return ok;
}
