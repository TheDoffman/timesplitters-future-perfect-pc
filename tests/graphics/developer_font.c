#include "pc_developer_font.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned char regular[1024*104*4],bold[1024*104*4];
int main(void) {
    PcDeveloperFontMetrics a=pc_developer_font_measure("Developer Tools",20,400,PC_FONT_UI);
    PcDeveloperFontMetrics b=pc_developer_font_measure("Developer Tools",20,600,PC_FONT_UI);
    assert(fabsf(a.advance-142.978515625f)<.002f);
    assert(fabsf(pc_developer_font_measure_spaced("Developer Tools",20,600,PC_FONT_UI,-.3f).advance-144.8361f)<.01f);
    assert(a.advance>100&&a.advance<180&&a.ascent>0&&a.descent<0&&b.advance>100);
    float fractional=pc_developer_font_measure("PC Settings",13.5f,600,PC_FONT_UI).advance;
    float integer=pc_developer_font_measure("PC Settings",13,600,PC_FONT_UI).advance;
    assert(fabsf(fractional/integer-13.5f/13)<.0001f);
    assert(pc_developer_font_measure("1111",17,700,PC_FONT_MONO).advance==pc_developer_font_measure("8888",17,700,PC_FONT_MONO).advance);
    unsigned used=0;
    assert(pc_developer_font_bitmap("Developer Tools",regular,1024,28,&used,20,400,PC_FONT_UI,0));
    assert(pc_developer_font_bitmap("Developer Tools",bold,1024,28,&used,20,600,PC_FONT_UI,0));
    assert(memcmp(regular,bold,sizeof regular));
    unsigned lit=0;for(unsigned i=0;i<1024*28;i++)lit+=bold[i*4]>0;assert(lit>100);
    assert(!pc_developer_font_bitmap("bad",bold,1024,28,&used,NAN,400,PC_FONT_UI,0));
    char fitted[160];pc_developer_font_fit("Long label with UTF-8 \xc3\x97",12,400,PC_FONT_UI,45,fitted,sizeof fitted);
    assert(strstr(fitted,"\xe2\x80\xa6")&&pc_developer_font_measure(fitted,12,400,PC_FONT_UI).advance<=45);
    pc_developer_font_fit("Fits",12,400,PC_FONT_UI,100,fitted,sizeof fitted);assert(!strcmp(fitted,"Fits"));
    printf("Shared fonts: regular %.3f / semibold %.3f; fractional metrics, real glyphs, numeric spacing and ellipsis passed\n",a.advance,b.advance);
    return 0;
}
