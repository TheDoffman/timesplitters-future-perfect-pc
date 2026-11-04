/* Real x86 ntdll walks over controlled native stack frames, never guest pointers. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <io.h>
#include <fcntl.h>
typedef ULONG (NTAPI *walk_type)(PVOID*,ULONG,ULONG);
typedef USHORT (NTAPI *capture_type)(ULONG,ULONG,PVOID*,PULONG);
typedef VOID (NTAPI *callers_type)(PVOID*,PVOID*);
static walk_type walk;static capture_type capture;static callers_type callers;
static uintptr_t frame,saved_bp,saved_sp,after_sp,callsite;static ULONG mode,skip,capacity,got,hash;
static PVOID captured[32],first,second;
__declspec(noinline) static int marker0(void){return 101;}
__declspec(noinline) static int marker1(void){return 202;}
__declspec(noinline) static int marker2(void){return 303;}
__declspec(noinline) static int marker3(void){return 404;}
__declspec(naked) static void invoke(void) {
 __asm {
  mov saved_sp,esp
  mov saved_bp,ebp
  mov ebp,frame
  cmp mode,0
  jne walk_mode
  mov eax,offset capture_return
  mov callsite,eax
  push offset hash
  push offset captured
  push capacity
  push skip
  call dword ptr [capture]
 capture_return:
  movzx eax,ax
  jmp finish
 walk_mode:
  cmp mode,1
  jne caller_mode
  mov eax,offset walk_return
  mov callsite,eax
  push 0
  push capacity
  push offset captured
  call dword ptr [walk]
 walk_return:
  jmp finish
 caller_mode:
  push offset second
  push offset first
  call dword ptr [callers]
  xor eax,eax
 finish:
  mov got,eax
  mov after_sp,esp
  mov ebp,saved_bp
  ret
 }
}
static uint32_t normalized(uintptr_t p) {
    if(!p)return 0;
    if(p==callsite)return 0x1000;
    if(p==(uintptr_t)marker0)return 0x1100;
    if(p==(uintptr_t)marker1)return 0x1104;
    if(p==(uintptr_t)marker2)return 0x1108;
    if(p==(uintptr_t)marker3)return 0x110c;
    assert(!"unexpected native return PC");return 0;
}
int main(void) {
    uintptr_t chain[64],markers[4]={(uintptr_t)marker0,(uintptr_t)marker1,(uintptr_t)marker2,(uintptr_t)marker3};
    uint32_t packet[4],result[36];HMODULE n=GetModuleHandleA("ntdll.dll");FARPROC raw;
    raw=GetProcAddress(n,"RtlWalkFrameChain");memcpy(&walk,&raw,sizeof(walk));assert(walk);
    raw=GetProcAddress(n,"RtlCaptureStackBackTrace");memcpy(&capture,&raw,sizeof(capture));assert(capture);
    raw=GetProcAddress(n,"RtlGetCallersAddress");memcpy(&callers,&raw,sizeof(callers));assert(callers);
    assert(_setmode(_fileno(stdin),_O_BINARY)!=-1);assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
    while(fread(packet,1,sizeof(packet),stdin)==sizeof(packet)) {
        uint32_t depth=packet[3],sum=0;mode=packet[0];skip=packet[1];capacity=packet[2];assert(mode<3 && capacity<=32 && depth<=24);
        memset(chain,0,sizeof(chain));memset(captured,0,sizeof(captured));memset(result,0,sizeof(result));hash=0;first=second=NULL;callsite=0;
        for(uint32_t i=0;i<depth;++i){chain[i*2]=i+1<depth?(uintptr_t)&chain[(i+1)*2]:0;chain[i*2+1]=markers[i%4];}
        frame=depth?(uintptr_t)chain:0;invoke();assert(saved_sp==after_sp && got<=capacity && got<=32);
        result[0]=got;result[2]=normalized((uintptr_t)first);result[3]=normalized((uintptr_t)second);
        for(uint32_t i=0;i<got;++i){sum+=(uint32_t)(uintptr_t)captured[i];result[4+i]=normalized((uintptr_t)captured[i]);result[1]+=result[4+i];}
        if(mode==0)assert(hash==sum);
        assert(fwrite(result,1,sizeof(result),stdout)==sizeof(result));
    }
    return 0;
}
