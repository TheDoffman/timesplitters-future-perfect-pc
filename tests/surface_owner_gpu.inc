/* These checks run outside timing spans on both WARP and explicit hardware.
 * Original shaders still own event/byte boundaries; this exercises only the
 * shared coupled GPU owner's resource and renderer-state contracts. */
static void field_owner_restored(PcSurfaceGpu *surface,const RhiOutputState *expected) {
    RhiOutputState actual;rhi_output_save(&actual);
    CHECK(actual.color==expected->color && actual.depth==expected->depth);
    CHECK(actual.viewport_count==expected->viewport_count);
    CHECK(!memcmp(actual.viewports,expected->viewports,actual.viewport_count*sizeof actual.viewports[0]));
    rhi_output_restore(&actual);
    ID3D11Device *device=NULL;ID3D11DeviceContext *context=NULL;
    ID3D11Texture2D_GetDevice((ID3D11Texture2D*)rhi_d3d11_native_image(surface->world.image),&device);
    ID3D11Device_GetImmediateContext(device,&context);
    ID3D11ShaderResourceView *textures[4]={0};ID3D11DeviceContext_PSGetShaderResources(context,0,4,textures);
    for(unsigned i=0;i<4;i++)CHECK(!textures[i]);
    ID3D11Buffer *buffer=NULL;ID3D11DeviceContext_PSGetConstantBuffers(context,1,1,&buffer);CHECK(!buffer);
    ID3D11DeviceContext_Release(context);ID3D11Device_Release(device);
}
static void field_owner_lifecycle(PcSurfaceGpu *surface) {
    char errors[4096];PcSurfaceGpuConfig invalid=surface->config;invalid.side=0;
    CHECK(!pc_surface_gpu_create(&invalid,errors,sizeof errors));
    invalid=surface->config;invalid.d=NAN;CHECK(!pc_surface_gpu_create(&invalid,errors,sizeof errors));
    CHECK(!pc_surface_gpu_output(surface) && !pc_surface_gpu_advance(surface,.5));
    RhiOutputState saved;rhi_output_save(&saved);
    RoundTarget input=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
    RoundTarget residual=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
    RoundTarget sentinel=round_target(RHI_FORMAT_R8G8B8A8_UNORM);
    RhiImageDesc desc={0};desc.width=desc.height=SURFACE_SIDE;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_D32_FLOAT;desc.bind=RHI_BIND_DEPTH;
    RhiImage *depth=rhi_image_create(&desc,NULL);CHECK(depth);
    RhiView *depth_view=rhi_view_create(depth,RHI_VIEW_DEPTH,NULL);CHECK(depth_view);
    const float color[4]={.2f,.4f,.6f,1};rhi_clear_color(sentinel.write,color);
    rhi_clear_depth(depth_view,RHI_CLEAR_DEPTH,.25f,0);
    RhiViewport viewports[2]={{2,3,17,19,.2f,.8f},{11,13,23,29,.1f,.9f}};
    rhi_set_render_target(sentinel.write,depth_view);rhi_set_viewports(2,viewports);
    RhiOutputState expected;rhi_output_save(&expected);
    for(unsigned cycle=0;cycle<2;cycle++) {
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++) {
            field_a[i*4]=cycle?.12f:-.01f;field_a[i*4+1]=cycle?.7f:.3f;
            field_a[i*4+2]=0;field_a[i*4+3]=1;
            field_b[i*4]=field_b[i*4+1]=field_b[i*4+2]=0;field_b[i*4+3]=1;
        }
        rhi_image_update(input.image,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
        rhi_image_update(residual.image,0,NULL,field_b,SURFACE_SIDE*16,sizeof field_b);
        CHECK(pc_surface_gpu_begin(surface,input.read,residual.read));field_owner_restored(surface,&expected);
        CHECK(pc_surface_gpu_output(surface)==surface->world.read);
        field_read(surface->world.image,0,field_b);CHECK(!memcmp(field_a,field_b,sizeof field_a));
        CHECK(pc_surface_gpu_advance(surface,0));
        CHECK(!pc_surface_gpu_advance(surface,NAN) && !pc_surface_gpu_advance(surface,-1) && !pc_surface_gpu_advance(surface,1.01));
        CHECK(!pc_surface_gpu_begin(surface,sentinel.read,residual.read)); /* wrong storage */
        CHECK(!pc_surface_gpu_begin(surface,input.read,surface->world.read)); /* overwritten alias */
        CHECK(!pc_surface_gpu_begin(surface,surface->state[0].read,residual.read));
        field_owner_restored(surface,&expected);field_read(surface->world.image,0,field_b);
        CHECK(!memcmp(field_a,field_b,sizeof field_a));
        CHECK(pc_surface_gpu_advance(surface,.375));field_owner_restored(surface,&expected);
        field_read(surface->world.image,0,field_b);CHECK(memcmp(field_a,field_b,sizeof field_a));
        memcpy(field_a,field_b,sizeof field_a);
        pc_surface_gpu_reset(surface);CHECK(!pc_surface_gpu_advance(surface,.5));
        CHECK(!pc_surface_gpu_output(surface));
        field_read(surface->world.image,0,field_b);CHECK(!memcmp(field_a,field_b,sizeof field_a));
    }
    field_read(sentinel.image,2,field_a);
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned c=0;c<4;c++)CHECK(fabsf(field_a[i*4+c]-color[c])<.501f/255);
    rhi_set_viewports(0,NULL);RhiOutputState empty;rhi_output_save(&empty);
    CHECK(pc_surface_gpu_begin(surface,input.read,residual.read));field_owner_restored(surface,&empty);
    rhi_output_restore(&empty);pc_surface_gpu_reset(surface);
    rhi_output_restore(&expected);rhi_output_restore(&saved);
    rhi_view_destroy(depth_view);rhi_image_destroy(depth);
    round_destroy(&input);round_destroy(&residual);round_destroy(&sentinel);
    puts("PASS: surface owner reset, input rejection, output restoration and unbinding");
}
