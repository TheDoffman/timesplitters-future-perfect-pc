/* Exercise the production exit hook with faults inside its diagnostics. */
#include <windows.h>
#include <dbghelp.h>
#include <stdlib.h>
static int test_mode;
static USHORT WINAPI test_capture(DWORD skip,DWORD count,PVOID *frames,PDWORD hash)
{
    (void)skip;(void)count;(void)hash;
    if(test_mode==1)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,NULL);
    if(test_mode==2){frames[0]=(void *)&test_capture;return 1;}
    return 0;
}
static BOOL WINAPI test_symbol(HANDLE process,DWORD64 address,PDWORD64 offset,PSYMBOL_INFO symbol)
{
    (void)process;(void)address;(void)offset;(void)symbol;
    RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,NULL);
    return FALSE;
}
#undef CaptureStackBackTrace
#define CaptureStackBackTrace test_capture
#define SymFromAddr test_symbol
#include "kernel/exit_trace.c"
RECOMP_TLS uint32_t g_esp;
ptrdiff_t g_xbox_mem_offset;
int main(int argc,char **argv)
{
    if(argc!=3)return 90;
    test_mode=atoi(argv[1]);
    SetErrorMode(SEM_NOGPFAULTERRORBOX);
    if(test_mode==4)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,NULL);
    if(test_mode==3){
        void *page=VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_NOACCESS);
        if(!page)return 91;
        g_esp=0x10000;
        g_xbox_mem_offset=(ptrdiff_t)page-0x10000;
    }
    real_ExitProcess=ExitProcess;
    hook_ExitProcess((UINT)atoi(argv[2]));
    return 92;
}
