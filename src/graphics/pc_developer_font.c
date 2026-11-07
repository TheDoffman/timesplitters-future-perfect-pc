/* Only the verified, embedded font files are parsed. No external font loading. */
#include "pc_developer_font.h"
#include <math.h>
#include <string.h>
#ifdef _MSC_VER
#pragma warning(push,0)
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../../vendor/stb/stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "pc_developer_font_data.h"

static int font_info(stbtt_fontinfo *font,unsigned weight,unsigned face) {
    const unsigned char *bytes=face==PC_FONT_MONO?pc_font_mono:
        weight>=700?pc_font_bold:weight>=600?pc_font_semibold:pc_font_regular;
    return stbtt_InitFont(font,bytes,0);
}
/* Invalid input consumes one byte and uses the replacement glyph. */
static unsigned next_codepoint(const char **text) {
    const unsigned char *p=(const unsigned char *)*text;unsigned c=*p++,n=0;
    if(c<128){*text=(const char *)p;return c;}
    if(c>=0xc2&&c<=0xdf){n=1;c&=31;}
    else if(c>=0xe0&&c<=0xef){n=2;c&=15;}
    else if(c>=0xf0&&c<=0xf4){n=3;c&=7;}
    else {(*text)++;return 0xfffd;}
    unsigned count=n;
    while(n--){if((*p&0xc0)!=0x80){(*text)++;return 0xfffd;}c=(c<<6)|(*p++&63);}
    if((count==1&&c<128)||(count==2&&c<2048)||(count==3&&c<65536)||c>0x10ffff||(c>=0xd800&&c<=0xdfff)){(*text)++;return 0xfffd;}
    *text=(const char *)p;return c;
}
PcDeveloperFontMetrics pc_developer_font_measure_spaced(const char *text,float px,unsigned weight,unsigned face,float spacing) {
    PcDeveloperFontMetrics result={0};stbtt_fontinfo font;int ascent,descent,gap;
    if(!text||!isfinite(px)||px<1||px>96||!isfinite(spacing)||fabsf(spacing)>20||!font_info(&font,weight,face))return result;
    float scale=stbtt_ScaleForMappingEmToPixels(&font,px);
    stbtt_GetFontVMetrics(&font,&ascent,&descent,&gap);result.ascent=ascent*scale;result.descent=descent*scale;
    int previous=0;
    while(*text) {
        int glyph=stbtt_FindGlyphIndex(&font,(int)next_codepoint(&text)),advance,bearing;
        if(previous)result.advance+=stbtt_GetGlyphKernAdvance(&font,previous,glyph)*scale;
        stbtt_GetGlyphHMetrics(&font,glyph,&advance,&bearing);result.advance+=advance*scale+spacing;previous=glyph;
    }
    result.advance=fmaxf(0,result.advance);return result;
}
PcDeveloperFontMetrics pc_developer_font_measure(const char *text,float px,unsigned weight,unsigned face) {
    return pc_developer_font_measure_spaced(text,px,weight,face,0);
}
int pc_developer_font_bitmap(const char *text,unsigned char *pixels,unsigned width,unsigned height,unsigned *used,float px,unsigned weight,unsigned face,float spacing) {
    stbtt_fontinfo font;
    if(!text||!pixels||!used||!width||width>16384||!height||height>4096||!isfinite(px)||px<6||px>96||!isfinite(spacing)||fabsf(spacing)>20||!font_info(&font,weight,face))return 0;
    PcDeveloperFontMetrics metrics=pc_developer_font_measure_spaced(text,px,weight,face,spacing);
    float scale=stbtt_ScaleForMappingEmToPixels(&font,px),pen=2;
    int baseline=(int)roundf(((float)height-(metrics.ascent-metrics.descent))*.5f+metrics.ascent),previous=0;
    memset(pixels,0,(size_t)width*height*4);
    while(*text) {
        int glyph=stbtt_FindGlyphIndex(&font,(int)next_codepoint(&text)),advance,bearing,w,h,x,y;
        if(previous)pen+=stbtt_GetGlyphKernAdvance(&font,previous,glyph)*scale;
        float shift=pen-floorf(pen);
        unsigned char *bitmap=stbtt_GetGlyphBitmapSubpixel(&font,scale,scale,shift,0,glyph,&w,&h,&x,&y);
        if(bitmap) {
            for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
                int dx=(int)floorf(pen)+x+col,dy=baseline+y+row;
                if(dx<0||dy<0||(unsigned)dx>=width||(unsigned)dy>=height)continue;
                unsigned char a=bitmap[(size_t)row*w+col],*out=pixels+((size_t)dy*width+dx)*4;
                if(a>out[0])out[0]=out[1]=out[2]=out[3]=a;
            }
            stbtt_FreeBitmap(bitmap,NULL);
        }
        stbtt_GetGlyphHMetrics(&font,glyph,&advance,&bearing);pen+=advance*scale+spacing;previous=glyph;
    }
    unsigned extent=(unsigned)ceilf(metrics.advance)+4;*used=extent<width?extent:width;return 1;
}
void pc_developer_font_fit_spaced(const char *text,float px,unsigned weight,unsigned face,float spacing,float width,char *out,size_t capacity) {
    if(!out||!capacity)return;
    size_t length=strlen(text);if(length>=capacity)length=capacity-1;
    while(length&&((unsigned char)text[length]&0xc0)==0x80)length--;
    memcpy(out,text,length);out[length]=0;
    if(pc_developer_font_measure_spaced(out,px,weight,face,spacing).advance<=width+.01f)return;
    const char *dots="\xe2\x80\xa6";
    while(length) {
        do{length--;}while(length&&((unsigned char)out[length]&0xc0)==0x80);
        out[length]=0;
        if(length+3<capacity&&pc_developer_font_measure_spaced(out,px,weight,face,spacing).advance+pc_developer_font_measure_spaced(dots,px,weight,face,spacing).advance<=width+.01f) {
            memcpy(out+length,dots,4);return;
        }
    }
    out[0]=0;
}

void pc_developer_font_fit(const char *text,float px,unsigned weight,unsigned face,float width,char *out,size_t capacity) {
    pc_developer_font_fit_spaced(text,px,weight,face,0,width,out,capacity);
}
