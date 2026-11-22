#define _POSIX_C_SOURCE 200809L
#include "graphics/pc_present_capture.h"
#include <assert.h>
static int reads,oversized;
static int readback(void *data,uint32_t pitch,uint32_t *w,uint32_t *h)
{
    if(!data){*w=oversized?UINT32_MAX:2;*h=2;return 0;}
    assert(pitch==8);reads++;
    const unsigned char rgba[16]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
    memcpy(data,rgba,sizeof rgba);return 0;
}
int main(void)
{
#ifdef _WIN32
    _putenv_s("TSFP_PRESENT_CAPTURE","present-");_putenv_s("TSFP_PRESENT_CAPTURE_EVERY","1");
#else
    setenv("TSFP_PRESENT_CAPTURE","present-",1);setenv("TSFP_PRESENT_CAPTURE_EVERY","1",1);
#endif
    pc_present_capture(readback);pc_present_capture(readback);pc_present_capture(readback);
    assert(reads==3);
    FILE *file=fopen("present-000.bmp","rb");assert(file);
    unsigned char bmp[70]={0};assert(fread(bmp,1,sizeof bmp,file)==sizeof bmp);fclose(file);
    assert(bmp[0]=='B' && bmp[1]=='M' && bmp[18]==2 && bmp[22]==2);
    assert(bmp[54]==255 && bmp[55]==0 && bmp[56]==0); /* Bottom-left blue, BMP BGR. */
    assert(bmp[62]==0 && bmp[63]==0 && bmp[64]==255); /* Top-left red. */
    file=fopen("present-001.bmp","rb");assert(file);fclose(file);
    oversized=1;pc_present_capture(readback);assert(reads==3);
    puts("PASS completed-frame capture: bounded readback, BMP orientation and rotation");return 0;
}
