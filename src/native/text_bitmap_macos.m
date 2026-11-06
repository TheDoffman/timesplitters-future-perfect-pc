/* CoreText/CG objects are local to each call; no AppKit window or main thread
 * access is needed for text rasterization on a renderer worker. */
#import <CoreText/CoreText.h>
#include <math.h>
#include "text_bitmap_macos.h"
static int text_bitmap(const char *text,unsigned char *pixels,int width,int height,
                       int *used_width,double size,double background,int pad,int baseline,int styled,unsigned weight) {
    if(!text || !pixels || width<=0 || height<=0)return 0;
    CFStringRef string=CFStringCreateWithCString(NULL,text,kCFStringEncodingUTF8);
    if(!string)return 0;
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef c=CGBitmapContextCreate(pixels,width,height,8,width*4,space,
        kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if(!c){CFRelease(string);return 0;}
    CGContextSetRGBFillColor(c,background,background,background,1);
    CGContextFillRect(c,CGRectMake(0,0,width,height));
    CTFontRef font=(styled||height==32)?CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,size,NULL):CTFontCreateWithName(CFSTR("Menlo"),size,NULL);
    if(styled&&weight>=600) {
        double value=.23;CFNumberRef number=CFNumberCreate(NULL,kCFNumberDoubleType,&value);
        const void *tk[]={kCTFontWeightTrait},*tv[]={number};
        CFDictionaryRef traits=CFDictionaryCreate(NULL,tk,tv,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
        const void *ak[]={kCTFontTraitsAttribute},*av[]={traits};
        CFDictionaryRef attributes=CFDictionaryCreate(NULL,ak,av,1,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
        CTFontDescriptorRef descriptor=CTFontDescriptorCreateWithAttributes(attributes);
        CTFontRef weighted=CTFontCreateCopyWithAttributes(font,size,NULL,descriptor);if(weighted){CFRelease(font);font=weighted;}
        CFRelease(descriptor);CFRelease(attributes);CFRelease(traits);CFRelease(number);
    }
    CGColorRef white=CGColorCreateGenericRGB(1,1,1,1);
    const void *keys[]={kCTFontAttributeName,kCTForegroundColorAttributeName};
    const void *values[]={font,white};
    CFDictionaryRef attrs=CFDictionaryCreate(NULL,keys,values,2,
        &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef attributed=CFAttributedStringCreate(NULL,string,attrs);
    CTLineRef line=CTLineCreateWithAttributedString(attributed);
    double measured=CTLineGetTypographicBounds(line,NULL,NULL,NULL);
    if(measured>width-2*pad) {
        CFAttributedStringRef dots=CFAttributedStringCreate(NULL,CFSTR("…"),attrs);
        CTLineRef token=CTLineCreateWithAttributedString(dots);
        CTLineRef truncated=CTLineCreateTruncatedLine(line,width-2*pad,kCTLineTruncationEnd,token);
        if(truncated){CFRelease(line);line=truncated;}
        CFRelease(token);CFRelease(dots);
        measured=CTLineGetTypographicBounds(line,NULL,NULL,NULL);
    }
    CGContextSetTextPosition(c,pad,baseline);CTLineDraw(line,c);
    if(used_width)*used_width=(int)fmin(width,ceil(measured)+2*pad);
    CFRelease(line);CFRelease(attributed);CFRelease(attrs);CFRelease(string);
    CGColorRelease(white);CFRelease(font);CGContextRelease(c);
    return 1;
}
int tsfp_native_debug_bitmap(const char *text,unsigned char *pixels,int w,int h,int *used) {
    return text_bitmap(text,pixels,w,h,used,22,.03,8,12,0,400);
}
int tsfp_native_overlay_bitmap(const char *text,unsigned char *pixels,int w,int h,int *used) {
    return text_bitmap(text,pixels,w,h,used,h==32?24:18,0,h==32?2:6,6,0,400);
}

int tsfp_native_styled_bitmap(const char *t,unsigned char *p,int w,int h,int *used,unsigned size,unsigned weight) {
    if(size<6||size>96||w>16384||h>4096)return 0;
    return text_bitmap(t,p,w,h,used,size,0,2,4,1,weight);
}
