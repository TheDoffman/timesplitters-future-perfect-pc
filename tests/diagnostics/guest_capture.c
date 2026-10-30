#include "pc_guest_capture.h"
#include <assert.h>
static unsigned calls;
static int read_fixture(void *ctx,uint32_t address,void *out,size_t bytes) {
    (void)ctx;calls++;assert(pc_guest_capture_span(address,bytes));
    if(address==0x80001000u)return 0;
    memset(out,0,bytes);return 1;
}
int main(void) {
    assert(!pc_guest_capture_span(0xffffffffu,4096));
    assert(!pc_guest_capture_span(0x83fffff0u,32));
    assert(!pc_guest_capture_span(0,4096));
    assert(pc_guest_capture_span(0x83fff000u,4096));
    uint32_t r[8]={0,0xffffffffu,0x80001000u,0x80002000u,0,0,0x01000000u,0};
    for(int extended=0;extended<2;extended++) {
        FILE *f=tmpfile();assert(f);calls=0;
        assert(pc_guest_capture_write_mode(f,read_fixture,0,r,extended));
        long size=ftell(f);assert(size>65536 && size<PC_GUEST_CAPTURE_LIMIT);assert(calls==4);
        rewind(f);char signature[8];assert(fread(signature,1,8,f)==8&&!memcmp(signature,"TSFPGST1",8));
        unsigned records=0,missing=0;
        while(ftell(f)<size) {
            unsigned char h[16];assert(fread(h,1,16,f)==16);
            uint32_t requested=h[8]|(uint32_t)h[9]<<8|(uint32_t)h[10]<<16|(uint32_t)h[11]<<24;
            uint32_t actual=h[12]|(uint32_t)h[13]<<8|(uint32_t)h[14]<<16|(uint32_t)h[15]<<24;
            assert(actual==0||actual==requested);if(!actual)missing++;
            assert(fseek(f,actual,SEEK_CUR)==0);records++;
        }
        assert(records==9&&missing==6);fclose(f);
    }
    return 0;
}
