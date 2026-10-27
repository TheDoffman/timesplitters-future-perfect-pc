/* The title's 3D surround must cover Hor+ without scaling the logo/wipe. */
#define main ui_layout_suite
#include "ui_layout.c"
#undef main
static uint32_t emitted[32];
static void emit_matte(void)
{
    /* This helper runs inside an open primitive; flushing would split it. */
    CHECK(pending==1 && selection_count==0 && draw_count==0);
    memcpy(emitted,stack,sizeof emitted);g_esp+=4;
}
static void prepare(uint32_t caller)
{
    /* Captured left surround: XYZ + colour, four vertices. The right one
       has the opposite X sign. Only the first pair lies at the outer edge. */
    const float xy[4][2]={{-2.37f,1.197f},{-2.37f,-1.647f},
                         {-1.58f,-1.647f},{-1.58f,1.197f}};
    memset(stack,0,sizeof stack);stack[0]=caller;
    for(unsigned v=0;v<4;v++) {
        float x=xy[v][0]*(caller==0x2bde0a?-1.f:1.f),z=1.5f;
        memcpy(&stack[1+4*v],&x,4);memcpy(&stack[2+4*v],&xy[v][1],4);
        memcpy(&stack[3+4*v],&z,4);stack[4+4*v]=0x7f000000;
    }
    g_esp=0;pending=1;selection_count=draw_count=0;
}
static void check(int changed)
{
    uint32_t saved[32];memcpy(saved,stack,sizeof saved);
    pc_display_title_matte(emit_matte);
    CHECK(g_esp==4 && !memcmp(saved,stack,sizeof saved));
    for(unsigned i=0;i<32;i++) {
        if(changed && (i==1 || i==5)) {
            float actual,original;memcpy(&actual,&emitted[i],4);memcpy(&original,&saved[i],4);
            CHECK(fabsf(actual-original/hud_scale)<.00001f);
            /* Original 4:3 outer edge is offscreen. The widened projection
               must retain that coverage; the old unexpanded quad fails at
               21:9 and 32:9. */
            CHECK(fabsf(actual*1.44f*hud_scale/2.5f)>1.f);
        } else CHECK(emitted[i]==saved[i]);
    }
}
int main(void)
{
    const unsigned sizes[][2]={{640,480},{1280,960},{1280,1024},{1920,1080},
        {1920,1200},{2560,1440},{3440,1440},{5120,1440},{7680,4320},{1080,1920}};
    for(unsigned m=0;m<sizeof sizes/sizeof sizes[0];m++) {
        hud_scale=4.f*sizes[m][1]/(3.f*sizes[m][0]);wide=hud_scale!=1.f;
        for(unsigned side=0;side<2;side++) {
            prepare(side?0x2bde0a:0x2bddb4);check(hud_scale<1.f);
        }
    }
    wide=1;hud_scale=.55813956f;
    /* Top/bottom matte, logo, unrelated callers, gameplay, invalid view,
       partial viewport and inactive/invalid display modes stay original. */
    const uint32_t other[]={0,0x2bdd10,0x2bdd56,0x2bdca3,0x2bde8e};
    for(unsigned i=0;i<sizeof other/sizeof other[0];i++){prepare(other[i]);check(0);}
    scene=100;prepare(0x2bddb4);check(0);scene=101;
    wide=0;prepare(0x2bddb4);check(0);wide=1;
    view_address=0;prepare(0x2bddb4);check(0);view_address=0x100;
    const unsigned offsets[]={0,1,5,6};
    for(unsigned i=0;i<4;i++) {
        unsigned saved=view[offsets[i]];view[offsets[i]]=100;
        prepare(0x2bddb4);check(0);view[offsets[i]]=saved;
    }
    view[1]=640;view[6]=480;prepare(0x2bde0a);check(1);
    const float invalid[]={0.f,-1.f,1.f,INFINITY,NAN};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++){
        hud_scale=invalid[i];prepare(0x2bddb4);check(0);
    }
    puts("PASS: title surround covers wide displays; logo, wipe, open batch and caller state preserved");
    return 0;
}
