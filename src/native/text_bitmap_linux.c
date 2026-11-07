/* Linux font rasterization adapter; shared RHI owns compositing and policy. */
#include "text_bitmap_macos.h"
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <pthread.h>
#include <string.h>
static pthread_once_t font_once=PTHREAD_ONCE_INIT;
static void init_fonts(void){FcInit();}
static int bitmap(const char *text,unsigned char *pixels,int w,int h,int *used,int size,int pad,int styled,unsigned weight) {
    if(!text||!pixels||w<=0||h<=0||w>16384||h>4096)return 0;
    pthread_once(&font_once,init_fonts);
    FcPattern *pattern=FcNameParse((const FcChar8 *)((styled||h==32)?"sans-serif":"monospace"));
    if(!pattern)return 0;
    if(styled)FcPatternAddInteger(pattern,FC_WEIGHT,weight>=600?FC_WEIGHT_DEMIBOLD:FC_WEIGHT_REGULAR);
    FcConfigSubstitute(NULL,pattern,FcMatchPattern);FcDefaultSubstitute(pattern);
    FcResult result;FcPattern *match=FcFontMatch(NULL,pattern,&result);FcPatternDestroy(pattern);
    FcChar8 *path=NULL;FT_Library library=NULL;FT_Face face=NULL;
    int ok=match&&FcPatternGetString(match,FC_FILE,0,&path)==FcResultMatch;
    if(ok)ok=FT_Init_FreeType(&library)==0;
    if(ok)ok=FT_New_Face(library,(const char *)path,0,&face)==0;
    if(match)FcPatternDestroy(match);
    if(ok)ok=FT_Set_Pixel_Sizes(face,0,(FT_UInt)size)==0;
    if(ok){
        memset(pixels,0,(size_t)w*h*4);for(size_t i=3;i<(size_t)w*h*4;i+=4)pixels[i]=255;
        int x=pad,baseline=size+2;
        const unsigned char *p=(const unsigned char *)text;
        while(*p&&x<w-pad){
            unsigned c=*p++;int continuation=0;
            if(c>=0xc0&&c<0xe0){c&=31;continuation=1;}
            else if(c>=0xe0&&c<0xf0){c&=15;continuation=2;}
            else if(c>=0xf0&&c<0xf8){c&=7;continuation=3;}
            else if(c>=0x80)c='?';
            while(continuation--){if((*p&0xc0)!=0x80){c='?';break;}c=(c<<6)|(*p++&63);}
            if(FT_Load_Char(face,c,FT_LOAD_RENDER))continue;
            FT_GlyphSlot glyph=face->glyph;FT_Bitmap *b=&glyph->bitmap;
            for(unsigned row=0;row<b->rows;row++)for(unsigned col=0;col<b->width;col++){
                int px=x+glyph->bitmap_left+(int)col,py=baseline-glyph->bitmap_top+(int)row;
                if(px<0||px>=w||py<0||py>=h)continue;
                unsigned char coverage=b->buffer[(b->pitch>=0?row:b->rows-1-row)*(unsigned)(b->pitch>=0?b->pitch:-b->pitch)+col];
                unsigned char *at=pixels+((size_t)py*w+px)*4;at[0]=at[1]=at[2]=coverage;
            }
            x+=(int)(glyph->advance.x>>6);
        }
        if(used)*used=x+pad<w?x+pad:w;
    }
    if(face)FT_Done_Face(face);if(library)FT_Done_FreeType(library);
    return ok;
}
int tsfp_native_debug_bitmap(const char *t,unsigned char *p,int w,int h,int *used){return bitmap(t,p,w,h,used,22,8,0,400);}
int tsfp_native_overlay_bitmap(const char *t,unsigned char *p,int w,int h,int *used){return bitmap(t,p,w,h,used,h==32?24:18,h==32?2:6,0,400);}

int tsfp_native_styled_bitmap(const char *t,unsigned char *p,int w,int h,int *used,unsigned size,unsigned weight) {
    if(size<6||size>96)return 0;return bitmap(t,p,w,h,used,(int)size,2,1,weight);
}
