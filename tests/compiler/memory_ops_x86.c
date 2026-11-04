/* Actual x86 ntdll references, with native pointers kept separate from guest words. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
typedef VOID (NTAPI *fill_type)(PVOID,SIZE_T,UCHAR);
typedef VOID (NTAPI *zero_type)(PVOID,SIZE_T);
typedef VOID (NTAPI *pattern_type)(PVOID,SIZE_T,ULONG);
typedef VOID (NTAPI *move_type)(PVOID,const VOID*,SIZE_T);
typedef ULONG (__fastcall *swap32_type)(ULONG);
typedef USHORT (__fastcall *swap16_type)(USHORT);
typedef VOID (NTAPI *mask_type)(PULONG,const GENERIC_MAPPING*);
static unsigned char ram[512];
static void *pointer(uint32_t a){return a?ram+(a&UINT32_C(0x7fffffff)):NULL;}
#define LOAD(variable,type,name) type variable;raw=GetProcAddress(module,name);assert(raw);memcpy(&variable,&raw,sizeof(variable))
int main(void) {
    HMODULE module=GetModuleHandleW(L"ntdll.dll");FARPROC raw;unsigned char packet[528];
    LOAD(fill,fill_type,"RtlFillMemory");LOAD(zero,zero_type,"RtlZeroMemory");
    LOAD(pattern,pattern_type,"RtlFillMemoryUlong");LOAD(move,move_type,"RtlMoveMemory");
    LOAD(swap32,swap32_type,"RtlUlongByteSwap");LOAD(swap16,swap16_type,"RtlUshortByteSwap");
    LOAD(mask,mask_type,"RtlMapGenericMask");
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
    while(fread(packet,1,sizeof(packet),stdin)==sizeof(packet)) {
        uint32_t args[4],result=0;volatile uintptr_t before_sp,after_sp;memcpy(args,packet,sizeof(args));memcpy(ram,packet+16,sizeof(ram));
        __asm mov before_sp, esp
        switch(args[0]) {
            case 0:fill(pointer(args[1]),args[2],(UCHAR)args[3]);break;
            case 1:zero(pointer(args[1]),args[2]);break;
            case 2:pattern(pointer(args[1]),args[2],args[3]);break;
            case 3:move(pointer(args[1]),pointer(args[2]),args[3]);break;
            case 4:result=swap32(args[1]);break;
            case 5:result=swap16((USHORT)args[1]);break;
            case 6:mask((PULONG)pointer(args[1]),(const GENERIC_MAPPING*)pointer(args[2]));break;
            default:return 2;
        }
        __asm mov after_sp, esp
        if(before_sp!=after_sp){fprintf(stderr,"native op=%u stack before=%lu after=%lu\n",args[0],(unsigned long)before_sp,(unsigned long)after_sp);return 3;}assert(fwrite(&result,1,4,stdout)==4);assert(fwrite(ram,1,sizeof(ram),stdout)==sizeof(ram));
    }
    return 0;
}
