/* Included after common scene creation. Retain world targets for the next
 * frame, but resolve the completed single-view world before drawing the HUD.
 * The rest of this frame's overlays use output pixels and original blending. */
#include "graphics/native_hud.h"
#include "graphics/native_hud_depth.h"
static struct {
    D3D8DeviceState other;
    TsfpHudDepth depth_copy;
    RhiView *auto_view;
    RhiImage *auto_image;
    unsigned auto_width,auto_height;
} g_native_hud;

static void native_hud_targets_destroy(D3D8DeviceState *s)
{
    if(s->scene_surface)IDirect3DSurface8_Release(&s->scene_surface->iface);
    rhi_view_destroy(s->depth_view);rhi_image_destroy(s->depth_image);
    rhi_view_destroy(s->rhi_scene_srv);rhi_view_destroy(s->rhi_default_rtv);
    rhi_image_destroy(s->rhi_scene_image);rhi_image_destroy(s->rhi_scene_resolved);
    memset(s,0,sizeof *s);
}
static void native_hud_swap(void)
{
    D3D8DeviceState *s=&g_device_state,*other=&g_native_hud.other;
#define SWAP_FIELD(type,field) do {type value=s->field;s->field=other->field;other->field=value;}while(0)
    SWAP_FIELD(UINT,width);SWAP_FIELD(UINT,height);
    SWAP_FIELD(D3D8Surface *,scene_surface);
    SWAP_FIELD(RhiImage *,depth_image);SWAP_FIELD(RhiView *,depth_view);
    SWAP_FIELD(RhiImage *,rhi_scene_image);SWAP_FIELD(RhiImage *,rhi_scene_resolved);
    SWAP_FIELD(RhiView *,rhi_scene_srv);SWAP_FIELD(RhiView *,rhi_default_rtv);
#undef SWAP_FIELD
    RhiView *view=g_auto_ds->dsv;g_auto_ds->dsv=g_native_hud.auto_view;g_native_hud.auto_view=view;
    RhiImage *image=g_auto_ds->image;g_auto_ds->image=g_native_hud.auto_image;g_native_hud.auto_image=image;
    unsigned w=g_auto_ds->width,h=g_auto_ds->height;
    g_auto_ds->width=g_native_hud.auto_width;g_auto_ds->height=g_native_hud.auto_height;
    g_native_hud.auto_width=w;g_native_hud.auto_height=h;
}
static void native_hud_rebind(void)
{
    D3D8DeviceState *s=&g_device_state;
    rhi_set_render_target(d3d8_surface_draw_view(g_cur_rt?g_cur_rt:s->scene_surface),draw_depth_view(g_cur_ds));
    apply_host_viewport();rebind_stage_srvs(NULL);
}
static void native_hud_restore(void)
{
    if(!g_native_hud_active)return;
    native_hud_swap();g_native_hud_active=0;native_hud_rebind();
}
static void native_hud_shutdown(void)
{
    native_hud_restore();native_hud_targets_destroy(&g_native_hud.other);
    rhi_view_destroy(g_native_hud.auto_view);rhi_image_destroy(g_native_hud.auto_image);
    tsfp_hud_depth_destroy(&g_native_hud.depth_copy);memset(&g_native_hud,0,sizeof g_native_hud);
}
static void native_hud_begin(void)
{
#ifdef TSFP_PC_DISPLAY
    D3D8DeviceState *s=&g_device_state,*other=&g_native_hud.other;
    unsigned width,height;
    const char *disabled=getenv("TSFP_NATIVE_HUD");
    if(g_native_hud_active || (disabled && !strcmp(disabled,"0")) ||
       !g_device_initialized || g_cur_rt || !g_auto_ds || g_cur_ds!=g_auto_ds || g_interp_on ||
       s->viewport.X || s->viewport.Y ||
       s->viewport.Width!=s->present_width || s->viewport.Height!=s->present_height)return;
    pc_display_scene_size(s->present_width*d3d8_display_policy()->scale,
                          s->present_height*d3d8_display_policy()->scale,&width,&height);
    if(!width || !height || width>=s->width || height>=s->height)return;
    RhiView *world=d3d8_GetSceneView();
    RhiView *source_depth=rhi_view_create(g_auto_ds->image,RHI_VIEW_SAMPLED,NULL);
    if(!world || !source_depth){rhi_view_destroy(source_depth);return;}
    if(other->width!=width || other->height!=height) {
        D3D8DeviceState next={0};next.width=width;next.height=height;
        RhiView *view=NULL;
        if(SUCCEEDED(create_render_targets_samples(&next,1)))
            view=rhi_view_create(next.depth_image,RHI_VIEW_DEPTH,NULL);
        if(!view) {
            native_hud_targets_destroy(&next);rhi_view_destroy(source_depth);
            native_hud_rebind();return;
        }
        native_hud_targets_destroy(other);rhi_view_destroy(g_native_hud.auto_view);
        rhi_image_destroy(g_native_hud.auto_image);*other=next;
        g_native_hud.auto_view=view;g_native_hud.auto_image=rhi_image_retain(next.depth_image);
        g_native_hud.auto_width=width;g_native_hud.auto_height=height;
        fprintf(stderr,"[NATIVE-HUD] World %ux%u; HUD %ux%u\n",s->width,s->height,width,height);
    }
    rhi_clear_depth(other->depth_view,RHI_CLEAR_DEPTH|RHI_CLEAR_STENCIL,1.f,0);
    int ok=tsfp_hud_depth_copy(&g_native_hud.depth_copy,source_depth,other->depth_view,width,height);
    rhi_view_destroy(source_depth);
    D3D8DisplayFit fit={0,0,width,height};
    if(ok && SUCCEEDED(d3d8_display_resolve(world,s->width,s->height,other->rhi_default_rtv,fit))) {
        native_hud_swap();g_native_hud_active=1;
    }
    native_hud_rebind();
#endif
}
