/* Measured first-pass clipping and rounding are held over each authored interval. Fractional
 * dynamics still execute on every frame. Both quantization points use actual
 * byte render targets, not a CPU approximation to UNORM conversion. */
#include "pc_native_timing.h"
#include <float.h>
#include "graphics/surface_interval.h"
#include "graphics/surface_gpu_cache.h"
typedef struct RoundTarget { RhiImage *image;RhiView *read,*write; } RoundTarget;
static RoundTarget round_target(unsigned format) {
    RhiImageDesc d={0};d.width=d.height=SURFACE_SIDE;d.depth=d.mip_levels=d.samples=1;
    d.format=format;d.bind=RHI_BIND_SAMPLED|RHI_BIND_RENDER_TARGET;
    RoundTarget t={0};t.image=rhi_image_create(&d,NULL);CHECK(t.image);
    t.read=rhi_view_create(t.image,RHI_VIEW_SAMPLED,NULL);
    t.write=rhi_view_create(t.image,RHI_VIEW_RENDER_TARGET,NULL);CHECK(t.read && t.write);return t;
}
static void round_destroy(RoundTarget *t) {
    rhi_view_destroy(t->read);rhi_view_destroy(t->write);rhi_image_destroy(t->image);
}
#include "surface_owner_gpu.inc"
static void round_draw(FieldFinal *f,RhiShader *pixel,const float *offsets,
    const NV2APSConstants *constants,RhiView **inputs,unsigned count,RoundTarget *to) {
    RhiView *none[4]={0};rhi_set_textures(0,4,none);rhi_set_render_target(to->write,NULL);
    rhi_set_textures(0,count,inputs);rhi_set_shader(RHI_STAGE_PIXEL,pixel);
    rhi_set_shader(RHI_STAGE_VERTEX,offsets?f->vertex:f->fullscreen);
    rhi_set_topology(offsets?RHI_TOPOLOGY_TRIANGLE_STRIP:RHI_TOPOLOGY_TRIANGLES);
    if(offsets)rhi_buffer_update(f->vertex_buffer,offsets);
    if(constants)rhi_buffer_update(f->pixel_buffer,constants);
    rhi_draw(offsets?4:3,0);rhi_set_textures(0,4,none);rhi_set_render_target(NULL,NULL);
    rhi_set_shader(RHI_STAGE_VERTEX,f->fullscreen);rhi_set_topology(RHI_TOPOLOGY_TRIANGLES);
}
#if SURFACE_ROUNDING_PROFILE == 3
#include "surface_noise_gpu.inc"
#ifndef SURFACE_NOISE_REPEATS
#define SURFACE_NOISE_REPEATS 1
#endif
#endif
static void field_rounding_cases(FieldFinal *f,RhiShader **feedback) {
    CHECK(SURFACE_SIDE==128 && SURFACE_WRAP);
#if SURFACE_ROUNDING_PROFILE == 3
    FieldNoise noise;field_noise_init(&noise);
#endif
    const double a=26./255,b=165./255,d=249./255,k=43./255,c=128./255;
    DWORD definitions[3][60];const char *names[]={"first.bin","feedback.bin","filter.bin"};
    for(unsigned i=0;i<3;i++){FILE *file=fopen(names[i],"rb");CHECK(file);CHECK(fread(definitions[i],4,60,file)==60);fclose(file);}
    float v0[3][4]={{(float)k,(float)k,(float)k,(float)k}};
    float v1[3][4]={{0,(float)a,0,0},{0,0,(float)b,(float)b},{0,0,(float)d,(float)d}};
    NV2APSConstants cb[3];field_constants(&cb[0],definitions[0],v0);field_constants(&cb[1],definitions[1],v1);
    cb[1].c1[6][1]=128/255.f;
    memset(&cb[2],0,sizeof cb[2]);for(unsigned i=0;i<4;i++)cb[2].tex_scale[i][0]=cb[2].tex_scale[i][1]=1;
    for(unsigned i=0;i<4;i++)cb[2].c0[3][i]=128/255.f;
    float offsets[3][192][4]={0};
    offsets[0][18][0]=offsets[0][18][1]=-1.f/SURFACE_SIDE;
    offsets[0][19][0]=1.f/SURFACE_SIDE;offsets[0][19][1]=-1.f/SURFACE_SIDE;
    offsets[0][20][0]=offsets[0][20][1]=1.f/SURFACE_SIDE;
    offsets[1][18][0]=-1.f/SURFACE_SIDE;offsets[1][18][1]=1.f/SURFACE_SIDE;
    offsets[2][17][1]=-1.f/SURFACE_SIDE;offsets[2][18][0]=1.f/SURFACE_SIDE;
    offsets[2][19][1]=1.f/SURFACE_SIDE;offsets[2][20][0]=-1.f/SURFACE_SIDE;
    RhiShader *filter=shader(RHI_STAGE_PIXEL,filter_hlsl,"ps_5_0");
    RhiShader *unpack=shader(RHI_STAGE_PIXEL,"Texture2D source:register(t0);"
        "float4 main(float4 p:SV_POSITION):SV_TARGET {float4 s=source.Load(int3(p.xy,0));return float4(s.g-.5,s.b,0,1);}","ps_5_0");
#define TSFP_SURFACE_VEC3 "float3"
#include "graphics/surface_gradient_shader.h"
    const char residual_prefix[]="Texture2D rounded:register(t0);Texture2D source:register(t1);"
        TSFP_SURFACE_GRADIENT_SHADER;
    char residual_source[4096];snprintf(residual_source,sizeof residual_source,
        "%sfloat4 main(float4 p:SV_POSITION):SV_TARGET {int2 size=int2(%u,%u),at=int2(p.xy);"
        "float h=source.Load(int3(at,0)).b;float3 n=float3("
        "source.Load(int3((at+size+int2(-1,-1))%%size,0)).b,"
        "source.Load(int3((at+size+int2(1,-1))%%size,0)).b,"
        "source.Load(int3((at+size+int2(1,1))%%size,0)).b);"
        "float diagonal=source.Load(int3((at+size+int2(-1,1))%%size,0)).b;"
        "return float4(surface_feedback_residual(h,n,diagonal,rounded.Load(int3(at,0)).r,43.0/255.0),0,0,1);}",
        residual_prefix,SURFACE_SIDE,SURFACE_SIDE);
    RhiShader *residual=shader(RHI_STAGE_PIXEL,residual_source,"ps_5_0");
#undef TSFP_SURFACE_VEC3
#define TSFP_SURFACE_VEC2 "float2"
#define TSFP_SURFACE_VEC3 "float3"
#define TSFP_SURFACE_VEC4 "float4"
#include "graphics/surface_rounding_shader.h"
    const char boundary_source[]="Texture2D state:register(t0);Texture2D exact:register(t1);"
        TSFP_SURFACE_ROUNDING_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {int3 at=int3(p.xy,0);"
        "return surface_round_boundary(surface_limit_boundary(state.Load(at).xy,249.0/255.0),exact.Load(at));}";
    RhiShader *boundary=shader(RHI_STAGE_PIXEL,boundary_source,"ps_5_0");
    const char guard_source[]="Texture2D state:register(t0);Texture2D exact:register(t1);"
        TSFP_SURFACE_ROUNDING_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {int3 at=int3(p.xy,0);return surface_round_boundary(state.Load(at).xy,exact.Load(at));}";
    RhiShader *guard=shader(RHI_STAGE_PIXEL,guard_source,"ps_5_0");
#undef TSFP_SURFACE_VEC2
#undef TSFP_SURFACE_VEC3
#undef TSFP_SURFACE_VEC4
    RoundTarget bytes[2]={round_target(RHI_FORMAT_R8G8B8A8_UNORM),round_target(RHI_FORMAT_R8G8B8A8_UNORM)};
    RoundTarget first_byte=round_target(RHI_FORMAT_R8G8B8A8_UNORM),first_float=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
    PcSurfaceGpuConfig config={SURFACE_SIDE,a,b,d,k,c};char errors[4096]={0};
    PcSurfaceGpuCache cache={0};
    RhiImage *identity[4]={bytes[0].image,bytes[1].image,first_byte.image,f->byte_target};
    PcSurfaceGpu *surface=pc_surface_gpu_cache_acquire(&cache,0x10000,1,&config,identity,errors,sizeof errors);
    if(!surface)fprintf(stderr,"Surface owner creation: %s\n",errors);CHECK(surface);
    field_owner_lifecycle(surface);
    unsigned ready=surface->ready;
    CHECK(!pc_surface_gpu_begin_bytes(surface,first_byte.read,first_byte.read));
    CHECK(!pc_surface_gpu_begin_bytes(surface,first_float.read,bytes[0].read));
    CHECK(surface->ready==ready);
    RoundTarget world={surface->world.image,surface->world.read,surface->world.write};
    RoundTarget rounding=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
    /* Canonicalization is allowed only within the arithmetic guard, including
     * values away from ideal byte thresholds. Actual GPU conversion owns those
     * thresholds. Large errors must remain visible to trajectory comparisons. */
    const float test_value[]={.5f,.5f,.52f,1.5f,-.5f,.6491695f,.6491695f};
    const float test_exact[]={.500001f,.75f,.520001f,.5f,0,.6491709f,.64918f};
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++) {
        unsigned test=i%7;field_a[i*4]=test_value[test]-.5f;field_a[i*4+1]=test_value[test];
        field_a[i*4+2]=0;field_a[i*4+3]=1;
        for(unsigned c=0;c<4;c++)field_b[i*4+c]=test_exact[test];
    }
    rhi_image_update(world.image,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
    rhi_image_update(first_float.image,0,NULL,field_b,SURFACE_SIDE*16,sizeof field_b);
    RhiView *guard_inputs[2]={world.read,first_float.read};
    round_draw(f,guard,NULL,NULL,guard_inputs,2,&rounding);field_read(rounding.image,0,field_a);
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<4;channel++) {
        unsigned test=i%7;float expected=(test==0 || test==2 || test==5)?test_exact[test]:test_value[test];
        float boundary_error=fabsf(field_a[i*4+channel]-expected);CHECK(boundary_error<1e-7f);
    }
    float *references=malloc(60*sizeof field_a);CHECK(references);
    if(surface_timing_device) {
        /* Report the actual conversion around the observed RTX transition.
         * This is diagnostic evidence, not a portable threshold assumption. */
        RhiShader *copy=shader(RHI_STAGE_PIXEL,"Texture2D source:register(t0);float4 main(float4 p:SV_POSITION):SV_TARGET{return source.Load(int3(p.xy,0));}","ps_5_0");
        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<4;channel++)
            field_a[i*4+channel]=.649169921875f+((int)i-8192)*1e-9f;
        rhi_image_update(world.image,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
        round_draw(f,copy,NULL,NULL,&world.read,1,&bytes[0]);field_read(bytes[0].image,2,field_b);
        for(unsigned i=1;i<SURFACE_SIDE*SURFACE_SIDE;i++)if(field_b[i*4]!=field_b[(i-1)*4])
            printf("quantizer-transition input=%.15g..%.15g output=%.9g..%.9g ideal-half=%.15g\n",field_a[(i-1)*4],field_a[i*4],field_b[(i-1)*4],field_b[i*4],165.5/255);
        rhi_shader_destroy(copy);
    }
    static float initial[SURFACE_SIDE*SURFACE_SIDE*4],normal[SURFACE_SIDE*SURFACE_SIDE*4],exact[SURFACE_SIDE*SURFACE_SIDE*4];
    for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++) {
        unsigned at=(y*SURFACE_SIDE+x)*4;initial[at]=0;initial[at+1]=field_initial(x,y);initial[at+2]=0;initial[at+3]=1;
        if(SURFACE_ROUNDING_PROFILE==1)initial[at+1]=.5f+
            ((x==1 && y==3)?.45f:0)-((x==96 && y==23)?.45f:0);
        if(SURFACE_ROUNDING_PROFILE==2) {
            initial[at]=x<SURFACE_SIDE/2?.49f:-.49f;
            initial[at+1]=x<SURFACE_SIDE/2?.98f:.02f;
        }
    }
    const unsigned rates[]={60,120,144,0,144};
    for(unsigned smoothing=0;smoothing<2;smoothing++) {
        if(smoothing) {
            PcSurfaceGpu *replacement=pc_surface_gpu_cache_acquire(&cache,0x10000,2,&config,identity,errors,sizeof errors);
            CHECK(replacement && replacement!=surface && !pc_surface_gpu_output(replacement));
            surface=replacement;world.image=surface->world.image;world.read=surface->world.read;world.write=surface->world.write;
        }
        unsigned reference_fresh=0;memset(normal,0,sizeof normal);
        rhi_image_update(world.image,0,NULL,initial,SURFACE_SIDE*16,sizeof initial);
        round_draw(f,f->pack,NULL,NULL,&world.read,1,&bytes[0]);unsigned current=0;
        SurfaceTiming baseline;surface_timing_begin(&baseline,60);
        /* Independent original 60-Hz path with real byte initial/feedback state. */
        for(unsigned tick=0;tick<60;tick++) {
            surface_timing_mark(&baseline,tick,0);
            RhiView *inputs[4];
#if SURFACE_ROUNDING_PROFILE == 3
            field_noise_apply(&noise,f,&bytes[current],tick);
#endif
            if(smoothing) {for(unsigned i=0;i<4;i++)inputs[i]=bytes[current].read;
                round_draw(f,filter,&offsets[2][0][0],&cb[2],inputs,4,&bytes[current^1]);current^=1;}
            for(unsigned i=0;i<4;i++)inputs[i]=bytes[current].read;
            round_draw(f,feedback[0],&offsets[0][0][0],&cb[0],inputs,4,&first_byte);
            inputs[0]=first_byte.read;inputs[1]=bytes[current].read;
            round_draw(f,feedback[1],&offsets[1][0][0],&cb[1],inputs,2,&bytes[current^1]);current^=1;
            if(SURFACE_BENCHMARK) {
                field_final_run(f,bytes[current].read,0,1,NULL);
                surface_timing_mark(&baseline,tick,1);continue;
            }
            field_read(bytes[current].image,2,references+tick*SURFACE_SIDE*SURFACE_SIDE*4);
            field_final_run(f,bytes[current].read,0,1,field_b);unsigned changed=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<2;channel++)
                changed|=field_b[i*4+channel]!=normal[i*4+channel];
            reference_fresh+=changed;memcpy(normal,field_b,sizeof normal);
        }
        surface_timing_finish(&baseline,"original",smoothing,60,0);
        if(SURFACE_BENCHMARK)field_read(bytes[current].image,2,references+59*SURFACE_SIDE*SURFACE_SIDE*4);
        for(unsigned rate=0;rate<5;rate++) {
            unsigned fps=rates[rate],ticks=0,fresh=0;double worst=0,arithmetic_error=0;
            /* Use the actual clock producer and shared interval splitter. This
             * frequency represents all presets; CPU coverage adds odd clocks,
             * jitter and catch-up. Mode images stay immutable for every step. */
            const int64_t frequency=72000000;int64_t now=0;PcNativeTiming clock={0};
            pc_native_timing_step(&clock,now,frequency,144,0);
            unsigned frames=fps?fps:117;
            rhi_image_update(world.image,0,NULL,initial,SURFACE_SIDE*16,sizeof initial);
            round_draw(f,f->pack,NULL,NULL,&world.read,1,&bytes[0]);current=0;
            memset(normal,0,sizeof normal);
            SurfaceTiming timing;surface_timing_begin(&timing,frames);
            for(unsigned frame=0;frame<frames;frame++) {
                surface_timing_mark(&timing,frame,0);
                CHECK(pc_surface_gpu_cache_acquire(&cache,0x10000,1+smoothing,&config,identity,errors,sizeof errors)==surface);
                unsigned frame_rate=fps?fps:(frame<37?144:frame<67?120:frame<82?60:144);
                if(frame==10) {
                    now+=frequency/2;
                    PcNativeStep paused=pc_native_timing_step(&clock,now,frequency,frame_rate,1);
                    PcSurfaceInterval stopped;
                    CHECK(pc_surface_interval_split(paused.units_per_tick,paused.game_phase,paused.game_units,&stopped));
                    CHECK(!stopped.count && stopped.end_phase==clock.game_remainder);
                    if(!SURFACE_BENCHMARK) {
                        field_final_run(f,world.read,1,1,field_b);
                        CHECK(!memcmp(normal,field_b,sizeof normal));
                    }
                }
                now+=frequency/frame_rate;
                if(rate==4)now+=(frame&1)?frequency/10000:-frequency/10000;
                PcNativeStep step=pc_native_timing_step(&clock,now,frequency,frame_rate,0);
                PcSurfaceInterval interval;
                CHECK(pc_surface_interval_split(step.units_per_tick,step.game_phase,step.game_units,&interval));
                for(unsigned slice=0;slice<interval.count;slice++) {
                    const PcSurfaceSlice *part=&interval.pieces[slice];
                    RhiView *inputs[4];
                    if(part->begins_tick) {
#if SURFACE_ROUNDING_PROFILE == 3
                        for(unsigned repeat=0;repeat<SURFACE_NOISE_REPEATS;repeat++)
                            field_noise_apply(&noise,f,&bytes[current],ticks);
#endif
                        if(smoothing) {for(unsigned i=0;i<4;i++)inputs[i]=bytes[current].read;
                            round_draw(f,filter,&offsets[2][0][0],&cb[2],inputs,4,&bytes[current^1]);current^=1;}
                        for(unsigned i=0;i<4;i++)inputs[i]=bytes[current].read;
                        round_draw(f,feedback[0],&offsets[0][0][0],&cb[0],inputs,4,&first_byte);
                        CHECK(pc_surface_gpu_begin_bytes(surface,bytes[current].read,first_byte.read));
                    }
                    CHECK(pc_surface_gpu_advance(surface,part->delta));
                    if(part->ends_tick) {
                        inputs[0]=first_byte.read;inputs[1]=bytes[current].read;
                        round_draw(f,feedback[1],&offsets[1][0][0],&cb[1],inputs,2,&first_float);
                        if(!SURFACE_BENCHMARK) {
                            field_read(first_float.image,0,exact);field_read(world.image,0,field_b);
                            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<3;channel++) {
                                double limited=fmin(channel==1?1:d,fmax(0,field_b[i*4+channel-1]+(channel==1?.5f:0)));
                                arithmetic_error=fmax(arithmetic_error,fabs(limited-exact[i*4+channel]));
                            }
                        }
                        inputs[0]=world.read;inputs[1]=first_float.read;
                        round_draw(f,boundary,NULL,NULL,inputs,2,&bytes[current]);
                        if(!SURFACE_BENCHMARK) {
                            field_read(bytes[current].image,2,field_a);
                            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<3;channel++) {
                                double error=fabs(field_a[i*4+channel]-references[(ticks*SURFACE_SIDE*SURFACE_SIDE+i)*4+channel]);
                                if(error && !worst)printf("first-rounded-difference tick=%u cell=%u channel=%u got=%.9g expected=%.9g candidate_float=%.15g original_float=%.15g\n",
                                    ticks,i,channel,field_a[i*4+channel],references[(ticks*SURFACE_SIDE*SURFACE_SIDE+i)*4+channel],field_b[i*4+channel-1]+(channel==1?.5f:0),exact[i*4+channel]);
                                worst=fmax(worst,error);
                            }
                        }
                        ticks++;
                        round_draw(f,unpack,NULL,NULL,&bytes[current].read,1,&world);
                    }
                }
                if(SURFACE_BENCHMARK) {
                    field_final_run(f,world.read,1,1,NULL);
                    surface_timing_mark(&timing,frame,1);continue;
                }
                field_final_run(f,world.read,1,1,field_b);unsigned changed=0;
                for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<2;channel++)
                    changed|=field_b[i*4+channel]!=normal[i*4+channel];
                fresh+=changed;memcpy(normal,field_b,sizeof normal);
            }
            surface_timing_finish(&timing,"candidate",smoothing,fps,rate==4);
            field_final_run(f,world.read,1,1,field_a);
            rhi_image_update(f->packed,0,NULL,references+59*SURFACE_SIDE*SURFACE_SIDE*4,SURFACE_SIDE*16,sizeof field_a);
            field_final_run(f,f->packed_read,0,1,field_b);double final_error=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<3;channel++)
                final_error=fmax(final_error,fabs(field_a[i*4+channel]-field_b[i*4+channel]));
            if(SURFACE_BENCHMARK)
                printf("surface-timing-endpoint profile=%u smoothing=%u fps=%u jitter=%u final_byte_error=%.9g (intermediate accuracy/freshness not sampled)\n",SURFACE_ROUNDING_PROFILE,smoothing,fps,rate==4,final_error);
            else printf("rounded-flow profile=%u smoothing=%u fps=%u jitter=%u ticks=%u fresh_byte_ripples=%u max_byte_state_error=%.9g max_arithmetic_error=%.9g final_byte_error=%.9g\n",SURFACE_ROUNDING_PROFILE,smoothing,fps,rate==4,ticks,fresh,worst,arithmetic_error,final_error);
            CHECK(worst==0);CHECK(ticks==60 && clock.game_remainder==0);
            if(!SURFACE_BENCHMARK)printf("rounded-freshness reference60=%u candidate=%u frames=%u\n",reference_fresh,fresh,frames);
            /* A saturated/quantized original field can repeat final colors.
             * Preserve those repeats at 60; higher rates must add fresh frames.
             * Mild/impulse cases retain the stricter every-frame requirement. */
            if(!SURFACE_BENCHMARK) {
                CHECK(fps==60?fresh==reference_fresh:fresh>reference_fresh);
                if(SURFACE_ROUNDING_PROFILE<2 || fps!=60)CHECK(fresh==frames);
            }
            CHECK(final_error==0);
        }
    }
#ifdef SURFACE_LIVE_CAPTURE
#include "surface_capture_gpu.inc"
#endif
    free(references);for(unsigned i=0;i<2;i++)round_destroy(&bytes[i]);
    pc_surface_gpu_cache_clear(&cache);
#if SURFACE_ROUNDING_PROFILE == 3
    field_noise_destroy(&noise);
#endif
    round_destroy(&first_byte);round_destroy(&first_float);round_destroy(&rounding);
    rhi_shader_destroy(filter);rhi_shader_destroy(unpack);rhi_shader_destroy(residual);rhi_shader_destroy(boundary);rhi_shader_destroy(guard);
}
