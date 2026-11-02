/* Exact maximum record capacities, query overflow and null service boundaries. */
#include "host_text_codec.h"
#include <assert.h>
#include <string.h>
static unsigned char ram[180000],before[180000];
static tsfp_allocations allocations;static tsfp_allocation_record records[8];
static uint32_t get(uint32_t at,unsigned n){uint32_t v=0;for(unsigned i=0;i<n;++i)v|=(uint32_t)ram[at+i]<<(i*8);return v;}
static void put(uint32_t at,unsigned n,uint32_t v){for(unsigned i=0;i<n;++i)ram[at+i]=(unsigned char)(v>>(i*8));}
static void *resolve(void *host,uint32_t at,uint32_t n,int write){(void)host;(void)write;return at && at<sizeof(ram) && n<=sizeof(ram)-at?ram+at:NULL;}
static int huge(void *host,unsigned d,const unsigned char *s,uint32_t n,unsigned char *out,uint32_t cap,uint32_t *used){(void)host;(void)d;(void)s;(void)n;(void)out;(void)cap;*used=UINT32_MAX;return 1;}
int main(void) {
    tsfp_guest_vm vm;tsfp_allocation_config cfg;tsfp_host_text_codec host;tsfp_text_codec codec,bad;
    memset(&vm,0,sizeof(vm));memset(&cfg,0,sizeof(cfg));cfg.ram=ram;cfg.ram_size=sizeof(ram);cfg.heap_begin=160000;cfg.reserve_begin=sizeof(ram);
    assert(tsfp_allocations_init(&allocations,&cfg,records,8,NULL,0));vm.allocations=&allocations;vm.resolve=resolve;tsfp_host_text_codec_init(&host,&codec);
    for(uint32_t n=32766;n<=32768;++n) {
        memset(ram,0xa5,sizeof(ram));memset(ram+4096,'A',n);put(32,2,n);put(34,2,0);put(36,4,4096);
        put(16,2,0x7777);put(18,2,65534);put(20,4,80000);memcpy(before,ram,sizeof(ram));
        if(n==32766) {
            assert(tsfp_text_string_convert(&vm,&codec,16,32,0,TSFP_TEXT_TO_UNICODE)==0);
            assert(get(16,2)==65532 && get(80000+65532,2)==0 && ram[80000+65534]==0xa5);
        } else {assert(tsfp_text_string_convert(&vm,&codec,16,32,0,TSFP_TEXT_TO_UNICODE)==0xc00000f0);assert(!memcmp(before,ram,sizeof(ram)));}
    }
    for(unsigned cap=32767;cap<=32768;++cap) {
        memset(ram,0xa5,sizeof(ram));for(unsigned i=0;i<32767;++i)put(4096+i*2,2,0x100);
        put(32,2,65534);put(34,2,0);put(36,4,4096);put(16,2,0x7777);put(18,2,cap);put(20,4,80000);
        assert(tsfp_text_string_convert(&vm,&codec,16,32,0,TSFP_TEXT_TO_ANSI)==(cap==32767?0x80000005u:0));
        assert(get(16,2)==(cap==32767?32766u:32767u));assert(ram[80000+cap-1]==0 && ram[80000+cap]==0xa5);
    }
    memcpy(before,ram,sizeof(ram));bad=codec;bad.convert=huge;
    assert(tsfp_text_string_convert(&vm,&bad,16,32,0,TSFP_TEXT_TO_UNICODE)==0xc00000f0);
    assert(tsfp_text_string_convert(NULL,&codec,16,32,0,0)==0xc000000d);
    assert(tsfp_text_string_convert(&vm,NULL,16,32,0,0)==0xc000000d);
    assert(tsfp_text_string_convert(&vm,&codec,16,32,0,2)==0xc000000d);
    assert(tsfp_text_string_free(NULL,0));assert(!memcmp(before,ram,sizeof(ram)) && allocations.heap.count==0);
    memset(ram,0xa5,sizeof(ram));ram[4096]='A';put(32,2,1);put(34,2,1);put(36,4,4096);put(16,2,0);put(18,2,0);put(20,4,0);
    assert(tsfp_text_string_convert(&vm,&codec,16,32,1,TSFP_TEXT_TO_UNICODE)==0 && allocations.heap.count==1);
    assert(get(16,2)==2 && get(18,2)==4 && get(get(20,4),2)=='A');
    assert(tsfp_text_string_free(&vm,16) && allocations.heap.count==0 && get(16,4)==0 && get(20,4)==0);
    assert(tsfp_text_string_convert(&vm,&codec,16,32,1,TSFP_TEXT_TO_UNICODE)==0);
    put(48,2,0);put(50,2,0);put(52,4,0);
    assert(tsfp_text_string_convert(&vm,&codec,48,16,1,TSFP_TEXT_TO_ANSI)==0 && allocations.heap.count==2);
    assert(get(48,2)==1 && ram[get(52,4)]=='A' && ram[get(52,4)+1]==0);
    assert(tsfp_text_string_free(&vm,16) && tsfp_text_string_free(&vm,48) && allocations.heap.count==0);
    tsfp_allocations_stop(&allocations);return 0;
}
