#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned UINT;
typedef int32_t LONG;
typedef int32_t HRESULT;
typedef struct {LONG left,top,right,bottom;} RECT;
typedef struct {LONG x,y;} POINT;
typedef struct {int value;} IDirect3DTexture8;
static uint32_t memory[128];
#define HLE_MEM32(a) memory[(a)/4]
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
static IDirect3DTexture8 source_texture,target_texture;
static int source_present=1,target_present=1,copies,targets,fail;
static void *g_shadow;
static RECT last_rect;
static POINT last_point;
static IDirect3DTexture8 *hle_d3d8_rendered_texture(uint32_t a){assert(a==1);return source_present?&source_texture:0;}
static IDirect3DTexture8 *hle_d3d8_render_texture(void *d,uint32_t a){assert(d==g_shadow&&a==2);targets++;return target_present?&target_texture:0;}
static HRESULT xbox_D3D8CopyTextureRectToTexture(IDirect3DTexture8 *dst,IDirect3DTexture8 *src,const RECT *r,const POINT *p)
{assert(dst==&target_texture&&src==&source_texture);last_rect=*r;last_point=*p;copies++;return fail?-1:0;}
static int hle_d3d8_interp_rec,hle_d3d8_interp_predicting,recording,deferred;
#define E_INVALIDARG (-2)
#define S_OK 0
static unsigned char queued[256],snapshot[256];
static int hle_d3d8_defer_recording(void){return recording;}
static void hle_d3d8_defer_op(void (*op)(const void *),const void *p,size_t n)
{(void)op;assert(n<=sizeof queued);memcpy(queued,p,n);deferred++;}
static void hle_d3d8_interp_op(void (*op)(const void *),const void *p,size_t n)
{(void)op;assert(n<=sizeof snapshot);memcpy(snapshot,p,n);}
static HRESULT host_CopyTextureRectToTexture(IDirect3DTexture8 *,IDirect3DTexture8 *,const RECT *,const POINT *);
static HRESULT host_CopyTextureRegionScaled(IDirect3DTexture8 *,IDirect3DTexture8 *,const RECT *);
static int scaled_texture_copies;
static RECT scaled_texture_region;
static HRESULT xbox_D3D8CopyTextureRegionScaled(IDirect3DTexture8 *d,IDirect3DTexture8 *s,const RECT *r)
{assert(d==&target_texture && s==&source_texture);scaled_texture_copies++;scaled_texture_region=*r;return S_OK;}
static HRESULT host_CopyBackBufferRectToTexture(IDirect3DTexture8 *d,const RECT *r,const POINT *p){(void)d;(void)r;(void)p;assert(0);return 0;}
static HRESULT host_CopyBackBufferToTexture(IDirect3DTexture8 *d){(void)d;assert(0);return 0;}
static HRESULT host_CopyBackBufferRegionScaled(IDirect3DTexture8 *,const RECT *,const RECT *);
static int region_copies;
static RECT region_from,region_to;
static HRESULT xbox_D3D8CopyBackBufferRegionScaled(IDirect3DTexture8 *d,const RECT *r,const RECT *to)
{assert(d==&target_texture);region_copies++;region_from=*r;region_to=*to;return S_OK;}
/* RECORDING */
/* PRODUCTION */
static uint32_t args[5],g_backbuffer_va=0x900,g_shadow_width=640,g_shadow_height=480;
static unsigned long g_shadow_swaps;
#define HLE_ARG(i) args[i]
#define HLE_EXPORT(n) void hle_##n(void)
#define HLE_CALL_ORIGINAL(n) hle_original_##n()
#define HLE_RETURN(n) return
#define SURFACE_PARENT 32
static void first_call(int *seen,const char *name,uint32_t a){(void)seen;(void)name;(void)a;}
static int original_missing(void (*fn)(void),const char *name){(void)name;return !fn;}
static int shadow_trace_on(void){return 0;}
static void note_framebuffer_phys(uint32_t data){(void)data;}
static void surface_measure(uint32_t a,UINT *w,UINT *h,uint32_t *fmt){*w=a==256?640:256;*h=a==256?480:256;*fmt=0x12;}
static void native_copy(void){args[1]=0;args[2]=0;args[4]=0;}
static void (*const hle_original_D3DDevice_CopyRects)(void)=native_copy;
/* WRAPPER */
int main(void)
{
    /* The map capture executes in queue order and owns both rectangle values. */
    RECT from={212,0,314,181},to={128,0,309,181};
    recording=1;
    assert(host_CopyBackBufferRegionScaled(&target_texture,&from,&to)==S_OK);
    assert(region_copies==0 && ((dq_screen_copy *)queued)->has_rect==4);
    from.left=999;to.left=999;recording=0;op_screen_copy(queued);
    assert(region_copies==1 && region_from.left==212 && region_to.left==128);
    /* The saved corner must also reach the handheld only after queued draws. */
    RECT corner={0,0,128,128};recording=1;
    assert(host_CopyTextureRegionScaled(&target_texture,&source_texture,&corner)==S_OK);
    assert(scaled_texture_copies==0 && ((dq_screen_copy *)queued)->has_rect==2);
    corner.right=99;recording=0;op_screen_copy(queued);
    assert(scaled_texture_copies==1 && scaled_texture_region.right==128);
    deferred=0;
    /* Native pause takes a 160x120 subrectangle into a 256x256 picture. */
    HLE_MEM32(64)=320;HLE_MEM32(68)=240;HLE_MEM32(72)=480;HLE_MEM32(76)=360;
    HLE_MEM32(128)=0;HLE_MEM32(132)=0;
    assert(shadow_copy_texture_rects(1,2,640,480,256,256,64,1,128));
    assert(copies==1&&last_rect.left==320&&last_rect.top==240&&last_rect.right==480&&last_rect.bottom==360);
    assert(last_point.x==0&&last_point.y==0);
    /* NULL points mean the source's original coordinates, not (0,0). */
    assert(shadow_copy_texture_rects(1,2,640,480,256,256,64,1,0));
    assert(last_point.x==320&&last_point.y==240);
    assert(shadow_copy_texture_rects(1,2,160,120,256,256,0,0,0));
    assert(last_rect.left==0&&last_rect.top==0&&last_rect.right==160&&last_rect.bottom==120);
    assert(shadow_copy_texture_rects(1,2,640,480,256,256,0,0,0));
    assert(last_rect.right==256&&last_rect.bottom==256);
    int before=copies,allocated=targets;
    source_present=0;assert(!shadow_copy_texture_rects(1,2,640,480,256,256,64,1,128));
    assert(copies==before&&targets==allocated);source_present=1;
    assert(!shadow_copy_texture_rects(1,2,640,480,256,256,0,1,0));
    assert(!shadow_copy_texture_rects(1,2,640,480,256,256,64,17,0));
    assert(copies==before&&targets==allocated);
    target_present=0;assert(!shadow_copy_texture_rects(1,2,640,480,256,256,64,1,128));
    assert(copies==before);target_present=1;
    recording=1;
    assert(shadow_copy_texture_rects(1,2,640,480,256,256,64,1,128));
    assert(copies==before&&deferred==1);
    HLE_MEM32(64)=99; /* Guest memory may change before the GPU queue executes. */
    recording=0;op_screen_copy(queued);
    assert(copies==before+1&&last_rect.left==320);
    hle_d3d8_interp_rec=1;
    assert(shadow_copy_texture_rects(1,2,640,480,256,256,64,1,128));
    assert(((dq_screen_copy *)snapshot)->has_rect==3);
    assert(((dq_screen_copy *)snapshot)->src.left==99);
    hle_d3d8_interp_rec=0;
    fail=1;assert(!shadow_copy_texture_rects(1,2,640,480,256,256,64,1,128));
    fail=0;HLE_MEM32(64)=320;g_shadow=&source_texture;
    HLE_MEM32(256+SURFACE_PARENT)=1;HLE_MEM32(320+SURFACE_PARENT)=2;
    HLE_MEM32(1+4)=HLE_MEM32(2+4)=123;
    HLE_MEM32(256+4)=HLE_MEM32(320+4)=123;
    args[0]=256;args[1]=64;args[2]=1;args[3]=320;args[4]=128;
    before=copies;hle_D3DDevice_CopyRects();
    assert(copies==before+1&&last_rect.left==320&&last_rect.top==240&&last_rect.right==480);
    assert(last_point.x==0&&last_point.y==0&&args[2]==0);
    return 0;
}
