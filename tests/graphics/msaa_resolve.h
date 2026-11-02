/* Sample-frequency shading proves that every sample contributes to the resolve,
 * not merely that a solid multisampled clear can be copied. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static void test_msaa_resolve(void)
{
    const char *hlsl="struct O{float4 p:SV_POSITION;}; O vs(uint id:SV_VertexID){"
        "float2 c=float2((id<<1)&2,id&2);O o;o.p=float4(c*float2(2,-2)+float2(-1,1),0,1);return o;}"
        "float4 ps(O i,uint sample:SV_SampleIndex):SV_TARGET{return float4((sample+1)/8.0,.25,.5,1);}";
    char error[2048]={0};
    RhiShaderSource code={hlsl,strlen(hlsl),"msaa-sample-test",NULL,"vs","vs_5_0",1};
    RhiShader *vs=rhi_shader_create(RHI_STAGE_VERTEX,&code,error,sizeof error);assert(vs);
    code.entry="ps";code.target="ps_5_0";
    RhiShader *ps=rhi_shader_create(RHI_STAGE_PIXEL,&code,error,sizeof error);assert(ps);
    RhiRasterDesc rd={0};rd.fill=RHI_FILL_SOLID;rd.cull=RHI_CULL_NONE;rd.multisample=1;
    RhiRasterState *raster=rhi_raster_state_create(&rd);assert(raster);
    RhiDepthDesc dd={0};RhiDepthState *depth=rhi_depth_state_create(&dd);assert(depth);
    RhiBlendDesc bd={0};bd.write_mask=RHI_WRITE_ALL;
    RhiBlendState *blend=rhi_blend_state_create(&bd);assert(blend);
    int exercised=0;
    for(unsigned count=2;count<=8;count*=2) {
        RhiImageDesc desc={0};unsigned char pixels[16*16*4];
        if(!rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,count))continue;
        desc.type=RHI_IMAGE_2D;desc.width=desc.height=16;desc.depth=desc.mip_levels=1;
        desc.samples=count;desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_RENDER_TARGET;
        RhiImage *ms=rhi_image_create(&desc,NULL);assert(ms);
        RhiView *rt=rhi_view_create(ms,RHI_VIEW_RENDER_TARGET,NULL);assert(rt);
        desc.samples=1;desc.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
        RhiImage *single=rhi_image_create(&desc,NULL);assert(single);
        RhiViewport viewport={0,0,16,16,0,1};RhiOutputState saved;
        rhi_set_render_target(rt,NULL);rhi_set_viewports(1,&viewport);
        rhi_set_shader(RHI_STAGE_VERTEX,vs);rhi_set_shader(RHI_STAGE_PIXEL,ps);
        rhi_set_vertex_layout(NULL);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
        rhi_set_raster_state(raster);rhi_set_depth_state(depth,0);rhi_set_blend_state(blend,NULL,~0u);
        rhi_draw(3,0);
        assert(rhi_image_resolve(ms,single)==0);
        rhi_output_save(&saved);assert(rhi_output_color_is(&saved,rt));rhi_output_restore(&saved);
        assert(rhi_image_readback(single,0,pixels,16*4)==0);
        for(unsigned i=0;i<16*16;i++) {
            int red=(int)((count+1)*255/16);
            assert(abs((int)pixels[i*4]-red)<=1);
            assert(abs((int)pixels[i*4+1]-64)<=1 && abs((int)pixels[i*4+2]-128)<=1);
        }
        assert(rhi_image_resolve(single,ms)==-1);
        assert(rhi_image_resolve(ms,ms)==-1);
        rhi_set_render_target(NULL,NULL);rhi_view_destroy(rt);
        rhi_image_destroy(ms);rhi_image_destroy(single);exercised++;
    }
    assert(exercised>0);
    rhi_shader_destroy(vs);rhi_shader_destroy(ps);rhi_raster_state_destroy(raster);
    rhi_depth_state_destroy(depth);rhi_blend_state_destroy(blend);
    puts("PASS: MSAA sample averaging and output restoration");
}

static void test_msaa_subresources(void)
{
    unsigned samples=4;
    while(samples>1 && !rhi_sample_count_supported(RHI_FORMAT_R8G8B8A8_UNORM,samples))samples/=2;
    assert(samples>1);
    RhiImageDesc d={0};d.type=RHI_IMAGE_2D;d.width=d.height=32;d.depth=6;
    d.mip_levels=2;d.format=RHI_FORMAT_R8G8B8A8_UNORM;d.samples=1;
    d.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;d.cube=1;
    RhiImage *cube=rhi_image_create(&d,NULL);assert(cube);
    const float blue[4]={0,0,1,1},green[4]={0,1,0,1};
    for(unsigned face=0;face<6;face++)for(unsigned mip=0;mip<2;mip++){
        RhiViewDesc vd={0};vd.dim=RHI_VIEW_DIM_2D_ARRAY;vd.base_layer=face;
        vd.layer_count=vd.mip_count=1;vd.base_mip=mip;
        RhiView *view=rhi_view_create(cube,RHI_VIEW_RENDER_TARGET,&vd);assert(view);
        rhi_clear_color(view,blue);rhi_view_destroy(view);
    }
    d.width=d.height=16;d.depth=d.mip_levels=1;d.cube=0;d.samples=samples;
    RhiImage *ms=rhi_image_create(&d,NULL);assert(ms);
    RhiView *view=rhi_view_create(ms,RHI_VIEW_RENDER_TARGET,NULL);assert(view);
    rhi_set_render_target(view,NULL);rhi_clear_color(view,green);
    assert(rhi_image_resolve_subresource(ms,0,cube,11)==0);
    RhiOutputState saved;rhi_output_save(&saved);assert(rhi_output_color_is(&saved,view));rhi_output_restore(&saved);
    assert(rhi_image_resolve_subresource(ms,0,cube,0)==-1); /* wrong mip extent */
    assert(rhi_image_resolve_subresource(ms,1,cube,11)==-1); /* source layer */
    assert(rhi_image_resolve_subresource(ms,0,cube,12)==-1); /* destination layer */
    assert(rhi_image_resolve_subresource(ms,0,cube,~0u)==-1);
    assert(rhi_image_resolve(ms,cube)==-1); /* whole-image API remains strict */
    for(unsigned sub=0;sub<12;sub++){
        unsigned size=32>>(sub%2);unsigned char pixels[32*32*4];
        assert(rhi_image_readback(cube,sub,pixels,size*4)==0);
        for(unsigned i=0;i<size*size;i++){
            assert(pixels[i*4]==0 && pixels[i*4+3]==255);
            assert(pixels[i*4+1]==(sub==11?255:0));
            assert(pixels[i*4+2]==(sub==11?0:255));
        }
    }
    rhi_set_render_target(NULL,NULL);rhi_view_destroy(view);rhi_image_destroy(ms);rhi_image_destroy(cube);
    puts("PASS: MSAA cube-face/mip resolve preserves all other subresources");
}
