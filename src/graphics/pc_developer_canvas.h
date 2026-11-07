#ifndef TSFP_PC_DEVELOPER_CANVAS_H
#define TSFP_PC_DEVELOPER_CANVAS_H
/* Shared sidebar rasterization. Hosts supply font coverage and upload BGRA;
 * geometry, colors, clipping and cache policy are identical on every renderer. */
#include "pc_developer.h"
#include "pc_developer_font.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
enum { PC_DEV_FONT_W=1024, PC_DEV_FONT_H=104, PC_DEV_GLYPHS=96 };
typedef int (*PcDeveloperFont)(const char *,unsigned char *,unsigned,unsigned,unsigned *,float,unsigned,unsigned,float);
typedef struct PcDeveloperGlyph {
    char text[160];unsigned width,height,weight,face;float font_px,spacing;uint64_t stamp;unsigned char *pixels;
} PcDeveloperGlyph;
typedef struct PcDeveloperCanvas {
    unsigned char *pixels,*scratch;
    unsigned width,height;int x,y,changed;
    uint64_t digest,stamp;
    PcDeveloperGlyph glyphs[PC_DEV_GLYPHS];
} PcDeveloperCanvas;
static inline void pc_developer_canvas_release(PcDeveloperCanvas *c) {
    free(c->pixels);free(c->scratch);
    for(unsigned i=0;i<PC_DEV_GLYPHS;i++)free(c->glyphs[i].pixels);
    memset(c,0,sizeof *c);
}
static inline void pc_developer_canvas_pixel(PcDeveloperCanvas *c,int x,int y,uint32_t color,float coverage) {
    if(x<0||y<0||(unsigned)x>=c->width||(unsigned)y>=c->height)return;
    unsigned a=(unsigned)((color&255u)*fminf(1.f,fmaxf(0.f,coverage))+.5f);
    if(!a)return;
    unsigned char *p=c->pixels+((size_t)y*c->width+x)*4;
    unsigned old=p[3],out=a+(old*(255-a)+127)/255;
    unsigned rgb[3]={(color>>8)&255u,(color>>16)&255u,color>>24};
    for(unsigned k=0;k<3;k++)p[k]=(unsigned char)((rgb[k]*a+(p[k]*old*(255-a)+127)/255+out/2)/out);
    p[3]=(unsigned char)out;
}
static inline PcDeveloperGlyph *pc_developer_canvas_glyph(PcDeveloperCanvas *c,const char *text,float font_px,unsigned weight,unsigned face,float spacing,PcDeveloperFont font) {
    unsigned slot=0;uint64_t oldest=UINT64_MAX;
    for(unsigned i=0;i<PC_DEV_GLYPHS;i++) {
        if(c->glyphs[i].pixels&&c->glyphs[i].font_px==font_px&&c->glyphs[i].weight==weight&&c->glyphs[i].face==face&&c->glyphs[i].spacing==spacing&&!strcmp(c->glyphs[i].text,text)) {c->glyphs[i].stamp=++c->stamp;return &c->glyphs[i];}
        if(c->glyphs[i].stamp<oldest){oldest=c->glyphs[i].stamp;slot=i;}
    }
    if(!c->scratch)c->scratch=(unsigned char *)malloc(PC_DEV_FONT_W*PC_DEV_FONT_H*4);
    unsigned used=0,height=(unsigned)ceilf(font_px)+8;
    if(!c->scratch||!font(text,c->scratch,PC_DEV_FONT_W,height,&used,font_px,weight,face,spacing)||!used||used>PC_DEV_FONT_W)return NULL;
    unsigned char *pixels=(unsigned char *)malloc((size_t)used*height);
    if(!pixels)return NULL;
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<used;x++)
        pixels[(size_t)y*used+x]=c->scratch[((size_t)y*PC_DEV_FONT_W+x)*4+2];
    PcDeveloperGlyph *g=&c->glyphs[slot];free(g->pixels);g->pixels=pixels;g->width=used;g->height=height;g->font_px=font_px;g->weight=weight;g->face=face;g->spacing=spacing;g->stamp=++c->stamp;
    size_t length=strlen(text);if(length>=sizeof g->text)length=sizeof g->text-1;
    memcpy(g->text,text,length);g->text[length]=0;return g;
}
static inline float pc_developer_segment(float x,float y,float ax,float ay,float bx,float by) {
    float dx=bx-ax,dy=by-ay,n=dx*dx+dy*dy;
    float t=n>0?fminf(1,fmaxf(0,((x-ax)*dx+(y-ay)*dy)/n)):0;
    return hypotf(x-ax-t*dx,y-ay-t*dy);
}
static inline float pc_developer_icon_coverage(unsigned icon,float x,float y,float size) {
    float d=100;
#define SEG(a,b,c,e) d=fminf(d,pc_developer_segment(x,y,a,b,c,e))
    switch(icon) {
    case PC_DEV_CLOSE:SEG(6,6,18,18);SEG(18,6,6,18);break;
    case PC_DEV_DOCK:SEG(4,8,20,8);SEG(4,8,8,4);SEG(4,8,8,12);SEG(20,16,4,16);SEG(20,16,16,12);SEG(20,16,16,20);break;
    case PC_DEV_DOWN:SEG(6,9,12,15);SEG(12,15,18,9);break;
    case PC_DEV_RIGHT:SEG(9,6,15,12);SEG(15,12,9,18);break;
    case PC_DEV_SEARCH:d=fabsf(hypotf(x-10,y-10)-6);SEG(15,15,21,21);break;
    case PC_DEV_TOOLS:SEG(7,19,16,10);SEG(5,17,14,8);SEG(5,17,7,19);SEG(14,8,14,4);SEG(14,4,18,2);SEG(18,2,17,6);SEG(17,6,20,8);SEG(20,8,23,6);SEG(23,6,22,11);SEG(22,11,18,13);SEG(18,13,16,10);break;
    case PC_DEV_RESET:
        /* Open the upper-left quadrant so the counterclockwise arrow reads
         * clearly at the small reset-button size. Cap the arc at its ends. */
        d=x<12&&y<12?fminf(hypotf(x-12,y-5),hypotf(x-5,y-12)):fabsf(hypotf(x-12,y-12)-7);
        SEG(12,5,7,5);SEG(10,2,7,5);SEG(7,5,10,8);break;
    case PC_DEV_WARNING:SEG(12,3,2,21);SEG(2,21,22,21);SEG(22,21,12,3);SEG(12,9,12,14);SEG(12,18,12,18);break;
    default:break;
    }
#undef SEG
    return fminf(1,fmaxf(0,(1.0f-d)*size/24.f+.5f));
}
/* Return -1 on invalid geometry/allocation,0 when cached,1 when uploaded pixels
 * changed. The draw list is value-only and zero-initialized by its producer. */
