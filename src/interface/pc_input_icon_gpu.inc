/* Included once at the common HLE host boundary. Native quads supply positions,
 * placement, tint and clipping; this scope supplies a filtered original image. */
#define TSFP_INPUT_ICON_GPU 1
typedef struct InputGpuSpan { unsigned char x,y,w,h,r,g,b,a; } InputGpuSpan;
typedef struct InputGpuImage { const void *key; IDirect3DTexture8 *texture; } InputGpuImage;
static InputGpuImage input_gpu_images[256];
static IDirect3DDevice8 *input_gpu_device;
#ifdef _MSC_VER
static __declspec(thread) IDirect3DTexture8 *input_gpu_active;
#else
static _Thread_local IDirect3DTexture8 *input_gpu_active;
#endif
static void input_gpu_clear(void)
{
    for(unsigned i=0;i<256;i++)if(input_gpu_images[i].texture) {
        host_ReleaseTexture(input_gpu_images[i].texture);
        memset(&input_gpu_images[i],0,sizeof input_gpu_images[i]);
    }
    input_gpu_active=NULL;
}
/* Xbox A8R8G8B8 lock bytes are Morton ordered, including each mip.
 * The shared upload layer unswizzles them before sending them to the GPU. */
static unsigned input_gpu_morton(unsigned x,unsigned y,unsigned width)
{
    unsigned out=0,shift=0;
    for(unsigned bit=1;bit<width;bit<<=1,shift+=2) {
        if(x&bit)out|=1u<<shift;
        if(y&bit)out|=2u<<shift;
    }
    return out;
}
int tsfp_input_icon_begin(const void *source,unsigned count,unsigned size)
{
    IDirect3DDevice8 *dev=hle_d3d8_shadow_device();
    if(!dev || !dev->lpVtbl || !dev->lpVtbl->GetTexture || !dev->lpVtbl->GetRenderState ||
       !dev->lpVtbl->GetTextureStageState || !source || !count || size!=128 || input_gpu_active)return 0;
    if(dev!=input_gpu_device) {input_gpu_clear();input_gpu_device=dev;}
    unsigned slot;
    for(slot=0;slot<256;slot++) {
        if(input_gpu_images[slot].key==source) {input_gpu_active=input_gpu_images[slot].texture;return 1;}
        if(!input_gpu_images[slot].key)break;
    }
    if(slot==256)return 0;
    uint32_t *pixels=calloc(size*size,sizeof *pixels);if(!pixels)return 0;
    const InputGpuSpan *spans=source;
    for(unsigned i=0;i<count;i++) {
        const InputGpuSpan *s=spans+i;
        if(!s->w || !s->h || s->x+s->w>size || s->y+s->h>size) {free(pixels);return 0;}
        /* Premultiplied mipmaps avoid dark/colored fringes on transparent edges. */
        uint32_t color=((uint32_t)s->a<<24)|((s->r*s->a+127u)/255u<<16)|
                       ((s->g*s->a+127u)/255u<<8)|(s->b*s->a+127u)/255u;
        for(unsigned y=s->y;y<s->y+s->h;y++)for(unsigned x=s->x;x<s->x+s->w;x++)pixels[y*size+x]=color;
    }
    IDirect3DTexture8 *texture=NULL;
    if(FAILED(host_CreateTexture(dev,size,size,8,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture))) {free(pixels);return 0;}
    unsigned width=size;int success=1;
    for(unsigned level=0;level<8;level++) {
        D3DLOCKED_RECT lock;
        if(FAILED(host_LockRect(texture,level,&lock,NULL,0))) {success=0;break;}
        for(unsigned y=0;y<width;y++)for(unsigned x=0;x<width;x++)
            ((uint32_t *)lock.pBits)[input_gpu_morton(x,y,width)]=pixels[y*width+x];
        if(FAILED(host_UnlockRect(texture,level))) {success=0;break;}
        unsigned next=width/2;
        for(unsigned y=0;y<next;y++)for(unsigned x=0;x<next;x++) {
            unsigned at=y*2*width+x*2;uint32_t color=0;
            for(unsigned shift=0;shift<32;shift+=8) {
                unsigned sum=((pixels[at]>>shift)&255)+((pixels[at+1]>>shift)&255)+
                    ((pixels[at+width]>>shift)&255)+((pixels[at+width+1]>>shift)&255);
                color|=((sum+2)/4)<<shift;
            }
            pixels[y*next+x]=color;
        }
        width=next;
    }
    free(pixels);
    if(!success) {host_ReleaseTexture(texture);return 0;}
    input_gpu_images[slot].key=source;input_gpu_images[slot].texture=texture;input_gpu_active=texture;
    return 1;
}
void tsfp_input_icon_end(void) {input_gpu_active=NULL;}
static HRESULT input_gpu_draw(IDirect3DDevice8 *dev,D3DPRIMITIVETYPE type,UINT prims,const void *vertices,UINT stride)
{
    unsigned count=d3d8_up_vertices_read(type,prims);
    /* Native textured UI vertices are XYZ+RGBA+UV (or XYZRHW+RGBA+UV).
     * Retain every position, including the title's half-pixel convention. */
    if(!vertices || (stride!=24 && stride!=28) || count<3 || count>6)return E_FAIL;
    unsigned char copy[6*28];memcpy(copy,vertices,count*stride);
    float u0=1e30f,v0=1e30f,u1=-1e30f,v1=-1e30f;
    for(unsigned i=0;i<count;i++) {
        float uv[2];memcpy(uv,copy+i*stride+stride-8,8);
        if(!isfinite(uv[0]) || !isfinite(uv[1]))return E_FAIL;
        u0=fminf(u0,uv[0]);v0=fminf(v0,uv[1]);u1=fmaxf(u1,uv[0]);v1=fmaxf(v1,uv[1]);
    }
    if(u1<=u0 || v1<=v0)return E_FAIL;
    for(unsigned i=0;i<count;i++) {
        float uv[2];uint32_t color;unsigned char *v=copy+i*stride;
        memcpy(uv,v+stride-8,8);uv[0]=(uv[0]-u0)/(u1-u0);uv[1]=(uv[1]-v0)/(v1-v0);memcpy(v+stride-8,uv,8);
        memcpy(&color,v+stride-12,4);
        /* Native UI white/opaque is0x7f, expanded by its original combiner.
         * Our plain textured pass must apply that same seven-bit convention. */
        unsigned a=(color&255)*2u;if(a>255)a=255;
        uint32_t expanded=a;
        for(unsigned shift=8;shift<32;shift+=8) {
            unsigned channel=((color>>shift)&255)*2u;if(channel>255)channel=255;
            expanded|=((channel*a+127)/255)<<shift;
        }
        color=expanded;
        memcpy(v+stride-12,&color,4);
    }
    static const D3DRENDERSTATETYPE rs[]={D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_ALPHATESTENABLE,D3DRS_BLENDOP};
    static const DWORD rv[]={TRUE,D3DBLEND_ONE,D3DBLEND_INVSRCALPHA,FALSE,1u /* D3DBLENDOP_ADD */};
    static const D3DTEXTURESTAGESTATETYPE ts[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_MINFILTER,D3DTSS_MAGFILTER,D3DTSS_MIPFILTER,D3DTSS_ADDRESSU,D3DTSS_ADDRESSV,(D3DTEXTURESTAGESTATETYPE)24 /* texture transform flags */,D3DTSS_TEXCOORDINDEX,D3DTSS_MAXMIPLEVEL,D3DTSS_MIPMAPLODBIAS};
    static const DWORD tv[]={D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTADDRESS_CLAMP,D3DTADDRESS_CLAMP,0u /* D3DTTFF_DISABLE */,0,0,0};
    DWORD saved_rs[5],saved_ts[15],stage1,shader=d3d8_combiners_get_pixel_shader();
    IDirect3DBaseTexture8 *old=NULL;
    if(FAILED(dev->lpVtbl->GetTexture(dev,0,&old)))return E_FAIL;
    /* Read everything before changing state; failure leaves the device alone. */
    for(unsigned i=0;i<5;i++)if(FAILED(dev->lpVtbl->GetRenderState(dev,rs[i],saved_rs+i)))goto unreadable;
    for(unsigned i=0;i<15;i++)if(FAILED(dev->lpVtbl->GetTextureStageState(dev,0,ts[i],saved_ts+i)))goto unreadable;
    if(FAILED(dev->lpVtbl->GetTextureStageState(dev,1,D3DTSS_COLOROP,&stage1)))goto unreadable;
    IDirect3DTexture8 *image=input_gpu_active;input_gpu_active=NULL;
    host_SetTexture(dev,0,(IDirect3DBaseTexture8 *)image);host_combiners_set_pixel_shader(0);
    for(unsigned i=0;i<5;i++)host_SetRenderState(dev,rs[i],rv[i]);
    for(unsigned i=0;i<15;i++)host_SetTextureStageState(dev,0,ts[i],tv[i]);
    host_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    HRESULT result=host_DrawPrimitiveUP(dev,type,prims,copy,stride);
    /* SetTexture can enable COLOROP; restore the saved stages after binding. */
    host_SetTexture(dev,0,old);host_combiners_set_pixel_shader(shader);
    host_SetTextureStageState(dev,1,D3DTSS_COLOROP,stage1);
    for(unsigned i=0;i<15;i++)host_SetTextureStageState(dev,0,ts[i],saved_ts[i]);
    for(unsigned i=0;i<5;i++)host_SetRenderState(dev,rs[i],saved_rs[i]);
    if(old)old->lpVtbl->Release(old);
    input_gpu_active=image;return result;
unreadable:
    if(old)old->lpVtbl->Release(old);
    return E_FAIL;
}

