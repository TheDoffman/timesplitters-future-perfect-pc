#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#undef assert
#define assert(test) do {if(!(test)){fprintf(stderr,"FAIL: %s line %d\n",#test,__LINE__);exit(1);}} while(0)
typedef struct RhiOcclusion {uint64_t pixels;int active,ready;} RhiOcclusion;
typedef struct RhiView {int unused;} RhiView;
typedef struct D3D8Surface {int unused;} D3D8Surface;
typedef struct RhiImageDesc {unsigned samples;} RhiImageDesc;
static D3D8Surface test_surface,*g_cur_rt;
static struct {D3D8Surface *scene_surface;} g_device_state={&test_surface};
static RhiView test_view;
static RhiOcclusion *active,*last;
static unsigned created,destroyed;
static RhiView *d3d8_surface_draw_view(D3D8Surface *s){(void)s;return &test_view;}
static void *rhi_view_image(RhiView *v){return v;}
static void rhi_image_get_desc(void *p,RhiImageDesc *d){(void)p;d->samples=4;}
static double rt_scale_x(void){return 1;}
static double rt_scale_y(void){return 1;}
static RhiOcclusion *rhi_occlusion_create(void){created++;return calloc(1,sizeof(RhiOcclusion));}
static void rhi_occlusion_destroy(RhiOcclusion *q){assert(active!=q);destroyed++;free(q);}
static int rhi_occlusion_begin(RhiOcclusion *q){assert(!active);active=last=q;q->active=1;q->pixels=0;q->ready=1;return 1;}
static int rhi_occlusion_end(RhiOcclusion *q){assert(active==q);q->active=0;active=NULL;return 1;}
static int rhi_occlusion_result(RhiOcclusion *q,uint64_t *p){if(!q->ready)return 0;*p=q->pixels;return 1;}
static void gpu_draw(uint64_t samples){if(active)active->pixels+=samples;}
#include "graphics/visibility_rhi_device.inc"
int main(void)
{
    uint32_t pixels=77;
    assert(xbox_D3D8VisibilityBegin());
    /* A feedback snapshot is renderer preparation, not visibility geometry.
       The old broad native scope counted 1,024,000 extra normalized samples. */
    gpu_draw(4096000);
    visibility_guest_draw_begin();gpu_draw(680);visibility_guest_draw_end();
    assert(last);
    last->ready=0;
    xbox_D3D8VisibilityEnd(12,1);
    assert(xbox_D3D8VisibilityResult(12,&pixels)==0 && pixels==77);
    last->ready=1;
    assert(xbox_D3D8VisibilityResult(12,&pixels)==1 && pixels==170);
    /* Multiple guest draws may need unrelated host work between them. */
    assert(xbox_D3D8VisibilityBegin());
    visibility_guest_draw_begin();gpu_draw(40);visibility_guest_draw_end();
    gpu_draw(123456);
    visibility_guest_draw_begin();gpu_draw(80);visibility_guest_draw_end();
    last->ready=0;
    xbox_D3D8VisibilityEnd(13,1);
    pixels=99;assert(xbox_D3D8VisibilityResult(13,&pixels)==0 && pixels==99);
    last->ready=1;
    assert(xbox_D3D8VisibilityResult(13,&pixels)==1 && pixels==30);
    assert(xbox_D3D8VisibilityResult(12,&pixels)==1 && pixels==170);
    /* An empty scope is genuinely zero, and reset releases retained segments. */
    assert(xbox_D3D8VisibilityBegin());gpu_draw(123456);
    xbox_D3D8VisibilityEnd(14,1);
    assert(xbox_D3D8VisibilityResult(14,&pixels)==1 && !pixels);
    assert(xbox_D3D8VisibilityBegin());
    for(unsigned i=0;i<=TSFP_VISIBILITY_DRAW_PARTS;i++) {
        visibility_guest_draw_begin();gpu_draw(4);visibility_guest_draw_end();
    }
    xbox_D3D8VisibilityEnd(15,1);pixels=99;
    assert(xbox_D3D8VisibilityResult(15,&pixels)==-1 && pixels==99);
    device_visibility_reset();assert(!active && created==destroyed);
    puts("PASS: exact guest samples exclude feedback draws; async, multi-draw, scale/cache/reset");
    return 0;
}
