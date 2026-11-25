/* Actual shared display-resolve shader: execute the same pixel checks on
 * Windows, native Mac and Linux. Caller owns the live RHI device. */
#ifndef TSFP_COLOR_GRADE_PIXELS_H
#define TSFP_COLOR_GRADE_PIXELS_H
#include <assert.h>
#include <math.h>
/* The state-cache fixture supplies its own public policy stub. Keep the real
 * display implementation's policy local to this resolve fixture. */
#define d3d8_display_policy color_grade_display_policy
#include "d3d/d3d8_display.c"
#undef d3d8_display_policy
static void draw_color_grade(RhiView *src,RhiView *out,RhiImage *target,int enabled,unsigned char pixel[4])
{
    D3D8DisplayFit fit={0,0,1,1};
    assert(SUCCEEDED(display_resolve_original(src,1,1,out,fit,0,enabled)));
    assert(!rhi_image_readback(target,0,pixel,4));
}
static void rhi_color_grade_pixels(void)
{
    unsigned char input[4]={64,128,192,255},pixel[4];
    RhiImageDesc desc={0};desc.type=RHI_IMAGE_2D;desc.width=desc.height=desc.depth=desc.mip_levels=desc.samples=1;
    desc.format=RHI_FORMAT_R8G8B8A8_UNORM;desc.bind=RHI_BIND_SAMPLED;
    RhiSubresourceData data={input,4,4};
    RhiImage *source=rhi_image_create(&desc,&data);assert(source);
    RhiView *srv=rhi_view_create(source,RHI_VIEW_SAMPLED,NULL);assert(srv);
    desc.bind=RHI_BIND_RENDER_TARGET;RhiImage *target=rhi_image_create(&desc,NULL);assert(target);
    RhiView *rtv=rhi_view_create(target,RHI_VIEW_RENDER_TARGET,NULL);assert(rtv);
    /* Intermediate native-HUD/post-process passes stay exactly neutral. */
    draw_color_grade(srv,rtv,target,0,pixel);assert(!memcmp(input,pixel,4));
    draw_color_grade(srv,rtv,target,1,pixel);
    float v[3];for(unsigned c=0;c<3;c++)v[c]=(input[c]/255.f-.5f)*1.09f+.51f;
    float l=v[0]*.299f+v[1]*.587f+v[2]*.114f;
    for(unsigned c=0;c<3;c++) {
        float value=fminf(1.f,fmaxf(0.f,l+(v[c]-l)*1.12f));
        assert(abs(pixel[c]-(int)lroundf(value*255.f))<=1);
    }
    assert(pixel[3]==255 && memcmp(input,pixel,3));
    /* A later intermediate resolve cannot apply the grade a second time. */
    unsigned char graded[4];memcpy(graded,pixel,4);
    rhi_set_render_target(NULL,NULL);
    rhi_image_update(source,0,NULL,graded,4,4);
    draw_color_grade(srv,rtv,target,0,pixel);assert(!memcmp(graded,pixel,4));
    for(unsigned white=0;white<=1;white++) {
        unsigned char extreme[4]={(unsigned char)(255*white),(unsigned char)(255*white),(unsigned char)(255*white),73};
        rhi_set_render_target(NULL,NULL);rhi_image_update(source,0,NULL,extreme,4,4);
        draw_color_grade(srv,rtv,target,1,pixel);assert(!memcmp(extreme,pixel,4));
    }
    rhi_set_render_target(NULL,NULL);d3d8_display_shutdown();
    rhi_view_destroy(srv);rhi_view_destroy(rtv);rhi_image_destroy(source);rhi_image_destroy(target);
    puts("PASS: permanent color grade pixels, neutral intermediate passes, alpha and no double grading");
}
#endif
