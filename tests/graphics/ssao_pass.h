#include "graphics/ssao_pass.h"
#include "graphics/ssao_projection.h"
#include "graphics/ssao_settings.h"
static void test_ssao_pass(RhiView *restore_target)
{
    TsfpSsaoPass pass={0};
    const float a=100.0f/99.9f,b=-10.0f/99.9f;
    float inverse[16]={1,0,0,0,0,1,0,0,0,0,0,1/b,0,0,1,-a/b};
    float lens[16]={1,0,0,0,0,1,0,0,0,0,a,1,0,0,b,0},computed[16];
    CHECK(tsfp_ssao_inverse(lens,computed));
    for(unsigned i=0;i<16;i++)CHECK(fabsf(computed[i]-inverse[i])<.00001f);
    lens[11]=0;CHECK(!tsfp_ssao_inverse(lens,computed));lens[11]=1;
    lens[0]=0;CHECK(!tsfp_ssao_inverse(lens,computed));lens[0]=NAN;
    CHECK(!tsfp_ssao_inverse(lens,computed));
    for(unsigned shape=0;shape<2;shape++) {
        unsigned width=shape?65:64,height=shape?33:64;
        unsigned hw=(width+1)/2,hh=(height+1)/2;
        float *input=malloc(width*height*sizeof(float));
        float *pixels=malloc(hw*hh*2*sizeof(float));CHECK(input&&pixels);
        for(unsigned scene=0;scene<5;scene++) {
            for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
                input[y*width+x]=scene==2?1.0f:a+b/((x<width/2 && scene==1)?9.5f:(x<width/2 && scene==3)?2.f:(scene==4)?(x<width/2?1.85f:2.f):10.0f);
            RhiImageDesc desc={0};desc.type=RHI_IMAGE_2D;desc.width=width;desc.height=height;
            desc.depth=desc.mip_levels=desc.samples=1;desc.format=RHI_FORMAT_R32_FLOAT;desc.bind=RHI_BIND_SAMPLED;
            RhiSubresourceData initial={input,width*sizeof(float),width*height*sizeof(float)};
            RhiImage *image=rhi_image_create(&desc,&initial);CHECK(image);
            RhiView *srv=rhi_view_create(image,RHI_VIEW_SAMPLED,NULL);CHECK(srv);
            rhi_set_render_target(restore_target,NULL);
            CHECK(tsfp_ssao_render(&pass,srv,width,height,inverse,1,
                scene>=3?TSFP_SSAO_RADIUS:2,scene>=3?TSFP_SSAO_BIAS:.02f,
                scene>=3?TSFP_SSAO_STRENGTH:3));
            CHECK(pass.width==hw&&pass.height==hh);
            RhiOutputState state;rhi_output_save(&state);
            CHECK(rhi_output_color_is(&state,restore_target));rhi_output_restore(&state);
            CHECK(rhi_image_readback(pass.image,0,pixels,hw*2*sizeof(float))==0);
            unsigned dark=0;
            for(unsigned i=0;i<hw*hh;i++) {
                CHECK(pixels[i*2]>=0&&pixels[i*2]<=1);
                if(scene!=1&&scene!=4) {
                    if(!(fabsf(pixels[i*2]-1)<.0001f))
                        fprintf(stderr,"AO mismatch: shape=%u scene=%u pixel=%u/%u visibility=%.9g depth=%.9g\n",
                            shape,scene,i,hw*hh,pixels[i*2],pixels[i*2+1]);
                    CHECK(fabsf(pixels[i*2]-1)<.0001f);
                }
                if(pixels[i*2]<.995f)dark++;
                if(scene==2)CHECK(pixels[i*2+1]==0);
                else CHECK(pixels[i*2+1]>(scene==4?1.84f:scene==3?1.99f:9.49f) && pixels[i*2+1]<10.01f);
            }
            if(scene==1||scene==4)CHECK(dark>0&&dark<hw*hh/2);
            float *color=malloc(width*height*4*sizeof(float));
            float *result=malloc(width*height*4*sizeof(float));CHECK(color&&result);
            for(unsigned i=0;i<width*height;i++) {
                color[i*4]=.8f;color[i*4+1]=.4f;color[i*4+2]=.2f;
                color[i*4+3]=(i%7)/8.0f;
            }
            desc.format=RHI_FORMAT_R32G32B32A32_FLOAT;
            initial.data=color;initial.row_pitch=width*4*sizeof(float);initial.slice_pitch=width*height*4*sizeof(float);
            RhiImage *color_image=rhi_image_create(&desc,&initial);CHECK(color_image);
            RhiView *color_view=rhi_view_create(color_image,RHI_VIEW_SAMPLED,NULL);CHECK(color_view);
            desc.bind=RHI_BIND_RENDER_TARGET;
            RhiImage *composite=rhi_image_create(&desc,NULL);CHECK(composite);
            RhiView *out=rhi_view_create(composite,RHI_VIEW_RENDER_TARGET,NULL);CHECK(out);
            CHECK(tsfp_ssao_composite(&pass,color_view,srv,out,width,height));
            CHECK(rhi_image_readback(composite,0,result,width*4*sizeof(float))==0);
            unsigned shaded=0;
            for(unsigned i=0;i<width*height;i++) {
                CHECK(result[i*4+3]==color[i*4+3]);
                if(scene!=1&&scene!=4)for(unsigned c=0;c<3;c++)CHECK(fabsf(result[i*4+c]-color[i*4+c])<.00001f);
                CHECK(result[i*4]>=0 && result[i*4]<=.80001f);
                CHECK(fabsf(result[i*4+1]*2-result[i*4])<.00001f);
                if(result[i*4]<.795f)shaded++;
                // The foreground plane must not inherit the background contact shadow.
                if(scene==1 && i%width<width/2)CHECK(fabsf(result[i*4]-.8f)<.0001f);
            }
            if(scene==1||scene==4)CHECK(shaded>0&&shaded<width*height/2);
            // In-place multiplicative path is what the game uses before HUD drawing.
            float white[4]={.8f,.4f,.2f,.625f};rhi_clear_color(out,white);
            CHECK(tsfp_ssao_apply(&pass,srv,out,width,height));
            CHECK(rhi_image_readback(composite,0,result,width*4*sizeof(float))==0);
            for(unsigned i=0;i<width*height;i++) {
                CHECK(result[i*4+3]==.625f);
                if(scene!=1&&scene!=4)CHECK(fabsf(result[i*4]-.8f)<.00001f);
                CHECK(result[i*4]>=0&&result[i*4]<=.80001f);
            }
            rhi_view_destroy(out);rhi_image_destroy(composite);
            rhi_view_destroy(color_view);rhi_image_destroy(color_image);free(result);free(color);
            rhi_view_destroy(srv);rhi_image_destroy(image);
        }
        free(pixels);free(input);
    }
    // A viewport offset in both axes must not sample or shade its neighbours.
    float split_depth[96*64],split_color[96*64*4];
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<96;x++)
        split_depth[y*96+x]=(x>=32&&x<80&&y>=16&&y<48)?a+b/(x<56?9.5f:10.f):.1f;
    RhiImageDesc split_desc={0};split_desc.type=RHI_IMAGE_2D;split_desc.width=96;split_desc.height=64;
    split_desc.depth=split_desc.mip_levels=split_desc.samples=1;split_desc.format=RHI_FORMAT_R32_FLOAT;
    split_desc.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData split_initial={split_depth,96*sizeof(float),sizeof split_depth};
    RhiImage *split_source=rhi_image_create(&split_desc,&split_initial);CHECK(split_source);
    RhiView *split_srv=rhi_view_create(split_source,RHI_VIEW_SAMPLED,NULL);CHECK(split_srv);
    split_desc.format=RHI_FORMAT_R32G32B32A32_FLOAT;split_desc.bind=RHI_BIND_RENDER_TARGET;
    RhiImage *split_target=rhi_image_create(&split_desc,NULL);CHECK(split_target);
    RhiView *split_rtv=rhi_view_create(split_target,RHI_VIEW_RENDER_TARGET,NULL);CHECK(split_rtv);
    const float base_color[4]={.8f,.4f,.2f,.625f};rhi_clear_color(split_rtv,base_color);
    CHECK(tsfp_ssao_render_region(&pass,split_srv,32,16,48,32,inverse,1,2,.02f,3));
    CHECK(pass.width==24&&pass.height==16);
    CHECK(tsfp_ssao_apply(&pass,split_srv,split_rtv,48,32));
    CHECK(rhi_image_readback(split_target,0,split_color,96*4*sizeof(float))==0);
    unsigned split_shaded=0;
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<96;x++) {
        float *pixel=split_color+(y*96+x)*4;
        CHECK(pixel[3]==.625f);
        if(x<32||x>=80||y<16||y>=48)CHECK(memcmp(pixel,base_color,sizeof base_color)==0);
        else if(pixel[0]<.795f)split_shaded++;
    }
    CHECK(split_shaded>0&&split_shaded<48*32/2);
    CHECK(!tsfp_ssao_render_region(&pass,split_srv,90,0,10,10,inverse,1,2,.02f,3));
    rhi_view_destroy(split_rtv);rhi_view_destroy(split_srv);rhi_image_destroy(split_target);rhi_image_destroy(split_source);
    tsfp_ssao_destroy(&pass);
    CHECK(!pass.image&&!pass.vs&&!pass.width);
    puts("PASS: half-resolution SSAO contact, flat surface, sky and resize");
}
