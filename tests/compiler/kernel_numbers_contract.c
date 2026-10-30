/* Actual import bodies with complete fixed frames and unchanged registers. */
#include "guest_numbers.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
static unsigned char ram[1024];
static uint32_t g_eax,g_edx,g_esp;static unsigned frame_bytes,available,lookups,frame_reads;
static void put(uint32_t at,uint32_t value,unsigned bytes) {
    for(unsigned i=0;i<bytes;++i)ram[at+i]=(unsigned char)(value>>(i*8));
}
static uint32_t get(uint32_t at,unsigned bytes) {
    uint32_t result=0;for(unsigned i=0;i<bytes;++i)result|=(uint32_t)ram[at+i]<<(i*8);return result;
}
static void *resolve(void *host,uint32_t address,uint32_t bytes,int write) {
    uint32_t at=address&UINT32_C(0x7fffffff);(void)host;(void)write;
    if(at==128){++frame_reads;if(bytes!=frame_bytes || bytes>available)return NULL;}
    if(!at || at>=sizeof(ram) || bytes>sizeof(ram)-at)return NULL;
    return ram+at;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    ++lookups;memset(vm,0,sizeof(*vm));vm->resolve=resolve;
}
#include "guest_kernel_numbers.inc"
static void frame(const uint32_t *a,unsigned count,uint32_t stack) {
    memset(ram,0xa5,sizeof(ram));g_eax=0x12345678;g_edx=0x87654321;g_esp=stack;
    frame_bytes=available=count*4;frame_reads=lookups=0;
    for(unsigned i=0;i<count;++i)put(128+i*4,a[i],4);
}
static void registers(uint32_t stack) {assert(g_esp==stack && g_edx==0x87654321 && lookups==1 && frame_reads==1);}
int main(void) {
    uint32_t a[4]={32,0,400,0};unsigned char before[1024];
    frame(a,3,128);memcpy(ram+32,"0x2A",5);bridge_RtlCharToInteger();registers(128);assert(g_eax==0 && get(400,4)==42);
    a[0]=16;a[1]=10;a[2]=400;frame(a,3,128);put(16,6,2);put(18,8,2);put(20,64,4);put(64,'1',2);put(66,'2',2);put(68,'3',2);
    bridge_RtlUnicodeStringToInteger();registers(128);assert(g_eax==0 && get(400,4)==123);
    a[0]=255;a[1]=16;a[2]=2;a[3]=400;frame(a,4,128);bridge_RtlIntegerToChar();registers(128);assert(g_eax==0 && ram[400]=='F' && ram[401]=='F' && ram[402]==0xa5);
    a[0]=UINT32_MAX;a[1]=10;a[2]=16;frame(a,3,128);put(18,22,2);put(20,64,4);
    bridge_RtlIntegerToUnicodeString();registers(128);assert(g_eax==0 && get(16,2)==20 && get(64,2)=='4' && get(84,2)==0);
    a[0]=0x80000020;a[1]=0;a[2]=400;frame(a,3,0x80000080);memcpy(ram+32,"42",3);bridge_RtlCharToInteger();registers(0x80000080);assert(g_eax==0 && get(400,4)==42);
    frame(a,3,128);available=11;memcpy(before,ram,sizeof(ram));bridge_RtlCharToInteger();registers(128);assert(g_eax==0xc000000d && memcmp(before,ram,sizeof(ram))==0);
    frame(a,3,0xfffffff8);memcpy(before,ram,sizeof(ram));bridge_RtlCharToInteger();assert(g_eax==0xc000000d && g_esp==0xfffffff8 && lookups==1 && frame_reads==0 && memcmp(before,ram,sizeof(ram))==0);
    frame(a,3,0);bridge_RtlCharToInteger();assert(g_eax==0xc000000d && g_esp==0 && lookups==1 && frame_reads==0);
    return 0;
}
