/* Reuse the guest-memory/deferred-batch harness, exercising production hooks. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
static unsigned effect_kind;
static float seen[8];
static void put_float(uint32_t a,float f){memcpy(&WORD_AT(a),&f,4);}
static float get_float(uint32_t a){float f;memcpy(&f,&WORD_AT(a),4);return f;}
static void effect_draw(void)
{
    unsigned points=effect_kind==1?4:2;
    for(unsigned i=0;i<points;i++)for(unsigned axis=0;axis<2;axis++) {
        uint32_t a=effect_kind==1?WORD_AT(g_esp+16+i*4)+axis*4:g_esp+4+i*8+axis*4;
        seen[i*2+axis]=effect_kind==0?(float)(int32_t)WORD_AT(a):get_float(a);
    }
    pending=1;g_esp+=4;
}
static void scope_draw(void)
{
    CHECK(placement==2 && tag==0x137f00);pending=1;
    stack[0]=0x138471;
    pc_display_weapon_effect(draw,0);
    CHECK(placement==2 && tag==0x137f00);
}
int main(void)
{
    const float ratios[]={1.f,16.f/15.f,.75f,5.f/6.f,24.f/43.f,.375f};
    const uint32_t calls[]={0x13871b,0x13874a,0x138a6c,0x138c5c,0x138d73,0x138f1d};
    const uint32_t gradients[]={0x138227,0x13825a,0x138285,0x1382b1,
        0x1382dd,0x138309,0x138339,0x13836c};
    /* Both halves and rotating sub-pieces use the SAME off-centre target,
       rather than shrinking independently about their individual centres. */
    for(unsigned r=0;r<6;r++)for(unsigned c=0;c<6;c++) {
        hud_scale=ratios[r];wide=hud_scale!=1;g_esp=0;g_esi=0x4000;
        put_float(g_esi,550);put_float(g_esi+4,110);
        effect_kind=c<2?0:1;stack[0]=calls[c];
        float xy[8]={520,90,550,130,560,125,535,95};
        if(c==1){xy[0]=550;xy[2]=580;}
        unsigned points=effect_kind==1?4:2;
        for(unsigned i=0;i<points;i++) {
            uint32_t a=effect_kind==1?0x4010+i*12:4+i*8;
            if(effect_kind==1){stack[4+i]=a;put_float(a+8,.001f);}
            for(unsigned axis=0;axis<2;axis++) {
                if(effect_kind==0)WORD_AT(a+axis*4)=(uint32_t)(int32_t)xy[i*2+axis];
                else put_float(a+axis*4,xy[i*2+axis]);
            }
        }
        uint32_t saved_stack[32],saved_data[32];
        memcpy(saved_stack,stack,sizeof stack);memcpy(saved_data,effect_data,sizeof effect_data);
        ui_place(4,77);pending=1;int n=draw_count;
        pc_display_weapon_effect(effect_draw,effect_kind);sub_00022FF0();
        CHECK(drawn[n]==4 && (!wide || drawn[n+1]==1));
        CHECK(placement==4 && tag==77 && g_esp==4 && g_esi==0x4000);
        CHECK(!memcmp(stack,saved_stack,sizeof stack) && !memcmp(effect_data,saved_data,sizeof effect_data));
        for(unsigned i=0;i<points*2;i++) {
            float centre=(i&1)?110:550,scale=(i&1)?pc_ui_scale_y(hud_scale):pc_ui_scale_x(hud_scale);
            float expected=centre+(xy[i]-centre)*scale;
            CHECK(fabsf(seen[i]-expected)<(effect_kind==0?.501f:.001f));
        }
    }
    for(unsigned r=0;r<6;r++)for(unsigned c=0;c<8;c++) {
        hud_scale=ratios[r];wide=hud_scale!=1;g_esp=0;effect_kind=2;
        stack[0]=gradients[c];
        /* Alternate outer corners; inclusive guest edges become full extents. */
        float xy[4]={c&1?100:0,c&2?80:0,c&1?639:540,c&2?479:400};
        for(unsigned i=0;i<4;i++)put_float(4+i*4,xy[i]);
        stack[5]=0x10203040;stack[6]=0x50607080;stack[7]=0x90a0b0c0;stack[8]=0xd0e0f000;
        uint32_t saved[32];memcpy(saved,stack,sizeof stack);
        ui_place(2,0x137f00);pending=1;int n=draw_count;
        pc_display_weapon_effect(effect_draw,2);sub_00022FF0();
        CHECK(drawn[n]==2 && (!wide || drawn[n+1]==1));
        CHECK(!memcmp(saved,stack,sizeof stack) && tag==0x137f00 && placement==2);
        for(unsigned i=0;i<4;i++) {
            float extent=i&1?480.f:640.f,scale=i&1?pc_ui_scale_y(hud_scale):pc_ui_scale_x(hud_scale);
            float expected=!wide?xy[i]:xy[i]==0?0:xy[i]==extent-1?extent:extent*.5f+(xy[i]-extent*.5f)*scale;
            CHECK(fabsf(seen[i]-expected)<.001f);
        }
    }
    wide=1;hud_scale=.5f;g_esp=0;ui_place(4,77);int n=draw_count;
    pc_display_effect_scope(scope_draw);
    CHECK(drawn[n]==2 && drawn[n+1]==1 && placement==4 && tag==77);
    /* Unrelated calls, split viewports, invalid target and partial pointer
       validation must leave all guest memory and placement untouched. */
    for(unsigned mode=0;mode<7;mode++) {
        g_esp=0;g_esi=mode==3?0:0x4000;players=mode==1?2:1;view[0]=mode==2?10:0;
        stack[0]=mode==0?0x123456:0x138a6c;
        for(unsigned i=0;i<4;i++)stack[4+i]=0x4010+i*12;
        if(mode==4)stack[7]=0x9000;
        put_float(0x4000,mode==5?NAN:550);put_float(0x4004,110);
        if(mode==6)stack[0]=0x138227; /* gradient outside the scope group */
        uint32_t saved[32];memcpy(saved,effect_data,sizeof saved);int before=selection_count;
        pc_display_weapon_effect(draw,mode==6?2:1);sub_00022FF0();
        CHECK(before==selection_count && !memcmp(saved,effect_data,sizeof saved) && g_esp==4);
    }
    puts("PASS: lock-on target anchoring, scope gradients/composite, guards and restoration");
    return 0;
}
