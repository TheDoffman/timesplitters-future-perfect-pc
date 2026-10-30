/* Shared GPU contract for HLSL and GLSL. Each backend supplies only a render
 * callback: (filter mode, UV scale, 64x64 RGBA float readback). */
#ifndef TSFP_MENU_SAMPLING_CASES_H
#define TSFP_MENU_SAMPLING_CASES_H
#include <math.h>
#include <stdio.h>
#include <string.h>
static void menu_sampling_texels(float texels[16*16*4])
{
    for(int y=0;y<16;y++)for(int x=0;x<16;x++) {
        int i=(y*16+x)*4;
        texels[i]=x<8?.1f:.9f;texels[i+1]=.4f;texels[i+2]=y<8?.2f:.7f;
        texels[i+3]=x<8?0.f:1.f;
    }
}
#define MENU_CHECK(x) do { if(!(x)) { fprintf(stderr,"Menu sampling contract failed at %d: %s\n",__LINE__,#x);return 0; } } while(0)
static int menu_sampling_cases(void (*render)(float,float,float *))
{
    float linear[64*64*4],cubic[64*64*4];int changed=0;
    render(0,1,linear);render(1,1,cubic);
    for(int i=0;i<64*64*4;i++) {
        MENU_CHECK(isfinite(cubic[i]));MENU_CHECK(cubic[i]>=-.00001f && cubic[i]<=1.00001f);
        if(fabsf(cubic[i]-linear[i])>.005f)changed++;
        if(i%4==1)MENU_CHECK(fabsf(cubic[i]-.4f)<.00001f);
    }
    MENU_CHECK(changed>100);
    /* The dark side of a glyph edge darkens and the bright side brightens,
       while opaque/transparent plateaus retain exactly their original alpha. */
    MENU_CHECK(cubic[(20*64+30)*4+3]<linear[(20*64+30)*4+3]);
    MENU_CHECK(cubic[(20*64+33)*4+3]>linear[(20*64+33)*4+3]);
    MENU_CHECK(cubic[(20*64+10)*4+3]==0.f && cubic[(20*64+50)*4+3]==1.f);
    render(2,1,cubic);
    MENU_CHECK(cubic[(20*64+30)*4+3]<linear[(20*64+30)*4+3]);
    MENU_CHECK(cubic[(20*64+33)*4+3]>linear[(20*64+33)*4+3]);
    for(int i=0;i<64*64*4;i++)MENU_CHECK(cubic[i]>=-.00001f && cubic[i]<=1.00001f);
    for(int scale=4;scale<=8;scale+=4) {
        render(0,(float)scale,linear);render(1,(float)scale,cubic);
        MENU_CHECK(!memcmp(linear,cubic,sizeof(linear)));
        render(2,(float)scale,cubic);MENU_CHECK(!memcmp(linear,cubic,sizeof(linear)));
    }
    return 1;
}
#undef MENU_CHECK
#endif
