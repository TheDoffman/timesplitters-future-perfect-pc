#include "pc_developer_canvas.h"
#include <assert.h>
#include <stdio.h>
static unsigned calls;
static int font(const char *text,unsigned char *pixels,unsigned w,unsigned h,unsigned *used,float font_px,unsigned weight,unsigned face,float spacing) {
    assert(!strcmp(text,"test")&&font_px>=6&&weight>=100&&face<=1&&spacing==0);calls++;memset(pixels,255,(size_t)w*h*4);*used=8;return 1;
}
static unsigned char *at(PcDeveloperCanvas *c,unsigned x,unsigned y) {return c->pixels+((size_t)y*c->width+x)*4;}
int main(void) {
    PcDeveloperCanvas c={0};PcDeveloperLine lines[3]={0};
    lines[0].kind=PC_DEVELOPER_BOX;lines[0].x=10;lines[0].y=20;lines[0].width=40;lines[0].height=40;lines[0].radius=8;lines[0].color=0x20262cffu;
    assert(pc_developer_canvas_update(&c,lines,1,font)==1&&c.x==10&&c.y==20&&c.width==40&&c.height==40);
    assert(at(&c,0,0)[3]==0&&at(&c,20,20)[0]==44&&at(&c,20,20)[2]==32);
    assert(pc_developer_canvas_update(&c,lines,1,font)==0&&!c.changed);
    lines[1].kind=PC_DEVELOPER_BOX;lines[1].x=20;lines[1].y=30;lines[1].width=20;lines[1].height=20;lines[1].color=0xff000080u;
    lines[1].clip_top=35;lines[1].clip_bottom=45;
    assert(pc_developer_canvas_update(&c,lines,2,font)==1);
    assert(at(&c,15,12)[2]==32&&at(&c,15,17)[2]>140&&at(&c,15,17)[3]==255&&at(&c,15,27)[2]==32);
    lines[2].kind=PC_DEVELOPER_LABEL;lines[2].x=12;lines[2].y=21;lines[2].width=8;lines[2].height=32;lines[2].color=0x00ff00ffu;snprintf(lines[2].text,160,"test");
    assert(pc_developer_canvas_update(&c,lines,3,font)==1&&calls==1);
    assert(at(&c,4,4)[1]==255&&at(&c,4,4)[0]==0&&at(&c,4,4)[2]==0);
    lines[2].x++;assert(pc_developer_canvas_update(&c,lines,3,font)==1&&calls==1);
    lines[2].font_px=13;lines[2].font_weight=600;
    assert(pc_developer_canvas_update(&c,lines,3,font)==1&&calls==2); /* size and weight own cache entries */
    lines[2].kind=PC_DEVELOPER_ICON;lines[2].icon=PC_DEV_CLOSE;lines[2].width=24;lines[2].height=24;lines[2].x=18;lines[2].y=28;
    assert(pc_developer_canvas_update(&c,lines,3,font)==1&&calls==2);assert(at(&c,20,20)[1]>240);
    lines[2].width=NAN;assert(pc_developer_canvas_update(&c,lines,3,font)==-1);
    lines[2].width=8;memset(lines[2].text,'x',sizeof lines[2].text);assert(pc_developer_canvas_update(&c,lines,3,font)==-1);
    pc_developer_canvas_release(&c);assert(!c.pixels&&!c.width);
    puts("Developer canvas rounded geometry, clipping, alpha, BGRA, glyph reuse and validation passed");return 0;
}
