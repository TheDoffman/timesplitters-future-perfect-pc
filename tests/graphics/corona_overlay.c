/* Exercise production placement with the deferred guest sprite batch. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
static void corona_draw(void) { pending=1;g_esp+=4; }
int main(void)
{
    const float ratios[]={1.f,.75f,24.f/43.f,.375f,1.25f};
    const uint32_t calls[]={0x17941,0x179bc};
    const float bounds[][4]={{156,217,170,231},{-8788.69f,-8727.82f,9115.31f,9176.18f}};
    for(unsigned r=0;r<5;r++)for(unsigned c=0;c<2;c++)for(unsigned b=0;b<2;b++)for(int scope=0;scope<5;scope++) {
        hud_scale=ratios[r];wide=hud_scale!=1;players=1;view[0]=0;
        pending=-1;draw_count=selection_count=0;g_esp=0;
        ui_place(scope,0xabc);pending=1;
        stack[0]=calls[c];
        /* A near light may extend beyond the screen. Preserve its projected
           size rather than hiding the clipped edges by shrinking the halo. */
        memcpy(stack+1,bounds[b],sizeof bounds[b]);
        stack[5]=stack[6]=0;stack[7]=stack[8]=1024;
        stack[9]=c?0x7f7f7fa6:0x6e6af2a6;
        uint32_t saved[32];memcpy(saved,stack,sizeof saved);
        pc_display_aim_primitive(corona_draw,0);
        CHECK(g_esp==4 && placement==scope && tag==0xabc);
        CHECK(!memcmp(saved,stack,sizeof saved));
        if(wide)CHECK(draw_count==2 && drawn[0]==scope && drawn[1]==1 && pending<0);
        else CHECK(draw_count==0 && pending==1);
        pending=1;sub_00022FF0();CHECK(drawn[draw_count-1]==scope);
    }
    /* Adjacent sprite calls, pointer primitives and split views keep their
       established policy. No guest state or HUD size is changed. */
    for(unsigned guard=0;guard<5;guard++) {
        wide=1;players=guard==2?2:1;view[0]=guard==3?10:0;
        pending=-1;g_esp=0;selection_count=0;ui_place(2,0xabc);
        stack[0]=guard==0?0x17942:0x17941;
        int before=selection_count;
        pc_display_aim_primitive(corona_draw,guard==1?1:guard==4?2:0);
        CHECK(selection_count==before && placement==2 && tag==0xabc && g_esp==4);
        sub_00022FF0();
    }
    puts("PASS: world coronas retain geometry/UV/colour and flush full-view layers");
    return 0;
}
