/* Crane's four independently batched quadrants must share one placement. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
static uint32_t before[32];
static int expected;
static void vignette(void)
{
    CHECK(placement==expected && !memcmp(stack,before,sizeof stack));
    if(expected==1)CHECK(tag==0x00137010u);
    sub_00022FF0();
    for(unsigned i=0;i<4;i++) { pending=1; if(i<3)sub_00022FF0(); }
    g_esp+=4;
}
int main(void)
{
    const unsigned sizes[][2]={{640,480},{1280,960},{1280,1024},{1920,1080},
        {1920,1200},{3440,1440},{5120,1440},{7680,4320},{1080,1920}};
    for(unsigned m=0;m<9;m++)for(int old=0;old<=5;old++)for(unsigned edge=0;edge<2;edge++) {
        draw_count=selection_count=0;pending=-1;g_esp=0;
        wide=sizes[m][0]*3!=sizes[m][1]*4;
        hud_scale=4.f*sizes[m][1]/(3.f*sizes[m][0]);
        view[1]=edge?640:639;view[6]=edge?480:479;
        memset(stack,0,sizeof stack);stack[0]=0x1371d7;stack[1]=0x3f30a3d7;stack[2]=0x7f;
        memcpy(before,stack,sizeof before);ui_place(old,123);pending=1;
        expected=wide?1:old;
        /* The 4:3 path must not require a new tag. */
        if(!wide && old==1)ui_tag=tag=0x00137010u;
        uint32_t old_tag=tag;
        pc_display_aim_panel(vignette,0x00137010u);
        CHECK(g_esp==4 && !memcmp(stack,before,sizeof stack));
        CHECK(placement==old && tag==old_tag);
        sub_00022FF0();
        CHECK(draw_count==5 && drawn[0]==old);
        for(unsigned i=1;i<5;i++)CHECK(drawn[i]==expected);
        pending=1;sub_00022FF0();CHECK(drawn[5]==old);
    }
    /* Partial viewports, multiplayer and invalid data cannot stretch across views. */
    wide=1;expected=2;view[1]=639;view[6]=479;
    for(unsigned i=0;i<7;i++) {
        draw_count=selection_count=0;pending=-1;g_esp=0;ui_place(2,123);
        uint32_t saved[9];memcpy(saved,view,sizeof view);
        if(i==0)view[0]=20;if(i==1)view[1]=319;if(i==2)view[5]=20;
        if(i==3)view[6]=239;if(i==4)view_address=0;if(i==5)players=2;
        if(i==6)hud_scale=0;
        pc_display_aim_panel(vignette,0x00137010u);
        CHECK(placement==2 && tag==123 && selection_count==1);
        sub_00022FF0();memcpy(view,saved,sizeof view);view_address=0x100;players=1;hud_scale=.75f;
    }
    puts("PASS: crane quadrants, batch boundaries, caller restoration, nine aspect ratios and viewport guards");
    return 0;
}
