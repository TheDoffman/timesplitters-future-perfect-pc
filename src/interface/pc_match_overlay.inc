#include "pc_match_overlay.h"
static volatile LONG match_overlay_migrating;
void pc_match_overlay_migration(int active)
{InterlockedExchange(&match_overlay_migrating,!!active);}

/* Use the existing shared final-frame canvas, independently of Tools/HUD
 * preferences. Reserve the status card's draw slots and paint it last. */
unsigned pc_developer_lines(unsigned width,unsigned height,PcDeveloperLine *out,unsigned capacity)
{
    if(!out || !capacity || width<100 || height<100)return 0;
    const int migrating=InterlockedCompareExchange(&match_overlay_migrating,0,0)!=0;
    if(capacity>PC_DEVELOPER_MAX_DRAW)capacity=PC_DEVELOPER_MAX_DRAW;
    if(!migrating)return developer_sidebar_lines(width,height,out,capacity);
    if(capacity<3)return 0;
    unsigned count=developer_sidebar_lines(width,height,out,capacity-3);
    PcDeveloperLine *card=out+count;memset(card,0,3*sizeof *card);
    const float scale=fminf(fmaxf(.75f,pc_controls_developer_ui_scale()),fminf(width/460.f,height/160.f));
    const float w=420.f*scale,h=112.f*scale,x=(width-w)*.5f,y=(height-h)*.5f;
    card[0]=(PcDeveloperLine){.x=x,.y=y,.width=w,.height=h,.kind=PC_DEVELOPER_BOX,
        .color=0x151b21f5u,.radius=12.f*scale};
    card[1]=(PcDeveloperLine){.x=x+16.f*scale,.y=y+19.f*scale,.width=w-32.f*scale,.height=34.f*scale,
        .color=0xedf0f3ffu,.font_px=24.f*scale,.font_weight=700,.align=1};
    card[2]=(PcDeveloperLine){.x=x+16.f*scale,.y=y+61.f*scale,.width=w-32.f*scale,.height=28.f*scale,
        .color=0xb4c4d4ffu,.font_px=14.f*scale,.font_weight=400,.align=1};
    snprintf(card[1].text,sizeof card[1].text,"Migrating host...");
    snprintf(card[2].text,sizeof card[2].text,"Please wait. Your match will resume shortly.");
    return count+3;
}
