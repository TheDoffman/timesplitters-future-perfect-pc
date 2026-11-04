/* Included by the device after its target/state helpers. */
#include "graphics/ssao_pass.h"
#include "graphics/ssao_projection.h"
#include "graphics/ssao_settings.h"
#include "graphics/graphics_settings.h"
static TsfpSsaoPass g_scene_ao;
void xbox_D3D8AmbientOcclusion(const float *projection)
{
    D3D8DeviceState *s=&g_device_state;
    float inverse[16];
    if(!pc_graphics_get().ambient_occlusion || !g_device_initialized || g_cur_rt || !g_cur_ds ||
       !tsfp_ssao_inverse(projection,inverse))return;
    if(!s->present_width||!s->present_height)return;
    unsigned x=(unsigned)((uint64_t)s->viewport.X*s->width/s->present_width);
    unsigned y=(unsigned)((uint64_t)s->viewport.Y*s->height/s->present_height);
    unsigned right=(unsigned)(((uint64_t)s->viewport.X+s->viewport.Width)*s->width/s->present_width);
    unsigned bottom=(unsigned)(((uint64_t)s->viewport.Y+s->viewport.Height)*s->height/s->present_height);
    if(right<=x||bottom<=y||right>s->width||bottom>s->height)return;
    unsigned width=right-x,height=bottom-y;
    RhiView *dsv=draw_depth_view(g_cur_ds);if(!dsv)return;
    RhiImage *image=rhi_view_image(dsv);RhiImageDesc desc;
    rhi_image_get_desc(image,&desc);
    if(desc.width!=s->width||desc.height!=s->height||!(desc.bind&RHI_BIND_SAMPLED))return;
    RhiView *depth=rhi_view_create(image,RHI_VIEW_SAMPLED,NULL);if(!depth)return;
    if(tsfp_ssao_render_region(&g_scene_ao,depth,x,y,width,height,inverse,projection[5],TSFP_SSAO_RADIUS,TSFP_SSAO_BIAS,TSFP_SSAO_STRENGTH))
        tsfp_ssao_apply(&g_scene_ao,depth,d3d8_surface_draw_view(s->scene_surface),width,height);
    rhi_view_destroy(depth);
}
