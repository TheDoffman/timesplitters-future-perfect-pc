#include "graphics/visibility_queries.h"
static TsfpVisibilityQueries device_visibility;
static void *visibility_create(void *host){(void)host;return rhi_occlusion_create();}
static void visibility_destroy(void *host,void *q){(void)host;rhi_occlusion_destroy(q);}
static int visibility_begin(void *host,void *q){(void)host;return rhi_occlusion_begin(q);}
static int visibility_end(void *host,void *q){(void)host;return rhi_occlusion_end(q);}
static int visibility_result(void *host,void *q,uint64_t *pixels){(void)host;return rhi_occlusion_result(q,pixels);}
static const TsfpVisibilityOps device_visibility_ops={NULL,visibility_create,visibility_destroy,visibility_begin,visibility_end,visibility_result};
int xbox_D3D8VisibilityBegin(void) {
    D3D8Surface *surface=g_cur_rt?g_cur_rt:g_device_state.scene_surface;
    RhiView *view=surface?d3d8_surface_draw_view(surface):NULL;
    if(!view)return 0;
    RhiImageDesc desc;rhi_image_get_desc(rhi_view_image(view),&desc);
    double scale=rt_scale_x()*rt_scale_y()*(desc.samples?desc.samples:1);
    return tsfp_visibility_begin(&device_visibility,&device_visibility_ops,scale);
}
void xbox_D3D8VisibilityEnd(uint32_t index,int accepted) {
    (void)tsfp_visibility_end(&device_visibility,&device_visibility_ops,index,accepted);
}
int xbox_D3D8VisibilityResult(uint32_t index,uint32_t *pixels) {
    return tsfp_visibility_result(&device_visibility,&device_visibility_ops,index,pixels);
}
static void device_visibility_reset(void) {
    tsfp_visibility_reset(&device_visibility,&device_visibility_ops);
}
