#include "text_bitmap_macos.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char pixels[512*48*4];
int main(void) {
    int used=19, lit=0;
    memset(pixels,0,sizeof pixels);
    assert(tsfp_native_overlay_bitmap("Shared PC / Mac",pixels,512,28,&used));
    assert(used>12 && used<512);
    for(int y=0;y<28;y++)for(int x=0;x<used;x++)lit+=pixels[(y*512+x)*4]>128;
    assert(lit>30); /* Real glyph coverage, not only a successful API return. */
    assert(pixels[0]==0 && pixels[1]==0 && pixels[2]==0);
    int saved=used;
    assert(!tsfp_native_overlay_bitmap("\xff",pixels,512,28,&used) && used==saved);
    assert(!tsfp_native_overlay_bitmap(NULL,pixels,512,28,&used));
    assert(!tsfp_native_overlay_bitmap("text",pixels,0,28,&used));
    assert(tsfp_native_debug_bitmap("Existing GL debug text",pixels,512,48,&used));
    assert(used>16 && used<=512);
    puts("PASS: native text glyph coverage, dimensions and invalid UTF-8 preservation");
    return 0;
}
