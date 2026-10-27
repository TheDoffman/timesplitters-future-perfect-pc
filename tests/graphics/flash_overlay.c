/* Run the production white-wash group with deferred draws either side. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
static uint32_t before[32];
static unsigned calls;
static int expected;
static void white_wash(void)
{
    CHECK(placement==expected && !memcmp(stack,before,sizeof stack));
    if(wide && expected==1)CHECK(tag==0x000f90d0);
    calls++;pending=1;g_esp+=4;
}
int main(void)
{
    const unsigned sizes[][2]={{640,480},{1280,960},{1280,1024},{1920,1080},
        {1920,1200},{3440,1440},{5120,1440},{7680,4320},{1080,1920}};
    /* Every intensity, both guest viewport conventions and existing UI anchors.
       Geometry/colour/blending remain in the unmodified original function. */
    for(unsigned m=0;m<9;m++)for(unsigned a=0;a<256;a++)
    for(unsigned inclusive=0;inclusive<2;inclusive++)for(int old=0;old<=5;old++) {
        draw_count=selection_count=0;pending=-1;g_esp=0;
        wide=sizes[m][0]*3!=sizes[m][1]*4;
        memset(stack,0,sizeof stack);stack[0]=0x119913;stack[1]=a;
        view[1]=inclusive?640:639;view[6]=inclusive?480:479;
        memcpy(before,stack,sizeof before);ui_place(old,123);pending=1;
        unsigned n=calls;expected=wide?1:old;
        pc_display_flash_overlay(white_wash);
        CHECK(calls==n+1 && g_esp==4 && !memcmp(stack,before,sizeof stack));
        CHECK(placement==old && tag==123);
        if(wide)CHECK(draw_count==2 && drawn[0]==old && drawn[1]==1 && pending<0);
        else CHECK(draw_count==0 && pending==1 && selection_count==1);
        sub_00022FF0();pending=1;sub_00022FF0();CHECK(drawn[draw_count-1]==old);
    }
    /* No stretching across a split-screen boundary or invalid viewport. */
    wide=1;expected=2;view[1]=639;view[6]=479;
    const unsigned offsets[]={0,1,5,6};
    for(unsigned i=0;i<5;i++) {
        draw_count=selection_count=0;pending=-1;g_esp=0;ui_place(2,123);
        uint32_t save=0;
        if(i<4){save=view[offsets[i]];view[offsets[i]]=100;}
        else view_address=0;
        pc_display_flash_overlay(white_wash);
        CHECK(g_esp==4 && placement==2 && tag==123 && selection_count==1 && draw_count==0);
        sub_00022FF0();
        if(i<4)view[offsets[i]]=save;else view_address=0x100;
    }
    puts("PASS: white flash fills output, preserves intensity/batches and excludes partial views");
    return 0;
}
