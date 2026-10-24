/* Measured first-pass clipping and rounding are held over each authored interval. Fractional
 * dynamics still execute on every frame. Both quantization points use actual
 * byte render targets, not a CPU approximation to UNORM conversion. */
#include "pc_native_timing.h"
#include "graphics/surface_interval.h"
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
#include "graphics/surface_coupled_shader.h"
    const char forced_source[]="Texture2D state:register(t0);Texture2D rows0:register(t1);"
        "Texture2D rows1:register(t2);Texture2D residual:register(t3);"
        TSFP_SURFACE_COUPLED_SHADER TSFP_SURFACE_FORCED_SHADER
        "float4 main(float4 p:SV_POSITION):SV_TARGET {int3 at=int3(p.xy,0);"
        "return float4(surface_forced(state.Load(at).xy,rows0.Load(at),rows1.Load(at),residual.Load(at).x),0,1);}";
    RhiShader *forced=shader(RHI_STAGE_PIXEL,forced_source,"ps_5_0");
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
    RoundTarget world=round_target(RHI_FORMAT_R32G32B32A32_FLOAT),rounding=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
    RoundTarget force_spectrum=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
    RoundTarget states[2]={round_target(RHI_FORMAT_R32G32B32A32_FLOAT),round_target(RHI_FORMAT_R32G32B32A32_FLOAT)};
    /* Exercise both guards before the long trajectory: only a nearby original
     * result at a byte threshold may resolve ambiguity. Never replace a distant
     * state or perturb an unambiguous value merely because a reference exists. */
    const float test_value[]={.5f,.5f,.52f,1.5f,-.5f};
    const float test_exact[]={.500001f,.75f,.520001f,.5f,0};
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++) {
        unsigned test=i%5;field_a[i*4]=test_value[test]-.5f;field_a[i*4+1]=test_value[test];
        field_a[i*4+2]=0;field_a[i*4+3]=1;
        for(unsigned c=0;c<4;c++)field_b[i*4+c]=test_exact[test];
    }
    rhi_image_update(world.image,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
    rhi_image_update(first_float.image,0,NULL,field_b,SURFACE_SIDE*16,sizeof field_b);
    RhiView *guard_inputs[2]={world.read,first_float.read};
    round_draw(f,guard,NULL,NULL,guard_inputs,2,&rounding);field_read(rounding.image,0,field_a);
    for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<4;channel++) {
        unsigned test=i%5;float expected=test==0?test_exact[test]:test_value[test];
        float boundary_error=fabsf(field_a[i*4+channel]-expected);CHECK(boundary_error<1e-7f);
    }
    float *references=malloc(60*sizeof field_a);CHECK(references);
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
    const unsigned rates[]={60,120,144,0};
    for(unsigned smoothing=0;smoothing<2;smoothing++) {
        unsigned reference_fresh=0;memset(normal,0,sizeof normal);
        rhi_image_update(world.image,0,NULL,initial,SURFACE_SIDE*16,sizeof initial);
        round_draw(f,f->pack,NULL,NULL,&world.read,1,&bytes[0]);unsigned current=0;
        /* Independent original 60-Hz path with real byte initial/feedback state. */
        for(unsigned tick=0;tick<60;tick++) {
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
            field_read(bytes[current].image,2,references+tick*SURFACE_SIDE*SURFACE_SIDE*4);
            field_final_run(f,bytes[current].read,0,1,field_b);unsigned changed=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<2;channel++)
                changed|=field_b[i*4+channel]!=normal[i*4+channel];
            reference_fresh+=changed;memcpy(normal,field_b,sizeof normal);
        }
        for(unsigned rate=0;rate<4;rate++) {
            unsigned fps=rates[rate],ticks=0,fresh=0;double worst=0,arithmetic_error=0;
            /* Use the actual clock producer and shared interval splitter. This
             * frequency represents all presets; CPU coverage adds odd clocks,
             * jitter and catch-up. Coefficient cache keys are fixture-only. */
            const int64_t frequency=72000000;int64_t now=0;PcNativeTiming clock={0};
            pc_native_timing_step(&clock,now,frequency,144,0);
            unsigned frames=fps?fps:117;
            RoundTarget rows[13][2]={0};
            rhi_image_update(world.image,0,NULL,initial,SURFACE_SIDE*16,sizeof initial);
            round_draw(f,f->pack,NULL,NULL,&world.read,1,&bytes[0]);current=0;
            memset(normal,0,sizeof normal);
            for(unsigned frame=0;frame<frames;frame++) {
                unsigned frame_rate=fps?fps:(frame<37?144:frame<67?120:frame<82?60:144);
                if(frame==10) {
                    now+=frequency/2;
                    PcNativeStep paused=pc_native_timing_step(&clock,now,frequency,frame_rate,1);
                    PcSurfaceInterval stopped;
                    CHECK(pc_surface_interval_split(paused.units_per_tick,paused.game_phase,paused.game_units,&stopped));
                    CHECK(!stopped.count && stopped.end_phase==clock.game_remainder);
                    field_final_run(f,world.read,1,1,field_b);
                    CHECK(!memcmp(normal,field_b,sizeof normal));
                }
                now+=frequency/frame_rate;
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
                        inputs[0]=first_byte.read;inputs[1]=bytes[current].read;
                        round_draw(f,residual,NULL,NULL,inputs,2,&rounding);
                        field_gpu_transform(rounding.read,force_spectrum.write,0);
                        round_draw(f,unpack,NULL,NULL,&bytes[current].read,1,&world);
                        field_gpu_transform(world.read,states[0].write,0);
                    }
                    CHECK(part->units%(frequency/12)==0);
                    unsigned piece=(unsigned)(part->units/(frequency/12));
                    CHECK(piece>0 && piece<=12);
                    if(!rows[piece][0].image) {
                        rows[piece][0]=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
                        rows[piece][1]=round_target(RHI_FORMAT_R32G32B32A32_FLOAT);
                        for(unsigned row=0;row<2;row++) {
                            for(unsigned y=0;y<SURFACE_SIDE;y++)for(unsigned x=0;x<SURFACE_SIDE;x++) {
                                double dc=(!x && !y)?SURFACE_SIDE:0;PcSurfaceCoupledStep m;
                                CHECK(pc_surface_coupled_forced_step(a,b,d,k+pc_surface_stencil_shift(SURFACE_SIDE,SURFACE_SIDE,x,y,PC_SURFACE_WRAP),
                                    dc*(c-.5+k*c),dc*(c-.5),part->delta,&m));
                                memcpy(field_a+(y*SURFACE_SIDE+x)*4,m.row[row],16);
                            }
                            rhi_image_update(rows[piece][row].image,0,NULL,field_a,SURFACE_SIDE*16,sizeof field_a);
                        }
                    }
                    inputs[0]=states[0].read;inputs[1]=rows[piece][0].read;inputs[2]=rows[piece][1].read;inputs[3]=force_spectrum.read;
                    round_draw(f,forced,NULL,NULL,inputs,4,&states[1]);
                    RoundTarget swap=states[0];states[0]=states[1];states[1]=swap;
                    field_gpu_transform(states[0].read,world.write,1);
                    if(part->ends_tick) {
                        inputs[0]=first_byte.read;inputs[1]=bytes[current].read;
                        round_draw(f,feedback[1],&offsets[1][0][0],&cb[1],inputs,2,&first_float);
                        field_read(first_float.image,0,exact);field_read(world.image,0,field_b);
                        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<3;channel++) {
                            double limited=fmin(channel==1?1:d,fmax(0,field_b[i*4+channel-1]+(channel==1?.5f:0)));
                            arithmetic_error=fmax(arithmetic_error,fabs(limited-exact[i*4+channel]));
                        }
                        inputs[0]=world.read;inputs[1]=first_float.read;
                        round_draw(f,boundary,NULL,NULL,inputs,2,&bytes[current]);
                        field_read(bytes[current].image,2,field_a);
                        for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=1;channel<3;channel++) {
                            double error=fabs(field_a[i*4+channel]-references[(ticks*SURFACE_SIDE*SURFACE_SIDE+i)*4+channel]);
                            if(error && !worst)printf("first-rounded-difference tick=%u cell=%u channel=%u got=%.9g expected=%.9g candidate_float=%.15g original_float=%.15g\n",
                                ticks,i,channel,field_a[i*4+channel],references[(ticks*SURFACE_SIDE*SURFACE_SIDE+i)*4+channel],field_b[i*4+channel-1]+(channel==1?.5f:0),exact[i*4+channel]);
                            worst=fmax(worst,error);
                        }
                        ticks++;
                        round_draw(f,unpack,NULL,NULL,&bytes[current].read,1,&world);
                    }
                }
                field_final_run(f,world.read,1,1,field_b);unsigned changed=0;
                for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<2;channel++)
                    changed|=field_b[i*4+channel]!=normal[i*4+channel];
                fresh+=changed;memcpy(normal,field_b,sizeof normal);
            }
            field_final_run(f,world.read,1,1,field_a);
            rhi_image_update(f->packed,0,NULL,references+59*SURFACE_SIDE*SURFACE_SIDE*4,SURFACE_SIDE*16,sizeof field_a);
            field_final_run(f,f->packed_read,0,1,field_b);double final_error=0;
            for(unsigned i=0;i<SURFACE_SIDE*SURFACE_SIDE;i++)for(unsigned channel=0;channel<3;channel++)
                final_error=fmax(final_error,fabs(field_a[i*4+channel]-field_b[i*4+channel]));
            printf("rounded-flow profile=%u smoothing=%u fps=%u ticks=%u fresh_byte_ripples=%u max_byte_state_error=%.9g max_arithmetic_error=%.9g final_byte_error=%.9g\n",SURFACE_ROUNDING_PROFILE,smoothing,fps,ticks,fresh,worst,arithmetic_error,final_error);
            CHECK(worst==0);CHECK(ticks==60 && clock.game_remainder==0);
            printf("rounded-freshness reference60=%u candidate=%u frames=%u\n",reference_fresh,fresh,frames);
            /* A saturated/quantized original field can repeat final colors.
             * Preserve those repeats at 60; higher rates must add fresh frames.
             * Mild/impulse cases retain the stricter every-frame requirement. */
            CHECK(fps==60?fresh==reference_fresh:fresh>reference_fresh);
            if(SURFACE_ROUNDING_PROFILE<2 || fps!=60)CHECK(fresh==frames);
            CHECK(final_error==0);
            for(unsigned piece=0;piece<=12;piece++)if(rows[piece][0].image){round_destroy(&rows[piece][0]);round_destroy(&rows[piece][1]);}
        }
    }
    free(references);for(unsigned i=0;i<2;i++){round_destroy(&bytes[i]);round_destroy(&states[i]);}
#if SURFACE_ROUNDING_PROFILE == 3
    field_noise_destroy(&noise);
#endif
    round_destroy(&first_byte);round_destroy(&first_float);round_destroy(&world);round_destroy(&rounding);round_destroy(&force_spectrum);
    rhi_shader_destroy(filter);rhi_shader_destroy(unpack);rhi_shader_destroy(residual);rhi_shader_destroy(forced);rhi_shader_destroy(boundary);rhi_shader_destroy(guard);
}
