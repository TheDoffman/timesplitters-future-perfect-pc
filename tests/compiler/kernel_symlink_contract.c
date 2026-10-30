#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"link adapter line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {void *host;void *(*resolve)(void *,uint32_t,uint32_t,int);} tsfp_guest_vm;
static unsigned char memory[4096];static uint32_t args[2],g_eax;static unsigned reads;
#define STACK_ARG(i) args[i]
static void *resolve(void *p,uint32_t address,uint32_t bytes,int writable) {
    CHECK(p==memory && !writable);++reads;
    if(address==1 || address==9)CHECK(bytes==8);
    if(address>sizeof(memory) || bytes>sizeof(memory)-address)return NULL;
    return memory+address;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {vm->host=memory;vm->resolve=resolve;}
#include "guest_kernel_symlinks.inc"
static void header(unsigned address,unsigned buffer,unsigned n,unsigned max) {
    memory[address]=(unsigned char)n;memory[address+1]=(unsigned char)(n>>8);
    memory[address+2]=(unsigned char)max;memory[address+3]=(unsigned char)(max>>8);
    memory[address+4]=(unsigned char)buffer;memory[address+5]=(unsigned char)(buffer>>8);
    memory[address+6]=(unsigned char)(buffer>>16);memory[address+7]=(unsigned char)(buffer>>24);
}
int main(void) {
    unsigned char out[160],before[160];size_t n;unsigned i;char name[80],other[80];
    memset(name,'n',80);memcpy(other,name,80);other[79]='m';memcpy(memory+256,name,80);memset(memory+700,'t',160);memory[859]='z';
    header(1,256,80,80);header(9,700,160,160);args[0]=1;args[1]=9;
    bridge_IoCreateSymbolicLink();CHECK(!g_eax && reads==4);
    bridge_IoCreateSymbolicLink();CHECK(g_eax==0xc0000035);
    CHECK(tsfp_kernel_symlink_copy(name,80,out,sizeof(out),&n)==TSFP_SYMLINK_OK && n==160 && out[159]=='z');
    memory[859]='x';CHECK(tsfp_kernel_symlink_copy(name,80,out,sizeof(out),&n)==TSFP_SYMLINK_OK && out[159]=='z');
    memcpy(memory+256,other,80);bridge_IoCreateSymbolicLink();CHECK(!g_eax);
    bridge_IoDeleteSymbolicLink();CHECK(!g_eax);bridge_IoDeleteSymbolicLink();CHECK(g_eax==0xc0000034);
    memcpy(memory+256,name,80);bridge_IoDeleteSymbolicLink();CHECK(!g_eax);
    memset(out,0x66,sizeof(out));memcpy(before,out,sizeof(out));n=99;
    CHECK(tsfp_kernel_symlink_copy(name,80,out,sizeof(out),&n)==TSFP_SYMLINK_MISSING && n==99 && !memcmp(out,before,sizeof(out)));
    header(1,256,80,79);bridge_IoCreateSymbolicLink();CHECK(g_eax==0xc000000d);
    header(1,4090,80,80);bridge_IoCreateSymbolicLink();CHECK(g_eax==0xc000000d);
    header(1,0xfffffff0,80,80);bridge_IoCreateSymbolicLink();CHECK(g_eax==0xc000000d);
    args[0]=0xfffffffc;reads=0;bridge_IoDeleteSymbolicLink();CHECK(g_eax==0xc000000d && !reads);
    args[0]=0;bridge_IoCreateSymbolicLink();CHECK(g_eax==0xc000000d);
    for(i=0;i<32;++i) {name[0]=(char)i;CHECK(tsfp_kernel_symlink_create(name,80,"v",1)==TSFP_SYMLINK_OK);}
    name[0]=33;CHECK(tsfp_kernel_symlink_create(name,80,"v",1)==TSFP_SYMLINK_RESOURCE);
    for(i=0;i<32;++i) {name[0]=(char)i;CHECK(tsfp_kernel_symlink_remove(name,80)==TSFP_SYMLINK_OK);}
    puts("PASS: original mapped counted-string adapters, copied full names/targets, exact guest statuses and32-entry capacity");return 0;
}
