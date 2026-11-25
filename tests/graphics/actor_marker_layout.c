/* Continuous ratio sweep: the name and mirrored skull halves share one
 * world-projected centre and the same uniform physical-pixel scale. */
#include <stdio.h>
#include <math.h>
#include "pc_ui_layout.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    unsigned cases=0;
    const float names[]={0.f,8.f,64.f,180.f};
    for(unsigned w=320;w<=7680;w+=17)for(unsigned h=240;h<=4320;h+=31)
    for(unsigned n=0;n<4;n++)for(unsigned pos=0;pos<3;pos++) {
        float ratio=4.f*h/(3.f*w),sx=pc_ui_scale_x(ratio),sy=pc_ui_scale_y(ratio);
        float cx=pos?pos==1?320.f:620.f:20.f,cy=pos?pos==1?240.f:460.f:20.f;
        float pixel_scale=fminf(w/640.f,h/480.f),inner[2];
        for(unsigned right=0;right<2;right++) {
            float edge=cx+(right?.5f:-.5f)*names[n];
            float xy[]={edge+(right?12.f:-12.f),cy-18.f,edge,cy+18.f};
            pc_ui_actor_icon_shape(xy,ratio,names[n],right);
            CHECK(fabsf(fabsf(xy[2]-xy[0])*w/640.f-12.f*pixel_scale)<.01f);
            CHECK(fabsf((xy[3]-xy[1])*h/480.f-36.f*pixel_scale)<.01f);
            CHECK(fabsf((xy[1]+xy[3])*.5f-cy)<.001f);
            inner[right]=xy[2]*w/640.f;
            float text_edge=(cx+(edge-cx)*sx)*w/640.f;
            CHECK(fabsf(inner[right]-text_edge)<.01f);
        }
        CHECK(fabsf((inner[0]+inner[1])*.5f-cx*w/640.f)<.01f);
        CHECK(fabsf(inner[1]-inner[0]-names[n]*pixel_scale)<.01f);
        CHECK(fabsf(sx*w/640.f-sy*h/480.f)<.001f);
        cases++;
    }
    printf("PASS: %u name/skull layouts across wide, tall and arbitrary ratios\n",cases);
    return 0;
}
