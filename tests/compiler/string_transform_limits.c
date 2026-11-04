/* Maximum descriptor boundaries, empty ownership, lifetime and host leaf checks. */
#include "guest_string_transform.h"
#include "host_string_case.h"
#include "guest_text_codec.h"
#include <assert.h>
#include <ctype.h>
#include <wctype.h>
#include <stdlib.h>
#include <string.h>
static unsigned char ram[200000];
static void *resolve(void *host,uint32_t a,uint32_t n,int write){(void)host;(void)write;return a && n && a<sizeof(ram) && n<=sizeof(ram)-a?ram+a:NULL;}
static void word(unsigned char *p,uint32_t n){p[0]=(unsigned char)n;p[1]=(unsigned char)(n>>8);}
static void dword(unsigned char *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i));}
static uint32_t readword(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8);}
int main(void) {
    tsfp_allocations arena;tsfp_allocation_record owners[8];tsfp_allocation_config c;tsfp_guest_vm vm;unsigned char before[8];
    memset(&c,0,sizeof(c));c.ram=ram;c.ram_size=sizeof(ram);c.heap_begin=131072;c.reserve_begin=sizeof(ram);
    assert(tsfp_allocations_init(&arena,&c,owners,8,NULL,0));memset(&vm,0,sizeof(vm));vm.resolve=resolve;vm.allocations=&arena;
    for(unsigned n=32765;n<=32768;++n) {
        memset(ram,0xa5,131072);for(unsigned i=0;i<n;++i){ram[1024+i*2]='a';ram[1025+i*2]=0;}
        ram[1024+n*2]=ram[1025+n*2]=0;memcpy(before,ram+16,8);
        int result=tsfp_string_create(&vm,16,1024);
        assert(result==(n<=32766));
        if(result){assert(readword(ram+16)==n*2 && readword(ram+18)==n*2+2);assert(tsfp_text_string_free(&vm,16));assert(arena.heap.count==0);}
        else {assert(memcmp(before,ram+16,8)==0 && arena.heap.count==0);}
    }
    /* A counted case result may use all65534 bytes without a terminator. */
    memset(ram,0xa5,sizeof(ram));word(ram+32,65534);word(ram+34,0);dword(ram+36,1024);
    for(unsigned i=0;i<32767;++i){ram[1024+i*2]='a';ram[1025+i*2]=0;}
    assert(tsfp_string_transform(&vm,tsfp_host_string_case,16,32,1,1)==0);
    assert(readword(ram+16)==65534 && readword(ram+18)==65534 && arena.heap.count==1);
    assert(ram[131072]=='A' && ram[196604]=='A' && ram[196606]==0xa5);
    assert(tsfp_text_string_free(&vm,16) && arena.heap.count==0);
    /* Empty case allocation owns a minimum block, but publishes zero capacity. */
    memset(ram+16,0xa5,24);memset(ram+32,0,8);
    assert(tsfp_string_transform(&vm,tsfp_host_string_case,16,32,1,0)==0);
    assert(readword(ram+16)==0 && readword(ram+18)==0 && arena.heap.count==1);
    assert(tsfp_text_string_free(&vm,16) && arena.heap.count==0);
    memcpy(before,ram+16,8);
    assert(tsfp_string_transform(&vm,NULL,16,32,0,1)==UINT32_C(0xc000000d));
    assert(!tsfp_string_create(NULL,16,1024) && !tsfp_string_create(&vm,16,0));
    assert(memcmp(before,ram+16,8)==0);
    for(unsigned width=1;width<=2;++width)for(unsigned upper=0;upper<2;++upper)for(uint32_t v=0;v<65536;++v) {
        uint32_t x=v&(width==1?255u:65535u),expected;
        if(width==1)expected=(uint32_t)(upper?toupper((unsigned char)x):tolower((unsigned char)x))&255;
        else expected=(uint32_t)(upper?towupper((wint_t)x):towlower((wint_t)x))&65535;
        assert(tsfp_host_string_case(v|UINT32_C(0xdead0000),width,(int)upper)==expected);
    }
    assert(tsfp_host_string_case(123,3,1)==123);tsfp_allocations_stop(&arena);return 0;
}
