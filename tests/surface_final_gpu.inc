/* Original effect 22: callback 2C320, pixel definition 4BC318. This consumes
 * alpha-height cardinal differences, then produces the ripple shading input.
 * Resources are fixture-owned; no game hook invokes this experiment. */
typedef struct FieldFinal {
    RhiShader *vertex,*fullscreen,*pixel,*pack;
    RhiBuffer *vertex_buffer,*pixel_buffer;
    RhiImage *packed,*target,*byte_target;
    RhiView *packed_read,*packed_write,*target_write,*byte_write;
    float offsets[192][4];
    NV2APSConstants constants;
} FieldFinal;

static void field_final_init(FieldFinal *f,RhiShader *vertex,RhiShader *fullscreen,
    RhiBuffer *vertex_buffer,RhiBuffer *pixel_buffer) {
    memset(f,0,sizeof *f);f->vertex=vertex;f->fullscreen=fullscreen;
    f->vertex_buffer=vertex_buffer;f->pixel_buffer=pixel_buffer;
    f->offsets[17][0]=-1.f/SURFACE_SIDE;f->offsets[18][0]=1.f/SURFACE_SIDE;
    f->offsets[19][1]=1.f/SURFACE_SIDE;f->offsets[20][1]=-1.f/SURFACE_SIDE;
    DWORD definition[60];FILE *file=fopen("final.bin","rb");CHECK(file);
    CHECK(fread(definition,4,60,file)==60);fclose(file);
    CHECK(definition[14]==0x00808000u && definition[15]==0x000000ffu);
    /* Callback uploads .6 in red/green through the truncating byte uploader.
     * Constants 2/3 retain the original definition's center/blue values. */
    float vectors[3][4]={{153/255.f,0,0,0},{0,153/255.f,0,0},{128/255.f,128/255.f,0,0}};
    field_constants(&f->constants,definition,vectors);
    f->pixel=shader(RHI_STAGE_PIXEL,final_hlsl,"ps_5_0");
#define TSFP_SURFACE_VEC2 "float2"
#define TSFP_SURFACE_VEC4 "float4"
#include "graphics/surface_state_shader.h"
    const char pack_hlsl[]="Texture2D source:register(t0);" TSFP_SURFACE_STATE_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {return surface_state(source.Load(int3(p.xy,0)).xy);}";
    f->pack=shader(RHI_STAGE_PIXEL,pack_hlsl,"ps_5_0");
#undef TSFP_SURFACE_VEC2
#undef TSFP_SURFACE_VEC4
    RhiImageDesc desc={0};desc.width=desc.height=SURFACE_SIDE;desc.depth=1;
    desc.mip_levels=desc.samples=1;desc.format=RHI_FORMAT_R32G32B32A32_FLOAT;
    desc.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
    f->packed=rhi_image_create(&desc,NULL);f->target=rhi_image_create(&desc,NULL);
    CHECK(f->packed && f->target);
    f->packed_read=rhi_view_create(f->packed,RHI_VIEW_SAMPLED,NULL);
    f->packed_write=rhi_view_create(f->packed,RHI_VIEW_RENDER_TARGET,NULL);
    f->target_write=rhi_view_create(f->target,RHI_VIEW_RENDER_TARGET,NULL);
    CHECK(f->packed_read && f->packed_write && f->target_write);
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;
    f->byte_target=rhi_image_create(&desc,NULL);CHECK(f->byte_target);
    f->byte_write=rhi_view_create(f->byte_target,RHI_VIEW_RENDER_TARGET,NULL);CHECK(f->byte_write);
}
static void field_final_run(FieldFinal *f,RhiView *source,int encode,int byte_output,float *output) {
    RhiView *none[4]={0};rhi_set_textures(0,4,none);
    if(encode) {
        rhi_set_render_target(f->packed_write,NULL);rhi_set_textures(0,1,&source);
        rhi_set_shader(RHI_STAGE_VERTEX,f->fullscreen);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
        rhi_set_shader(RHI_STAGE_PIXEL,f->pack);rhi_draw(3,0);
        rhi_set_textures(0,4,none);source=f->packed_read;
    }
    rhi_set_render_target(byte_output?f->byte_write:f->target_write,NULL);
    RhiView *samples[4]={source,source,source,source};rhi_set_textures(0,4,samples);
    rhi_set_shader(RHI_STAGE_VERTEX,f->vertex);
    rhi_set_topology(SURFACE_WRAP?RHI_TOPOLOGY_TRIANGLE_STRIP:RHI_TOPOLOGY_TRIANGLES);
    rhi_buffer_update(f->vertex_buffer,f->offsets);
    rhi_set_shader(RHI_STAGE_PIXEL,f->pixel);rhi_buffer_update(f->pixel_buffer,&f->constants);
    rhi_draw(SURFACE_WRAP?4:3,0);
    if(output)field_read(byte_output?f->byte_target:f->target,byte_output?2:0,output);
    rhi_set_textures(0,4,none);rhi_set_render_target(NULL,NULL);
    rhi_set_shader(RHI_STAGE_VERTEX,f->fullscreen);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
}
static void field_final_destroy(FieldFinal *f) {
    rhi_view_destroy(f->packed_read);rhi_view_destroy(f->packed_write);rhi_view_destroy(f->target_write);
    rhi_view_destroy(f->byte_write);rhi_image_destroy(f->byte_target);
    rhi_image_destroy(f->packed);rhi_image_destroy(f->target);
    rhi_shader_destroy(f->pixel);rhi_shader_destroy(f->pack);
}
static void field_final_check(const float *state,unsigned height,const float *output,float tolerance) {
    for(int y=0;y<SURFACE_SIDE;y++)for(int x=0;x<SURFACE_SIDE;x++) {
        float slopes[2]={state[field_index(x-1,y,height)]-state[field_index(x+1,y,height)],
            state[field_index(x,y-1,height)]-state[field_index(x,y+1,height)]};
        for(unsigned c=0;c<3;c++) {
            float expected=1;
            if(c<2) {
                float v=fmaxf(-1,fminf(1,slopes[c]*4));
                v=fmaxf(-1,fminf(1,v*(153/255.f)*4));
                expected=fmaxf(0,fminf(1,v+128/255.f));
            }
            CHECK(fabsf(output[field_index(x,y,c)]-expected)<tolerance);
        }
    }
}
