#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc_surface_capture.h"
static unsigned char memory[0x800000];
static int read_memory(uint32_t address,void *out,unsigned bytes) {
    if(address>=sizeof memory || bytes>sizeof memory-address)return 0;
    memcpy(out,memory+address,bytes);return 1;
}
static void word(unsigned address,unsigned value){memcpy(memory+address,&value,4);}
int main(void) {
    const unsigned resource=0x10000,params=0x20000,ids=0x30000,table=0x40000,base=0x50000;
    word(resource+0x1c,params);word(resource+0x20,ids);
    word(resource+0x24,2);word(resource+0x28,0);word(resource+0x2c,1);
    word(0x4b85c8,table);word(table,base);
    for(unsigned i=0;i<4;i++) {
        word(ids+i*4,10+i);unsigned va=base+(10+i)*0x34;
        word(va+4,0x80000000+i*65536);word(va+12,0x600);word(va+16,0);
        word(params+0x10+i*4,0x3f000000+i);
    }
    word(0x7497b8,100);word(0x7de318,300);word(0x79094c,4);word(0x7de30c,7);
    word(0x4e7944,1);word(0x4e794c,0x3f800000);
    PcSurfaceCapture sample,untouched;memset(&sample,0xa5,sizeof sample);
    assert(sizeof sample==92 && pc_surface_capture_collect(read_memory,resource,&sample));
    assert(sample.resource==resource && sample.frame==100 && sample.level==7 && sample.whole==1);
    const unsigned order[3]={2,1,0};
    for(unsigned i=0;i<3;i++) {
        assert(sample.textures[i][0]==base+(10+order[i])*0x34);
        assert(sample.textures[i][1]==0x80000000+order[i]*65536);
    }
    untouched=sample;
    word(resource+0x24,4);assert(!pc_surface_capture_collect(read_memory,resource,&sample));
    assert(!memcmp(&sample,&untouched,sizeof sample));word(resource+0x24,2);
    word(ids+8,10);assert(!pc_surface_capture_collect(read_memory,resource,&sample));word(ids+8,12);
    word(ids,0xffffffff);assert(!pc_surface_capture_collect(read_memory,resource,&sample));word(ids,10);
    word(resource+0x1c,0xfffffff0);assert(!pc_surface_capture_collect(read_memory,resource,&sample));
    assert(!pc_surface_capture_collect(read_memory,0xfffffff0,&sample));
    assert(!memcmp(&sample,&untouched,sizeof sample));
    puts("PASS: captured original feedback order, copied identities and rejection without partial publication");
    return 0;
}
