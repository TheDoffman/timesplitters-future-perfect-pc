/* Included after ui_layout.c's deferred-draw harness and production hooks. */
static void actor_composite(void)
{
    /* Preserve the complete guest primitive ABI, including fixed-point UVs
       and the original neutral/translucent colour. */
    CHECK(stack[5]==0 && stack[6]==0 && stack[7]==0xff0 && stack[8]==0xff0);
    CHECK(stack[9]==0x8080807f);
    pending=1;g_esp+=4;
}
int main(void)
{
    const unsigned sizes[][2]={{640,480},{1280,720},{1920,1080},
        {3440,1440},{5120,1440},{1280,1024},{3840,2160}};
    /* Move the actor across the screen, including partly clipped bounds.
       The effect must use the world image's viewport, not the HUD viewport. */
    for(unsigned r=0;r<sizeof sizes/sizeof sizes[0];r++) {
        hud_scale=4.f*sizes[r][1]/(3.f*sizes[r][0]);wide=hud_scale!=1;
        for(int x=-20;x<=600;x+=155)for(int enclosing=0;enclosing<=4;enclosing++) {
            draw_count=selection_count=0;pending=-1;g_esp=0;
            ui_place(enclosing,0xabc);pending=1;
            stack[0]=0x1232a9;stack[1]=x;stack[2]=100;
            stack[3]=x+60;stack[4]=350;
            stack[5]=stack[6]=0;stack[7]=stack[8]=0xff0;stack[9]=0x8080807f;
            uint32_t saved[32];memcpy(saved,stack,sizeof saved);
            pc_display_weapon_effect(actor_composite,0);
            CHECK(g_esp==4 && placement==enclosing && tag==0xabc);
            CHECK(!memcmp(saved,stack,sizeof saved));
            if(wide) {
                CHECK(draw_count==2 && drawn[0]==enclosing && drawn[1]==1);
                CHECK(pending<0); /* Composite flushed before restoring HUD. */
            }
            pending=1;sub_00022FF0();
            CHECK(drawn[draw_count-1]==enclosing);
        }
    }
    /* Neighbouring blur draws, unrelated rectangles, wrong primitive kind,
       original 4:3 and split-screen retain the existing placement policy. */
    for(unsigned mode=0;mode<6;mode++) {
        wide=mode!=3;players=mode==4?2:1;view[0]=mode==5?10:0;
        pending=-1;g_esp=0;selection_count=0;ui_place(2,0xabc);
        stack[0]=mode==0?0x1231fb:mode==1?0x1232aa:0x1232a9;
        int before=selection_count;
        pc_display_weapon_effect(draw,mode==2?2:0);
        CHECK(selection_count==before && placement==2 && tag==0xabc && g_esp==4);
        sub_00022FF0();
    }
    puts("PASS: Ghost Gun actor composite placement, UV/tint/ABI, deferred batches and guards");
    return 0;
}
