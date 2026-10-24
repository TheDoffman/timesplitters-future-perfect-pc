/* Original effect 19, callback 2BE30. The preserved Original60 draw uses
 * A8L8 source pixels, linear wrap sampling, ONE+ONE addition and RGB-only writes.
 * UV offsets below are controlled, not the game's RNG sequence. */
typedef struct FieldNoise {
    RhiShader *vertex,*pixel;
    RhiImage *image;RhiView *read;
    RhiBuffer *offset;
    RhiBlendState *add,*copy;
    NV2APSConstants constants;
} FieldNoise;
static void field_noise_init(FieldNoise *n) {
    memset(n,0,sizeof *n);
    char vertex[65536];snprintf(vertex,sizeof vertex,
        "#define main original_vertex\n%s\n#undef main\n"
        "cbuffer NoiseOffset:register(b4){float4 noise_offset;}"
        "VS_OUT main(uint id:SV_VertexID){VS_IN v=(VS_IN)0;"
        "float2 uv=float2(id>=2?1:0,(id&1)?1:0);"
        "v.v1=float4(uv*128,.001,1);v.v2=float4(127,127,127,127)/255;"
        "v.v3=float4(uv+noise_offset.xy,0,1);v.v8=float4(0,0,0,1);return original_vertex(v);}",noise_vertex_hlsl);
    n->vertex=shader(RHI_STAGE_VERTEX,vertex,"vs_5_0");
    n->pixel=shader(RHI_STAGE_PIXEL,noise_hlsl,"ps_5_0");
    DWORD definition[60];FILE *file=fopen("noise.bin","rb");CHECK(file);
    CHECK(fread(definition,4,60,file)==60);fclose(file);
    float vectors[3][4]={{0,1,0,0}};field_constants(&n->constants,definition,vectors);
    static unsigned char pixels[128*128*4];file=fopen("noise.rgba","rb");CHECK(file);
    CHECK(fread(pixels,1,sizeof pixels,file)==sizeof pixels);fclose(file);
    RhiImageDesc desc={0};desc.width=desc.height=128;desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData data={pixels,128*4,sizeof pixels};n->image=rhi_image_create(&desc,&data);CHECK(n->image);
    n->read=rhi_view_create(n->image,RHI_VIEW_SAMPLED,NULL);CHECK(n->read);
    RhiBufferDesc buffer={16,RHI_USAGE_DEFAULT,RHI_BIND_UNIFORM,0};
    n->offset=rhi_buffer_create(&buffer,NULL);CHECK(n->offset);
    RhiBlendDesc blend={0};blend.enable=1;blend.src=blend.dst=RHI_BLEND_ONE;
    blend.op=blend.op_alpha=RHI_BLEND_OP_ADD;blend.src_alpha=blend.dst_alpha=RHI_BLEND_ONE;
    blend.write_mask=RHI_WRITE_R|RHI_WRITE_G|RHI_WRITE_B;
    n->add=rhi_blend_state_create(&blend);CHECK(n->add);
    blend.enable=0;blend.write_mask=RHI_WRITE_ALL;
    n->copy=rhi_blend_state_create(&blend);CHECK(n->copy);
}
static void field_noise_apply(FieldNoise *n,FieldFinal *f,RoundTarget *target,unsigned tick) {
    static float before[128*128*4],after[128*128*4];
    if(!tick)field_read(target->image,2,before);
    float offset[4]={.506488204f+((tick*17)%128)/128.f,.382273525f+((tick*31)%128)/128.f,0,0};
    RhiView *none[4]={0};rhi_set_textures(0,4,none);rhi_set_render_target(target->write,NULL);
    rhi_set_textures(0,1,&n->read);rhi_set_shader(RHI_STAGE_VERTEX,n->vertex);
    rhi_set_shader(RHI_STAGE_PIXEL,n->pixel);rhi_set_topology(RHI_TOPOLOGY_TRIANGLE_STRIP);
    rhi_buffer_update(n->offset,offset);rhi_set_uniform_buffers(RHI_STAGE_VERTEX,4,1,&n->offset);
    rhi_buffer_update(f->pixel_buffer,&n->constants);rhi_set_blend_state(n->add,NULL,~0u);
    rhi_draw(4,0);rhi_set_blend_state(n->copy,NULL,~0u);
    rhi_set_textures(0,4,none);rhi_set_render_target(NULL,NULL);
    rhi_set_shader(RHI_STAGE_VERTEX,f->fullscreen);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
    if(!tick) {
        field_read(target->image,2,after);unsigned increased=0;
        for(unsigned i=0;i<128*128;i++) {
            CHECK(after[i*4]==before[i*4] && after[i*4+2]==before[i*4+2] && after[i*4+3]==before[i*4+3]);
            CHECK(after[i*4+1]>=before[i*4+1]);increased+=after[i*4+1]>before[i*4+1];
        }
        CHECK(increased>0);
    }
}
static void field_noise_destroy(FieldNoise *n) {
    RhiBuffer *none=NULL;rhi_set_uniform_buffers(RHI_STAGE_VERTEX,4,1,&none);
    rhi_set_blend_state(NULL,NULL,~0u);
    rhi_shader_destroy(n->vertex);rhi_shader_destroy(n->pixel);
    rhi_view_destroy(n->read);rhi_image_destroy(n->image);rhi_buffer_destroy(n->offset);
    rhi_blend_state_destroy(n->add);rhi_blend_state_destroy(n->copy);
}
