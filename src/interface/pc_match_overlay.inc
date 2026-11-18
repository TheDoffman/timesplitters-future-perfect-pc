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
    enum { CARD_LINES=10 };
    if(capacity<CARD_LINES)return 0;
    unsigned count=developer_sidebar_lines(width,height,out,capacity-CARD_LINES);
    PcDeveloperLine *card=out+count;memset(card,0,CARD_LINES*sizeof *card);
    const float scale=fminf(fminf(2.f,fmaxf(.75f,pc_controls_developer_ui_scale())),fminf(width/600.f,height/220.f));
    const float w=520.f*scale,h=150.f*scale,x=(width-w)*.5f,y=(height-h)*.5f;
    /* Echo the original pause confirmations: pale rounded frame, black
       translucent interior, white capitals and restrained blue light bars. */
    card[0]=(PcDeveloperLine){.x=x-24.f*scale,.y=y-12.f*scale,.width=w+48.f*scale,.height=6.f*scale,
        .kind=PC_DEVELOPER_BOX,.color=0x367da42cu,.radius=3.f*scale};
    card[1]=(PcDeveloperLine){.x=x-8.f*scale,.y=y-10.f*scale,.width=w+16.f*scale,.height=2.f*scale,
        .kind=PC_DEVELOPER_BOX,.color=0x6bbfe180u};
    card[2]=(PcDeveloperLine){.x=x,.y=y,.width=w,.height=h,.kind=PC_DEVELOPER_BOX,
        .color=0xb8bdc1e8u,.radius=14.f*scale};
    card[3]=(PcDeveloperLine){.x=x+2.f*scale,.y=y+2.f*scale,.width=w-4.f*scale,.height=h-4.f*scale,
        .kind=PC_DEVELOPER_BOX,.color=0x030507fau,.radius=12.f*scale};
    card[4]=(PcDeveloperLine){.x=x+18.f*scale,.y=y+16.f*scale,.width=w-36.f*scale,.height=36.f*scale,
        .color=0xf5f5f2ffu,.font_px=26.f*scale,.font_weight=700,.align=1,.letter_spacing=.6f*scale};
    card[5]=(PcDeveloperLine){.x=x+20.f*scale,.y=y+62.f*scale,.width=w-40.f*scale,.height=1.f*scale,
        .kind=PC_DEVELOPER_BOX,.color=0x727b8280u};
    card[6]=(PcDeveloperLine){.x=x+18.f*scale,.y=y+73.f*scale,.width=w-36.f*scale,.height=26.f*scale,
        .color=0xf5f5f2ffu,.font_px=18.f*scale,.font_weight=700,.align=1,.letter_spacing=.4f*scale};
    card[7]=(PcDeveloperLine){.x=x+18.f*scale,.y=y+107.f*scale,.width=w-36.f*scale,.height=24.f*scale,
        .color=0xb9c6cfffU,.font_px=14.f*scale,.font_weight=700,.align=1,.letter_spacing=.3f*scale};
    card[8]=card[0];card[8].y=y+h+6.f*scale;
    card[9]=card[1];card[9].y=y+h+8.f*scale;
    snprintf(card[4].text,sizeof card[4].text,"MIGRATING HOST...");
    snprintf(card[6].text,sizeof card[6].text,"PLEASE WAIT");
    snprintf(card[7].text,sizeof card[7].text,"YOUR MATCH WILL RESUME SHORTLY");
    return count+CARD_LINES;
}
