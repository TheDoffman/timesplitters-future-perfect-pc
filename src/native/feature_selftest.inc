#include <unistd.h>

/* Small synthetic pack: no original artwork or personal files are modified. */
static int native_test_pack(IDirect3DDevice8 *dev) {
    const char *root=getenv("RECOMP_SAVE_DIR");if(!root)return 0;
    unsigned char original[32]={0},dds[184]={0};
    uint32_t header[32]={0};header[0]=0x20534444;header[1]=124;header[3]=8;header[4]=8;
    header[7]=4;header[19]=32;header[20]=4;header[21]=0x31545844;
    memcpy(dds,header,128);
    for(int at=128;at<184;at+=8){dds[at]=0;dds[at+1]=0xf8;}
    uint64_t key=tsfp_texture_key(D3DFMT_DXT1,8,8,original,sizeof original);
    char path[4096];snprintf(path,sizeof path,"%s/%016" PRIx64 ".dds",root,key);
    FILE *f=fopen(path,"wb");if(!f)return 0;
    int ok=fwrite(dds,1,sizeof dds,f)==sizeof dds;fclose(f);
    TsfpDDS parsed;unsigned char *data=tsfp_pack_read(path,8,8,56,&parsed);
    ok &= data!=NULL;free(data);
    data=tsfp_pack_read(path,8,8,55,&parsed);ok &= !data;free(data);
    data=tsfp_pack_read(path,8,4,1000,&parsed);ok &= !data;free(data);
    data=tsfp_pack_read(path,16,16,1000,&parsed);ok &= !data;free(data);
    setenv("TSFP_TEXTURE_PACK_DIR",root,1);
    IDirect3DTexture8 *texture=NULL;D3DLOCKED_RECT lock;
    ok &= SUCCEEDED(dev->lpVtbl->CreateTexture(dev,8,8,1,0,D3DFMT_DXT1,D3DPOOL_MANAGED,&texture));
    if(texture){
        ok &= SUCCEEDED(texture->lpVtbl->LockRect(texture,0,&lock,NULL,0));
        memcpy(lock.pBits,original,sizeof original);texture->lpVtbl->UnlockRect(texture,0);
        GLTexture *t=(GLTexture *)texture;
        ok &= t->pack_texture!=0 && native_pack_resident==56;
        /* Sample the replacement through the actual draw binding. */
        struct {float x,y,z,w;DWORD color;float u,v;} quad[4]={
            {8,8,0,1,0xffffffff,0,0},{56,8,0,1,0xffffffff,1,0},
            {56,56,0,1,0xffffffff,1,1},{8,56,0,1,0xffffffff,0,1}};
        dev->lpVtbl->SetTexture(dev,0,(IDirect3DBaseTexture8 *)texture);
        dev->lpVtbl->DrawPrimitiveUP(dev,D3DPT_QUADLIST,1,quad,sizeof quad[0]);
        ok &= native_test_pixel(dev,32,32,255,0,0);
        dev->lpVtbl->SetTexture(dev,0,NULL);texture->lpVtbl->Release(texture);
        ok &= native_pack_resident==0;
    }
    /* Corrupt packs must leave the original texture usable. */
    f=fopen(path,"wb");if(f){fwrite("bad",1,3,f);fclose(f);}else ok=0;
    texture=NULL;
    ok &= SUCCEEDED(dev->lpVtbl->CreateTexture(dev,8,8,1,0,D3DFMT_DXT1,D3DPOOL_MANAGED,&texture));
    if(texture){texture->lpVtbl->LockRect(texture,0,&lock,NULL,0);memcpy(lock.pBits,original,32);texture->lpVtbl->UnlockRect(texture,0);
        ok &= !((GLTexture *)texture)->pack_texture;texture->lpVtbl->Release(texture);}
    unsetenv("TSFP_TEXTURE_PACK_DIR");unlink(path);
    fprintf(stderr,"[native-feature-test] shared DDS validation, budget/aspect rejection, GPU replacement and corrupt fallback: %s\n",ok?"PASS":"FAIL");
    return ok;
}
