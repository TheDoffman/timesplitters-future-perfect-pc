/* Real renderer, fake value-only UI callbacks; no game/input/kernel dependency. */
#define main developer_replay_main
#include TSFP_REPLAY_SOURCE
#undef main
#include "pc_developer.h"
#include "rhi_backend.h"
static RhiBackend traced;
static const RhiBackend *backend;
static int32_t trace_present(uint32_t interval) {
    uint32_t width=0,height=0;
    if(!backend->swapchain_readback(NULL,0,&width,&height)) {
        unsigned char *pixels=calloc((size_t)width*height,4);unsigned white=0;
        if(pixels&&!backend->swapchain_readback(pixels,width*4,&width,&height)) {
            for(unsigned y=0;y<height&&y<28;y++)for(unsigned x=0;x<width&&x<60;x++) {
                unsigned char *p=pixels+((size_t)y*width+x)*4;
                white+=p[0]>200&&p[1]>200&&p[2]>200;
            }
            fprintf(stderr,"[TEST-OVERLAY] before present white=%u\n",white);
            write_bmp("developer-before-present.bmp",pixels,(size_t)width*4,width,height);
        }
        free(pixels);
    }
    return backend->present(interval);
}
extern void d3d8_overlay_developer_register(void (*)(void),
    unsigned (*)(unsigned,unsigned,PcDeveloperLine *,unsigned),int (*)(void),int (*)(void));
static unsigned overlay_rows(unsigned width,unsigned height,PcDeveloperLine *out,unsigned count)
{
    static int announced;
    if(!announced++){
        fprintf(stderr,"[TEST-OVERLAY] output=%ux%u capacity=%u\n",width,height,count);
        backend=g_rhi;traced=*backend;traced.present=trace_present;g_rhi=&traced;
    }
    if(!count||width<32||height<32)return 0;
    memset(out,0,sizeof *out);snprintf(out->text,sizeof out->text,"UI");
    out->x=2;out->y=2;out->width=(float)width-4;out->height=24;return 1;
}
static int watermark(void) {return 0;}
static int hud(void) {return 1;}
int main(int argc,char **argv)
{
    unsigned char pixels[1024*28*4];uint32_t used=0;unsigned coverage=0;
    if(!rhi_text_bitmap("UI",pixels,1024,28,&used)){fprintf(stderr,"[TEST-OVERLAY] bitmap failed\n");return 2;}
    for(unsigned i=0;i<1024*28;i++)coverage+=pixels[i*4+2]>128;
    fprintf(stderr,"[TEST-OVERLAY] bitmap width=%u coverage=%u\n",used,coverage);
    d3d8_overlay_developer_register(NULL,overlay_rows,watermark,hud);
    return developer_replay_main(argc,argv);
}
