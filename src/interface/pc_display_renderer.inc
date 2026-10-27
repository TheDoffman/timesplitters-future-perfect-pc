/* Included by d3d8_device.c: the guest update/menu callbacks run on this
   render thread, outside a draw. Preserve the depth surface's identity because
   HLE and the title can retain it across resolution changes. */
HWND xbox_D3D8Window(void) { return g_device_state.hwnd; }

static void pc_destroy_targets(D3D8DeviceState *s)
{
    rhi_view_destroy(s->depth_view); rhi_image_destroy(s->depth_image);
    rhi_view_destroy(s->rhi_scene_srv); rhi_view_destroy(s->rhi_default_rtv);
    rhi_image_destroy(s->rhi_scene_image);
}

int xbox_D3D8ChangeResolution(unsigned width,unsigned height)
{
    D3D8DeviceState next={0}, *s=&g_device_state;
    RhiView *auto_view;
    if(!g_device_initialized || g_cur_rt || !g_auto_ds || (g_cur_ds && g_cur_ds!=g_auto_ds)) return 0;
    if(!width && !height) {
        width=s->present_width*d3d8_display_policy()->scale;
        height=s->present_height*d3d8_display_policy()->scale;
    }
    if(width==s->width && height==s->height) return 1;
    next.width=width; next.height=height;
    if(FAILED(create_render_targets(&next))) {
        pc_destroy_targets(&next);
        return 0;
    }
    auto_view=rhi_view_create(next.depth_image,RHI_VIEW_DEPTH,NULL);
    if(!auto_view) {
        rhi_set_render_target(s->rhi_default_rtv,g_cur_ds?g_cur_ds->dsv:NULL);
        pc_destroy_targets(&next); return 0;
    }
    rhi_view_destroy(g_auto_ds->dsv);
    rhi_image_destroy(g_auto_ds->image);
    g_auto_ds->dsv=auto_view;
    g_auto_ds->image=rhi_image_retain(next.depth_image);
    g_auto_ds->width=width; g_auto_ds->height=height;
    rhi_set_render_target(next.rhi_default_rtv,g_cur_ds?g_cur_ds->dsv:NULL);
    pc_destroy_targets(s);
    s->depth_image=next.depth_image; s->depth_view=next.depth_view;
    s->rhi_scene_image=next.rhi_scene_image; s->rhi_scene_srv=next.rhi_scene_srv;
    s->rhi_default_rtv=next.rhi_default_rtv; s->width=width; s->height=height;
    apply_host_viewport(); rebind_stage_srvs(NULL);
    fprintf(stderr,"[PC-DISPLAY] Native scene target %ux%u\n",width,height);
    return 1;
}
