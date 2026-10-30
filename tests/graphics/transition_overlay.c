/* Exercise the production fade hook with the deferred batch test harness. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
static unsigned observed;
static uint32_t before[32];
static void fade(void)
{
    CHECK(placement==1 && !memcmp(stack,before,sizeof stack));
    observed++;pending=1;g_esp+=4;
}
int main(void)
{
    const unsigned sizes[][2]={{640,480},{1280,1024},{1920,1080},{1920,1200},
        {3440,1440},{5120,1440},{7680,4320},{1080,1920}};
    const uint32_t callers[]={0x6f227,0x6f264,0x6f2f0,0x6f381,0x6f3ab,
        0xfea07,0x70755,0x1cf429,0x1cf5e5,0x2bf803,0x2bda88,0x2da4d0,0x2da715};
    const uint32_t colours[]={0,1,0x40,0x7f,0xff,0x1234567f,0x7f7f7f00,0x7f7f7f40,0x7f7f7f7f};
    for(unsigned m=0;m<8;m++)for(unsigned c=0;c<sizeof callers/sizeof callers[0];c++)for(unsigned a=0;a<sizeof colours/sizeof colours[0];a++)for(unsigned inclusive=0;inclusive<2;inclusive++) {
        /* Reset the bounded queue for every alpha step. */
        draw_count=selection_count=0;pending=-1;g_esp=0;
        hud_scale=4.f*sizes[m][1]/(3.f*sizes[m][0]);wide=hud_scale!=1.f;
        memset(stack,0,sizeof stack);stack[0]=callers[c];
        view[1]=inclusive?640:639;view[6]=inclusive?480:479;
        stack[3]=640+inclusive;stack[4]=480+inclusive;stack[5]=colours[a];
        memcpy(before,stack,sizeof before);ui_place(2,123);pending=1;
        unsigned n=observed;int handled=pc_display_transition_overlay(fade);
        CHECK(handled==wide && observed==n+(unsigned)wide);
        if(wide){CHECK(g_esp==4 && draw_count==2 && drawn[0]==2 && drawn[1]==1);}
        CHECK(placement==2 && tag==123 && !memcmp(stack,before,sizeof stack));
        sub_00022FF0();
    }
    /* Loading uses a top border, bottom border and a fade below the top.
       The actual captured geometry was 0..71, 422..480 and 71..480.
       Each must span the output without changing its alpha or vertical bounds. */
    const uint32_t bands[][3]={{0x155f8e,0,71},{0x156102,422,480},
        {0x15682d,71,480},{0x15682d,0,480}};
    for(unsigned m=0;m<8;m++)for(unsigned b=0;b<4;b++)for(unsigned a=0;a<sizeof colours/sizeof colours[0];a++) {
        draw_count=selection_count=0;pending=-1;g_esp=0;
        hud_scale=4.f*sizes[m][1]/(3.f*sizes[m][0]);wide=hud_scale!=1.f;
        memset(stack,0,sizeof stack);stack[0]=bands[b][0];stack[2]=bands[b][1];
        stack[3]=640;stack[4]=bands[b][2];stack[5]=colours[a];
        view[1]=639;view[6]=479;memcpy(before,stack,sizeof before);
        ui_place(2,123);pending=1;unsigned n=observed;
        CHECK(pc_display_transition_overlay(fade)==wide);
        CHECK(observed==n+(unsigned)wide);
        if(wide)CHECK(g_esp==4 && draw_count==2 && drawn[0]==2 && drawn[1]==1);
        CHECK(placement==2 && tag==123 && !memcmp(stack,before,sizeof stack));
        sub_00022FF0();
    }
    wide=1;pending=-1;g_esp=0;
    for(unsigned b=0;b<4;b++) {
        stack[0]=bands[b][0];stack[1]=0;stack[2]=bands[b][1];stack[3]=640;stack[4]=bands[b][2];
        view[0]=100;CHECK(!pc_display_transition_overlay(fade));view[0]=0;
        view[5]=100;CHECK(!pc_display_transition_overlay(fade));view[5]=0;
        stack[1]=10;CHECK(!pc_display_transition_overlay(fade));stack[1]=0;
        stack[3]=320;CHECK(!pc_display_transition_overlay(fade));stack[3]=640;
        stack[4]=481;CHECK(!pc_display_transition_overlay(fade));stack[4]=bands[b][2];
        stack[2]=stack[4];CHECK(!pc_display_transition_overlay(fade));
    }
    stack[0]=0x155f8e;stack[2]=10;stack[4]=71;CHECK(!pc_display_transition_overlay(fade));
    stack[0]=0x15682d;stack[2]=71;stack[4]=422;CHECK(!pc_display_transition_overlay(fade));
    stack[2]=0;
    wide=1;g_esp=0;draw_count=selection_count=0;pending=-1;
    stack[0]=0x1cf429;stack[3]=640;stack[4]=480;
    /* Known fade with partial geometry, invalid view or split viewport. */
    for(unsigned i=1;i<=4;i++) {
        uint32_t save=stack[i];stack[i]=10;
        CHECK(!pc_display_transition_overlay(fade));stack[i]=save;
    }
    const unsigned offsets[]={0,1,5,6};
    for(unsigned i=0;i<4;i++) {
        uint32_t save=view[offsets[i]];view[offsets[i]]=100;
        CHECK(!pc_display_transition_overlay(fade));view[offsets[i]]=save;
    }
    view[1]=639;stack[3]=641;CHECK(!pc_display_transition_overlay(fade));
    view[1]=640;stack[3]=642;CHECK(!pc_display_transition_overlay(fade));stack[3]=640;
    view[6]=479;stack[4]=481;CHECK(!pc_display_transition_overlay(fade));
    view[6]=480;stack[4]=482;CHECK(!pc_display_transition_overlay(fade));stack[4]=480;
    view_address=0;CHECK(!pc_display_transition_overlay(fade));view_address=0x100;
    /* Full-canvas unrelated rectangles, mattes, scopes and menu backgrounds. */
    const uint32_t other[]={0,0x6f322,0x6f34b,0x136856,0x2d90eb,0x2d94ee};
    for(unsigned i=0;i<sizeof other/sizeof other[0];i++) {
        stack[0]=other[i];CHECK(!pc_display_transition_overlay(fade));
    }
    CHECK(pending==-1 && selection_count==0 && g_esp==0);
    puts("PASS: transition fades cover output, preserve alpha/state/batches and exclude partial UI");
    return 0;
}
