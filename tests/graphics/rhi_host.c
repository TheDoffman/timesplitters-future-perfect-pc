#include "rhi_backend.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
/* Host contract does not create a graphics device. */
const RhiBackend rhi_d3d11_backend = {0};
#endif
static int pumps, size_calls, bitmap_calls, accept = 1;
static uint32_t bitmap_width = 12;
static int bitmap(void *context,const char *text,unsigned char *pixels,uint32_t w,uint32_t h,uint32_t *used)
{
    assert(context==&pumps && text && pixels && w==32 && h==28);
    ++bitmap_calls; *used=bitmap_width; pixels[0]=255; return accept;
}
static uint32_t requested_width = 1920, requested_height = 1080;
static void pump(void *context) { assert(context == &pumps); ++pumps; }
static int size(void *context, void *window, uint32_t *width, uint32_t *height)
{
    assert(context == &pumps && window == &size_calls);
    ++size_calls;
    *width = requested_width; *height = requested_height;
    return accept;
}
static int32_t present_result;
static int present_calls;
static int32_t present(uint32_t interval) {
    assert(interval==0);present_calls++;return present_result;
}
int main(void)
{
    uint32_t width = 3, height = 7;
    RhiHost host = {.context=&pumps,.pump_events=pump,.drawable_size=size,.text_bitmap=bitmap};
    rhi_set_host(&host);
    host.pump_events = NULL; /* registration owns a copy */
    rhi_pump_events(); assert(pumps == 1);
    assert(rhi_drawable_size(&size_calls, &width, &height));
    assert(width == 1920 && height == 1080 && size_calls == 1);
    accept = 0;
    assert(!rhi_drawable_size(&size_calls, &width, &height));
    assert(width == 1920 && height == 1080);
    accept = 1; requested_height = 0;
    assert(!rhi_drawable_size(&size_calls, &width, &height));
    assert(width == 1920 && height == 1080);
    assert(!rhi_drawable_size(&size_calls, NULL, &height));
    assert(size_calls == 3);
    unsigned char pixels[32*28*4]={0};uint32_t used=9;
    assert(rhi_text_bitmap("sample",pixels,32,28,&used) && used==12 && pixels[0]==255);
    accept=0;assert(!rhi_text_bitmap("sample",pixels,32,28,&used) && used==12);
    accept=1;bitmap_width=33;assert(!rhi_text_bitmap("sample",pixels,32,28,&used) && used==12);
    assert(!rhi_text_bitmap(NULL,pixels,32,28,&used));
    assert(!rhi_text_bitmap("sample",pixels,0,28,&used));
    assert(bitmap_calls==3);
    rhi_set_host(NULL);
    assert(!rhi_drawable_size(NULL, &width, &height));
    assert(width == 1920 && height == 1080);
    RhiBackend mock={0};mock.name="mock";mock.present=present;g_rhi=&mock;
    present_result=0;assert(rhi_present_kind(0,RHI_PRESENT_GUEST)==0);
    present_result=1;assert(rhi_present_kind(0,RHI_PRESENT_INTERPOLATED)==1);
    present_result=-1;assert(rhi_present(0)==-1);
    assert(present_calls==3);g_rhi=NULL;assert(rhi_present(0)==-1);
    puts("PASS: host callbacks, registration copy, drawable failure preservation");
    return 0;
}
