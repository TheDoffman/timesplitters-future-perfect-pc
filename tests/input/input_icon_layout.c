#include <stdio.h>
#include <stdlib.h>
#include "../src/interface/pc_input_icon_layout.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void)
{
 float classic=pc_input_icon_canvas(1440,1080,20.f,28.f,0);
 float wide=pc_input_icon_canvas(1920,1080,20.f,28.f,0);
 float ultra=pc_input_icon_canvas(2520,1080,20.f,28.f,0);
 float super=pc_input_icon_canvas(3840,1080,20.f,28.f,0);
 CHECK(classic<wide && wide<=ultra && ultra<=super);
 CHECK(fabsf(classic*2.25f/4.f-roundf(classic*2.25f/4.f))<.001f);
 CHECK(pc_input_icon_canvas(3840,1080,22.f,23.f,0)<=23.f);
 CHECK(pc_input_icon_canvas(1440,1080,16.f,16.f,1)==pc_input_icon_canvas(3840,1080,16.f,16.f,1));
 CHECK(pc_input_icon_aspect(1280,800)>pc_input_icon_aspect(640,480));
 CHECK(pc_input_icon_canvas(0,0,20.f,28.f,0)==20.f);
 CHECK(pc_input_icon_canvas(1920,1080,0.f,20.f,0)==0.f);
 CHECK(pc_input_icon_canvas(1920,1080,20.f,0.f,0)==0.f);
 float fitted=pc_input_icon_fit(31.529f,32.f,1080);CHECK(fabsf(fitted*2.25f-roundf(fitted*2.25f))<.001f && fitted<=31.529f);
 CHECK(pc_input_icon_fit(32.f,0.f,1080)==0.f);
 float a=pc_input_icon_align(7.13f,1080);CHECK(fabsf(a*2.25f-roundf(a*2.25f))<.001f);
 CHECK(fabsf(pc_input_icon_canvas(2880,2160,20.f,28.f,0)-classic)<1.f);
 puts("PASS: aspect families, resolution-aware pixel steps, compact clip, row limits and pixel alignment");return 0;
}