/* Read/restore device state when the draw executes, not while earlier native
 * state changes are still in the deferred queue. The queue owns its vertices. */
typedef struct InputGpuCall {
    IDirect3DDevice8 *device;IDirect3DTexture8 *texture;
    D3DPRIMITIVETYPE type;unsigned prims,stride;unsigned char vertices[6*28];
} InputGpuCall;
static void input_gpu_execute(const void *arg)
{
    const InputGpuCall *p=arg;IDirect3DTexture8 *saved=input_gpu_active;
    input_gpu_active=p->texture;
    input_gpu_draw(p->device,p->type,p->prims,p->vertices,p->stride);
    input_gpu_active=saved;
}
static HRESULT input_gpu_dispatch(IDirect3DDevice8 *dev,D3DPRIMITIVETYPE type,unsigned prims,const void *vertices,unsigned stride)
{
    if(hle_d3d8_defer_recording()) {
        unsigned count=d3d8_up_vertices_read(type,prims);
        if(!vertices || (stride!=24 && stride!=28) || count<3 || count>6)return E_FAIL;
        InputGpuCall p={0};p.device=dev;p.texture=input_gpu_active;p.type=type;p.prims=prims;p.stride=stride;
        memcpy(p.vertices,vertices,count*stride);
        hle_d3d8_defer_op(input_gpu_execute,&p,sizeof p);return S_OK;
    }
    return input_gpu_draw(dev,type,prims,vertices,stride);
}
