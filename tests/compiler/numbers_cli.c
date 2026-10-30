/* Original whole-RAM protocol fixture, including checked aliases and access. */
#include "guest_numbers.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static unsigned char ram[512];
static void *resolve(void *host,uint32_t address,uint32_t bytes,int write) {
    uint32_t at=address&UINT32_C(0x7fffffff);(void)host;
    if(!at || at>=sizeof(ram) || bytes>sizeof(ram)-at)return NULL;
    if(at<480 && at+bytes>464)return NULL;
    if(write && at<416 && at+bytes>400)return NULL;
    return ram+at;
}
int main(void) {
    uint32_t a[5],status;tsfp_guest_vm vm;memset(&vm,0,sizeof(vm));
    assert(tsfp_number_from_ansi(NULL,1,10,2)==UINT32_C(0xc000000d));
    assert(tsfp_number_from_unicode(&vm,16,10,320)==UINT32_C(0xc000000d));
    assert(tsfp_number_to_ansi(NULL,1,10,2,3)==UINT32_C(0xc000000d));
    assert(tsfp_number_to_unicode(NULL,1,10,16)==UINT32_C(0xc000000d));
    vm.resolve=resolve;
    #ifdef _WIN32
    if(_setmode(_fileno(stdin),_O_BINARY)<0 || _setmode(_fileno(stdout),_O_BINARY)<0)return 6;
#endif
    while(fread(a,1,sizeof(a),stdin)==sizeof(a)) {
        if(fread(ram,1,sizeof(ram),stdin)!=sizeof(ram))return 2;
        switch(a[0]) {
        case 0:status=tsfp_number_from_ansi(&vm,a[1],a[2],a[3]);break;
        case 1:status=tsfp_number_from_unicode(&vm,a[1],a[2],a[3]);break;
        case 2:status=tsfp_number_to_ansi(&vm,a[1],a[2],a[3],a[4]);break;
        case 3:status=tsfp_number_to_unicode(&vm,a[1],a[2],a[3]);break;
        default:return 3;
        }
        if(fwrite(&status,1,4,stdout)!=4 || fwrite(ram,1,sizeof(ram),stdout)!=sizeof(ram))return 4;
    }
    return ferror(stdin)?5:0;
}
