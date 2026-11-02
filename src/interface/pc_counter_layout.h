#ifndef PC_COUNTER_LAYOUT_H
#define PC_COUNTER_LAYOUT_H
/* Bitmap heights differ between host adapters; target 24px at 1080p. */
static inline float pc_counter_scale(unsigned width,unsigned height,unsigned text_width,unsigned bitmap_height)
{
    if(!width || !height || !text_width || !bitmap_height)return 1.f;
    float scale=(float)height/1080.f*24.f/(float)bitmap_height;
    if(scale<1.f)scale=1.f; /* Keep small windows readable. */
    float fit=(float)width/((float)text_width+16.f);
    return scale<fit?scale:fit;
}
#endif
