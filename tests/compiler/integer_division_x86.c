#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
typedef LARGE_INTEGER (NTAPI *divide_api)(LARGE_INTEGER,ULONG,PULONG);
typedef LARGE_INTEGER (NTAPI *magic_api)(LARGE_INTEGER,LARGE_INTEGER,CCHAR);
int main(void) {
 HMODULE n=GetModuleHandleW(L"ntdll.dll");FARPROC raw;divide_api divide;magic_api magic;
 unsigned op,shift;uint64_t a,b;uint32_t code;ULONG rem;LARGE_INTEGER x,y,q;
 raw=GetProcAddress(n,"RtlExtendedLargeIntegerDivide");memcpy(&divide,&raw,sizeof(divide));
 raw=GetProcAddress(n,"RtlExtendedMagicDivide");memcpy(&magic,&raw,sizeof(magic));
 if(!divide || !magic)return 3;
 while(scanf("%u %" SCNx64 " %" SCNx64 " %u",&op,&a,&b,&shift)==4) {
  volatile uintptr_t before_sp,after_sp;uint64_t bits;memcpy(&x,&a,8);memcpy(&y,&b,8);memset(&q,0xa5,8);rem=0x5a5a5a5a;code=0;
  __asm mov before_sp, esp
  __try {if(op)q=magic(x,y,(CCHAR)shift);else q=divide(x,(ULONG)b,&rem);}
  __except(EXCEPTION_EXECUTE_HANDLER) {code=GetExceptionCode();}
  __asm mov after_sp, esp
  if(before_sp!=after_sp)return 5;
  memcpy(&bits,&q,8);printf("%016" PRIx64 " %08lx %08" PRIx32 "\n",bits,rem,code);
 }
 return 0;
}