static inline int pc_developer_canvas_update(PcDeveloperCanvas *c,const PcDeveloperLine *lines,unsigned count,PcDeveloperFont font) {
    c->changed=0;if(!count)return 0;
    if(count>PC_DEVELOPER_MAX_DRAW||!lines||!font)return -1;
    float left=1e9f,top=1e9f,right=0,bottom=0;
    uint64_t digest=UINT64_C(1469598103934665603);
    for(unsigned i=0;i<count;i++) {
        const PcDeveloperLine *l=&lines[i];
        if(!isfinite(l->x)||!isfinite(l->y)||!isfinite(l->width)||!isfinite(l->height)||
           l->x<0||l->y< -32768||l->width<=0||l->height<=0||l->x+l->width>16384||l->y+l->height>16384||
           !isfinite(l->radius)||l->radius<0||!isfinite(l->clip_top)||!isfinite(l->clip_bottom)||
           !memchr(l->text,0,sizeof l->text)||!isfinite(l->font_px)||!isfinite(l->letter_spacing)||fabsf(l->letter_spacing)>20||l->font_px>96||l->align>2)return -1;
        float visible_top=l->clip_bottom>l->clip_top?fmaxf(l->y,l->clip_top):l->y;
        float visible_bottom=l->clip_bottom>l->clip_top?fminf(l->y+l->height,l->clip_bottom):l->y+l->height;
        if(visible_bottom>visible_top){left=fminf(left,l->x);top=fminf(top,visible_top);right=fmaxf(right,l->x+l->width);bottom=fmaxf(bottom,visible_bottom);}
        const unsigned char *p=(const unsigned char *)l;
        for(size_t j=0;j<sizeof *l;j++){digest^=p[j];digest*=UINT64_C(1099511628211);}
    }
    int x=(int)floorf(left),y=(int)floorf(top);
    unsigned w=(unsigned)ceilf(right)-x,h=(unsigned)ceilf(bottom)-y;
    if(!w||!h||(size_t)w*h>32u*1024u*1024u)return -1;
    if(c->pixels&&c->digest==digest&&c->width==w&&c->height==h&&c->x==x&&c->y==y)return 0;
    if(c->width!=w||c->height!=h||!c->pixels) {
        unsigned char *pixels=(unsigned char *)realloc(c->pixels,(size_t)w*h*4);
        if(!pixels)return -1;c->pixels=pixels;c->width=w;c->height=h;
    }
    c->x=x;c->y=y;memset(c->pixels,0,(size_t)w*h*4);
    for(unsigned i=0;i<count;i++) {
        const PcDeveloperLine *l=&lines[i];
        int x0=(int)floorf(l->x)-x,y0=(int)floorf(l->y)-y;
        int x1=(int)ceilf(l->x+l->width)-x,y1=(int)ceilf(l->y+l->height)-y;
        int ct=l->clip_bottom>l->clip_top?(int)ceilf(l->clip_top)-y:0;
        int cb=l->clip_bottom>l->clip_top?(int)floorf(l->clip_bottom)-y:(int)h;
        int begin=y0>ct?y0:ct,end=y1<cb?y1:cb;
        if(l->kind==PC_DEVELOPER_BOX) {
            float radius=fminf(l->radius,fminf(l->width,l->height)*.5f);
            float cx=l->x-x+l->width*.5f,cy=l->y-y+l->height*.5f;
            for(int py=begin;py<end;py++)for(int px=x0;px<x1;px++) {
                float dx=fmaxf(fabsf(px+.5f-cx)-(l->width*.5f-radius),0);
                float dy=fmaxf(fabsf(py+.5f-cy)-(l->height*.5f-radius),0);
                float coverage=radius>0?fminf(1.f,fmaxf(0.f,radius+.5f-sqrtf(dx*dx+dy*dy))):1.f;
                pc_developer_canvas_pixel(c,px,py,l->color,coverage);
            }
        } else if(l->kind==PC_DEVELOPER_ICON) {
            for(int py=begin;py<end;py++)for(int px=x0;px<x1;px++) {
                float ix=(px+.5f-(l->x-x))*24/l->width,iy=(py+.5f-(l->y-y))*24/l->height;
                pc_developer_canvas_pixel(c,px,py,l->color,pc_developer_icon_coverage(l->icon,ix,iy,l->width));
            }
        } else if(l->text[0]) {
            PcDeveloperGlyph *g=pc_developer_canvas_glyph(c,l->text,l->font_px?l->font_px:24,l->font_weight?l->font_weight:400,l->font_face,l->letter_spacing,font);if(!g)continue;
            int tx=(int)roundf(l->x-x+(l->align==1?(l->width-(g->width-4))*.5f:l->align==2?l->width-(g->width-4):0));
            tx-=2;
            int ty=(int)roundf(l->y-y+(l->height-g->height)*.5f);
            uint32_t color=l->color?l->color:0xffffffffu;
            for(int py=begin;py<end;py++)for(int px=x0;px<x1;px++) {
                int gx=px-tx,gy=py-ty;
                if(gx<0||gy<0||(unsigned)gx>=g->width||(unsigned)gy>=g->height)continue;
                pc_developer_canvas_pixel(c,px,py,color,g->pixels[(size_t)gy*g->width+gx]/255.f);
            }
        }
    }
    c->digest=digest;c->changed=1;return 1;
}
#endif
