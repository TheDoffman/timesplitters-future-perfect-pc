#ifndef TSFP_SCREEN_COPY_RECT_H
#define TSFP_SCREEN_COPY_RECT_H
#include <stdint.h>
/* Clip a paired guest-pixel copy against both surfaces. Advancing one edge
 * advances the corresponding edge on the other surface, preserving alignment.
 * Wide intermediates avoid overflow for malformed 32-bit guest rectangles. */
typedef struct {
    int64_t left, top, x, y, width, height;
} TsfpScreenCopyRect;
static inline int tsfp_screen_copy_rect(int64_t left,int64_t top,int64_t right,int64_t bottom,
                                       int64_t x,int64_t y,int64_t source_width,int64_t source_height,
                                       int64_t dest_width,int64_t dest_height,TsfpScreenCopyRect *out)
{
    if(x<0){left-=x;x=0;}if(y<0){top-=y;y=0;}
    if(left<0){x-=left;left=0;}if(top<0){y-=top;top=0;}
    if(right>source_width)right=source_width;
    if(bottom>source_height)bottom=source_height;
    int64_t width=right-left,height=bottom-top;
    if(x>=dest_width || y>=dest_height)return 0;
    if(width>dest_width-x)width=dest_width-x;
    if(height>dest_height-y)height=dest_height-y;
    if(width<=0 || height<=0)return 0;
    out->left=left;out->top=top;out->x=x;out->y=y;out->width=width;out->height=height;
    return 1;
}
#endif
