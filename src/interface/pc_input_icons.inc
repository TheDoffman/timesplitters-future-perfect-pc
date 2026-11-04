/* Shared PNG-span renderer. Image data and attribution are generated separately. */
static int input_icon_id(int key)
{
    return key>=0 && key<256?input_icon_lookup[key]:-1;
}
static void input_icon_image(PcInputIconImage image,float x,float y,float w,float h,uint32_t tint)
{
    extern void sub_0001A2E0(void);
    for(unsigned i=0;i<image.count;i++) {
        const PcInputIconSpan *s=&input_icon_spans[image.first+i];
        unsigned a=(unsigned)s->a*(tint&255u)/255u;
        uint32_t colour=((uint32_t)(s->r*((tint>>24)&255u)/255u)<<24)|
                        ((uint32_t)(s->g*((tint>>16)&255u)/255u)<<16)|
                        ((uint32_t)(s->b*((tint>>8)&255u)/255u)<<8)|a;
        guest_call(sub_0001A2E0,5,bits(x+w*s->x/32.f),bits(y+h*s->y/32.f),
                   bits(x+w*(s->x+s->w)/32.f),bits(y+h*(s->y+s->h)/32.f),colour);
    }
}
static void input_icon_paint(const PcKeycapCap *cap)
{
    /* The supplied PNGs include transparent margins; compensate so their
     * visible key faces remain legible beside the game's original lettering. */
    float h=(cap->b-cap->y)*1.5f;
    float x=(cap->x+cap->r-h)*.5f,y=(cap->y+cap->b-h)*.5f;
    input_icon_image(input_icons[cap->icon*2+!!cap->pressed],x,y,h,h,cap->colour);
}
