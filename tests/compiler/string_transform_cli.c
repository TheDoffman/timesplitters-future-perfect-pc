/* Real creation/case providers and allocator against guarded guest RAM. */
#include "guest_string_transform.h"
#include "host_string_case.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static unsigned char ram[1024];static tsfp_allocations arena;static tsfp_allocation_record records[8];
static uint32_t g_eax,g_edx,g_ecx,g_esp,mode;static unsigned lookups,frame_size,available,expect_frame;static int missing;
static uint32_t read32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void write32(uint32_t v){unsigned char b[4];for(unsigned i=0;i<4;++i)b[i]=(unsigned char)(v>>(i*8));assert(fwrite(b,1,4,stdout)==4);}
static void *resolve(void *host,uint32_t address,uint32_t bytes,int write) {
    uint32_t at=address&UINT32_C(0x7fffffff);(void)host;
    if(expect_frame){expect_frame=0;assert(!write && bytes==frame_size);if(at!=64 || bytes>available)return NULL;}
    if(!at || !bytes || at>=sizeof(ram) || bytes>sizeof(ram)-at || (at<496 && at+bytes>480))return NULL;
    if(write==1 && ((at<416 && at+bytes>400) || ((mode&3)==2 && at+bytes>512)))return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    ++lookups;memset(vm,0,sizeof(*vm));vm->allocations=&arena;if(!missing)vm->resolve=resolve;expect_frame=1;
}
#include "guest_kernel_string_transform.inc"
static void invoke(unsigned op) {
    if(op==0)bridge_RtlCreateUnicodeString();else if(op==1)bridge_RtlUpcaseUnicodeString();else if(op==2)bridge_RtlDowncaseUnicodeString();
    else if(op==3)bridge_RtlUpperChar();else if(op==4)bridge_RtlLowerChar();else if(op==5)bridge_RtlUpcaseUnicodeChar();else bridge_RtlDowncaseUnicodeChar();
}
static void setup(unsigned op,uint32_t stack,uint32_t flags) {
    tsfp_allocation_config c;memset(&arena,0,sizeof(arena));memset(&c,0,sizeof(c));c.ram=ram;c.ram_size=sizeof(ram);c.heap_begin=512;c.reserve_begin=1024;
    assert(tsfp_allocations_init(&arena,&c,records,8,NULL,0));mode=flags;
    if((mode&3)==1 || (mode&3)==3)assert(tsfp_heap_allocate(&arena,(mode&3)==1?512:48,16)==512);
    g_eax=0x12345678;g_edx=0x87654321;g_ecx=0xabcdef01;g_esp=stack;
    lookups=expect_frame=0;missing=0;frame_size=available=op==0?8:op<3?12:4;
}
int main(int argc,char **argv) {
    unsigned char packet[1044],before[1024];int bridge=argc>=2 && !strcmp(argv[1],"bridge");
#ifdef _WIN32
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
    for(unsigned op=0;op<7;++op)for(unsigned bad=0;bad<5;++bad) {
        unsigned bytes=op==0?8:op<3?12:4;uint32_t stack=bad==0?0:bad==1?UINT32_MAX-(bytes-2):bad==4?1024:64;
        setup(op,stack,0);memset(ram,0xa5,sizeof(ram));memcpy(before,ram,sizeof(ram));
        if(bad==2)--available;if(bad==3)missing=1;
        invoke(op);assert(g_eax==(op==1 || op==2?UINT32_C(0xc000000d):0) && g_edx==0x87654321 && g_ecx==0xabcdef01 && g_esp==stack && lookups==1);
        assert(memcmp(before,ram,sizeof(ram))==0);tsfp_allocations_stop(&arena);
    }
    while(fread(packet,1,sizeof(packet),stdin)==sizeof(packet)) {
        uint32_t op=read32(packet),a=read32(packet+4),b=read32(packet+8),flag=read32(packet+12),options=read32(packet+16),result;
        assert(op<7);setup(op,64,options);memcpy(ram,packet+20,sizeof(ram));
        if(bridge){invoke(op);result=g_eax;assert(lookups==1 && !expect_frame);}
        else {
            tsfp_guest_vm vm;memset(&vm,0,sizeof(vm));vm.allocations=&arena;vm.resolve=resolve;
            if(op==0)result=(uint32_t)tsfp_string_create(&vm,a,b);
            else if(op<3)result=tsfp_string_transform(&vm,tsfp_host_string_case,a,b,(uint8_t)flag,op==1);
            else result=tsfp_host_string_case(a,op<5?1:2,op==3 || op==5);
            assert(!lookups);
        }
        assert(g_edx==0x87654321 && g_ecx==0xabcdef01 && g_esp==64);
        write32(result);write32((uint32_t)arena.heap.count);write32(arena.heap.count?records[0].address:0);write32(arena.heap.count?records[0].bytes:0);
        assert(fwrite(ram,1,sizeof(ram),stdout)==sizeof(ram));tsfp_allocations_stop(&arena);
    }
    return 0;
}
