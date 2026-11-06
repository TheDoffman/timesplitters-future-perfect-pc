#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef uint32_t DWORD;typedef unsigned UINT;typedef int HRESULT;
typedef int D3DPRIMITIVETYPE,D3DRENDERSTATETYPE,D3DTEXTURESTAGESTATETYPE;
#define S_OK 0
#define E_FAIL (-1)
#define FAILED(h) ((h)<0)
#define TRUE 1
#define FALSE 0
enum {D3DRS_ALPHABLENDENABLE=1,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_ALPHATESTENABLE,D3DRS_BLENDOP};
enum {D3DTSS_COLOROP=1,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_MINFILTER,D3DTSS_MAGFILTER,D3DTSS_MIPFILTER,D3DTSS_ADDRESSU,D3DTSS_ADDRESSV,D3DTSS_TEXCOORDINDEX,D3DTSS_MAXMIPLEVEL,D3DTSS_MIPMAPLODBIAS};
enum {D3DBLEND_ONE=1,D3DBLEND_INVSRCALPHA=6,D3DTOP_MODULATE=4,D3DTOP_DISABLE=1,D3DTA_TEXTURE=2,D3DTA_DIFFUSE=0,D3DTEXF_POINT=1,D3DTEXF_LINEAR=2,D3DTADDRESS_CLAMP=3,D3DFMT_A8R8G8B8=6,D3DPOOL_MANAGED=1};
typedef struct Texture Texture;typedef Texture IDirect3DTexture8,IDirect3DBaseTexture8;
typedef struct {unsigned (*Release)(Texture *);} TextureVtable;
struct Texture {TextureVtable *lpVtbl;uint32_t *levels[8];unsigned edge;};
typedef struct {int Pitch;void *pBits;} D3DLOCKED_RECT;
typedef struct Device IDirect3DDevice8;
typedef struct {HRESULT (*GetTexture)(IDirect3DDevice8 *,unsigned,Texture **);HRESULT (*GetRenderState)(IDirect3DDevice8 *,int,DWORD *);HRESULT (*GetTextureStageState)(IDirect3DDevice8 *,unsigned,int,DWORD *);} DeviceVtable;
struct Device {DeviceVtable *lpVtbl;};
static DWORD rs[32],ts[2][32],shader=73;
static unsigned creates,releases,draws,locks,unlocks;
static int fail_lock=-1,fail_get,fail_draw;
static Texture old_texture,*bound=&old_texture;
static unsigned release(Texture *t){if(t!=&old_texture){for(int i=0;i<8;i++)free(t->levels[i]);free(t);releases++;}return 1;}
static TextureVtable texture_vtable={release};
static HRESULT get_texture(IDirect3DDevice8 *d,unsigned stage,Texture **out){(void)d;CHECK(stage==0);*out=bound;return S_OK;}
static HRESULT get_rs(IDirect3DDevice8 *d,int state,DWORD *out){(void)d;if(fail_get)return E_FAIL;*out=rs[state];return S_OK;}
static HRESULT get_ts(IDirect3DDevice8 *d,unsigned stage,int type,DWORD *out){(void)d;*out=ts[stage][type];return S_OK;}
static DeviceVtable device_vtable={get_texture,get_rs,get_ts};
static IDirect3DDevice8 device={&device_vtable},second={&device_vtable},*current=&device;
static IDirect3DDevice8 *hle_d3d8_shadow_device(void){return current;}
static unsigned d3d8_up_vertices_read(int type,unsigned prims){return type==8?prims*4:prims+2;}
static HRESULT host_CreateTexture(IDirect3DDevice8 *d,unsigned w,unsigned h,unsigned levels,unsigned usage,int format,int pool,Texture **out){(void)d;(void)usage;CHECK(w==h && w<=512 && levels==1 && format==6 && pool==1);*out=calloc(1,sizeof **out);CHECK(*out);(*out)->lpVtbl=&texture_vtable;(*out)->edge=w;(*out)->levels[0]=calloc(w*h,4);creates++;return S_OK;}
static HRESULT host_LockRect(Texture *t,unsigned level,D3DLOCKED_RECT *out,const void *r,unsigned flags){(void)r;(void)flags;if((int)level==fail_lock)return E_FAIL;out->Pitch=t->edge*4;out->pBits=t->levels[level];locks++;return S_OK;}
static HRESULT host_UnlockRect(Texture *t,unsigned level){(void)t;(void)level;unlocks++;return S_OK;}
static unsigned host_ReleaseTexture(Texture *t){return release(t);}
static HRESULT host_SetTexture(IDirect3DDevice8 *d,unsigned stage,Texture *t){(void)d;CHECK(stage==0);bound=t;if(!t)ts[0][D3DTSS_COLOROP]=D3DTOP_DISABLE;else if(ts[0][D3DTSS_COLOROP]==D3DTOP_DISABLE)ts[0][D3DTSS_COLOROP]=D3DTOP_MODULATE;return S_OK;}
static HRESULT host_SetRenderState(IDirect3DDevice8 *d,int state,DWORD value){(void)d;rs[state]=value;return S_OK;}
static HRESULT host_SetTextureStageState(IDirect3DDevice8 *d,unsigned stage,int type,DWORD value){(void)d;ts[stage][type]=value;return S_OK;}
static DWORD d3d8_combiners_get_pixel_shader(void){return shader;}
static void host_combiners_set_pixel_shader(DWORD value){shader=value;}
static HRESULT host_DrawPrimitiveUP(IDirect3DDevice8 *,int,unsigned,const void *,unsigned);
static int deferred;static void (*queued_fn)(const void *);static union {uint64_t alignment;unsigned char bytes[512];} queued;
static int hle_d3d8_defer_recording(void){return deferred;}
static void hle_d3d8_defer_op(void (*fn)(const void *),const void *arg,size_t size){CHECK(size<=sizeof queued.bytes);memcpy(queued.bytes,arg,size);queued_fn=fn;}
static unsigned flushes;
static void hle_d3d8_defer_flush(void){flushes++;if(queued_fn){queued_fn(queued.bytes);queued_fn=NULL;}}
#include "../src/interface/pc_input_icon_gpu.inc"
static HRESULT host_DrawPrimitiveUP(IDirect3DDevice8 *d,int type,unsigned prims,const void *vertices,unsigned stride)
{
 if(input_gpu_active)return input_gpu_dispatch(d,type,prims,vertices,stride);
 CHECK(bound!=&old_texture && shader==0);
 CHECK(rs[D3DRS_SRCBLEND]==D3DBLEND_ONE && rs[D3DRS_DESTBLEND]==D3DBLEND_INVSRCALPHA);
 CHECK(ts[0][D3DTSS_MINFILTER]==D3DTEXF_POINT && ts[0][D3DTSS_MAGFILTER]==D3DTEXF_POINT && ts[0][D3DTSS_MIPFILTER]==0);
 CHECK(bound->levels[0][0]==0x80643214);
 const unsigned char *bytes=vertices;
 float uv[2];uint32_t color;
 memcpy(uv,bytes+stride-8,8);memcpy(&color,bytes+stride-12,4);
 CHECK(uv[0]==0.f && uv[1]==0.f && color==0xfefefeff);
 memcpy(uv,bytes+3*stride+stride-8,8);CHECK(uv[0]==input_gpu_uvmax && uv[1]==input_gpu_uvmax);
 draws++;return fail_draw?E_FAIL:S_OK;
}
int main(void)
{
 old_texture.lpVtbl=&texture_vtable;
 for(int i=0;i<32;i++){rs[i]=100+i;ts[0][i]=200+i;ts[1][i]=300+i;}
 DWORD before_rs[32],before_ts[2][32];memcpy(before_rs,rs,sizeof rs);memcpy(before_ts,ts,sizeof ts);
 InputGpuSpan source[]={{0,0,64,128,200,100,40,128}};
 CHECK(tsfp_input_icon_begin(source,1,128,128));CHECK(creates==1 && locks==1 && unlocks==1);
 struct Vertex {float x,y,z;uint32_t color;float u,v;} v[4]={{0,0,1,0x7f7f7f80,0,0},{10,0,1,0x7f7f7f80,2,0},{0,10,1,0x7f7f7f80,0,3},{10,10,1,0x7f7f7f80,2,3}};
 CHECK(host_DrawPrimitiveUP(&device,0,2,v,sizeof v[0])==S_OK);CHECK(draws==1 && bound==&old_texture && shader==73);
 CHECK(!memcmp(rs,before_rs,sizeof rs) && !memcmp(ts,before_ts,sizeof ts));CHECK(v[3].u==2 && v[3].v==3 && v[0].color==0x7f7f7f80);
 ts[0][D3DTSS_COLOROP]=D3DTOP_DISABLE;before_ts[0][D3DTSS_COLOROP]=D3DTOP_DISABLE;
 CHECK(host_DrawPrimitiveUP(&device,8,1,v,sizeof v[0])==S_OK);CHECK(!memcmp(ts,before_ts,sizeof ts));
 fail_draw=1;CHECK(host_DrawPrimitiveUP(&device,0,2,v,sizeof v[0])==E_FAIL);fail_draw=0;
 CHECK(!memcmp(rs,before_rs,sizeof rs) && !memcmp(ts,before_ts,sizeof ts));
 fail_get=1;unsigned before=draws;CHECK(host_DrawPrimitiveUP(&device,0,2,v,sizeof v[0])==E_FAIL);CHECK(draws==before);fail_get=0;
 deferred=1;before=draws;CHECK(host_DrawPrimitiveUP(&device,0,2,v,sizeof v[0])==S_OK);CHECK(draws==before && queued_fn);tsfp_input_icon_end();
 rs[2]=777;before_rs[2]=777;deferred=0;queued_fn(queued.bytes);CHECK(draws==before+1 && !memcmp(rs,before_rs,sizeof rs));
 tsfp_input_icon_end();CHECK(tsfp_input_icon_begin(source,1,128,128) && creates==1);tsfp_input_icon_end();
 InputGpuSpan invalid[]={{127,0,2,128,1,2,3,4}};CHECK(!tsfp_input_icon_begin(invalid,1,128,128));
 current=NULL;CHECK(!tsfp_input_icon_begin(source,1,128,128));current=&second;
 fail_lock=0;CHECK(!tsfp_input_icon_begin(source,1,128,128));CHECK(releases==2);fail_lock=-1;
 CHECK(tsfp_input_icon_begin(source,1,128,128));tsfp_input_icon_end();
 CHECK(tsfp_input_icon_begin(source,1,128,20));CHECK(input_gpu_uvmax==.625f && input_gpu_active->edge==32);
 CHECK(input_gpu_active->levels[0][input_gpu_morton(9,19,32)]==0x80643214);
 CHECK(input_gpu_active->levels[0][input_gpu_morton(10,19,32)]==0);
 CHECK(input_gpu_active->levels[0][input_gpu_morton(20,0,32)]==0);
 CHECK(host_DrawPrimitiveUP(&second,8,1,v,sizeof v[0])==S_OK);tsfp_input_icon_end();
 unsigned cached=creates;CHECK(tsfp_input_icon_begin(source,1,128,20) && creates==cached);tsfp_input_icon_end();
 CHECK(tsfp_input_icon_begin(source,1,128,21));
 /* Center texel straddles the source boundary with equal coverage. */
 CHECK(input_gpu_active->levels[0][input_gpu_morton(10,10,32)]==0x4032190a);
 deferred=1;CHECK(host_DrawPrimitiveUP(&second,8,1,v,sizeof v[0])==S_OK);tsfp_input_icon_end();
 CHECK(tsfp_input_icon_begin(source,1,128,64));tsfp_input_icon_end();deferred=0;
 queued_fn(queued.bytes);queued_fn=NULL;CHECK(!input_gpu_active);
 input_gpu_clear();CHECK(releases==creates);
 CHECK(!tsfp_input_icon_begin(source,1,128,0) && !tsfp_input_icon_begin(source,1,128,513));
 /* Every new physical size has a separate bitmap. Exhaustion flushes queued
  * draws before recycling textures, including sizes larger than source art. */
 for(unsigned n=1;n<=256;n++) {CHECK(tsfp_input_icon_begin(source,1,128,n));tsfp_input_icon_end();}
 CHECK(tsfp_input_icon_begin(source,1,128,128));deferred=1;
 CHECK(host_DrawPrimitiveUP(&second,8,1,v,sizeof v[0])==S_OK);tsfp_input_icon_end();deferred=0;
 before=draws;CHECK(tsfp_input_icon_begin(source,1,128,257));CHECK(flushes==1 && draws==before+1);
 tsfp_input_icon_end();input_gpu_clear();CHECK(releases==creates);
 puts("PASS: original 128px texture, size-matched coverage, point sampling without mips, cached upload, native geometry/color and full state restoration on success/failure");return 0;
}
