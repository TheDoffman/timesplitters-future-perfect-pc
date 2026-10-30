#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#include "guest_kernel_rtl.h"
#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"kernel RTL host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char ram[1024],before[1024];static tsfp_guest_vm vm;static tsfp_kernel_rtl_site site={0xabcdef01,257,0x8000,0x9000};
static char last_log[2048];static int single;
void xbox_log(int level,int category,const char *format,...) {
    va_list arguments;CHECK((level==XBOX_LOG_WARN || level==XBOX_LOG_ERROR) && category==XBOX_LOG_RTL);
    va_start(arguments,format);(void)vsnprintf(last_log,sizeof(last_log),format,arguments);va_end(arguments);
}
static void *resolve(void *context,uint32_t address,uint32_t bytes,int writable) {
    CHECK(context==ram && address && (bytes==1 || bytes==4) && !writable);
    if(address==900 && bytes==1)return ram+17;
    if(address>=sizeof(ram) || bytes>sizeof(ram)-address)return NULL;return ram+address;
}
#ifdef _WIN32
static DWORD WINAPI worker(void *unused)
#else
static void *worker(void *unused)
#endif
{
    (void)unused;
    CHECK(tsfp_kernel_rtl_critical(&vm,900,TSFP_KERNEL_CRITICAL_TRY,&site)==TSFP_CRITICAL_BUSY);
    CHECK(tsfp_kernel_rtl_critical(&vm,17,TSFP_KERNEL_CRITICAL_LEAVE,&site)==TSFP_CRITICAL_NOT_OWNER);
    if(single)CHECK(tsfp_kernel_rtl_critical(&vm,33,TSFP_KERNEL_CRITICAL_TRY,&site)==TSFP_CRITICAL_BUSY);
    else {
        CHECK(tsfp_kernel_rtl_critical(&vm,33,TSFP_KERNEL_CRITICAL_TRY,&site)==TSFP_CRITICAL_OK);
        CHECK(tsfp_kernel_rtl_critical(&vm,33,TSFP_KERNEL_CRITICAL_LEAVE,&site)==TSFP_CRITICAL_OK);
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc>1 && !strcmp(argv[1],"fault"))tsfp_kernel_rtl_fault(TSFP_CRITICAL_NO_MEMORY);
    single=argc>1 && !strcmp(argv[1],"single");memset(ram,0xa5,sizeof(ram));vm.host=ram;vm.resolve=resolve;
    memcpy(ram+129,"api %n",7);memcpy(ram+161,"x > 0",6);memcpy(ram+193,"hello",6);
    ram[257]=0x40;ram[258]=0x80;ram[259]=ram[260]=0;memcpy(before,ram,sizeof(ram));
    CHECK(tsfp_kernel_rtl_critical(NULL,17,TSFP_KERNEL_CRITICAL_ENTER,&site)==TSFP_CRITICAL_INVALID);
    CHECK(tsfp_kernel_rtl_critical(&vm,0,TSFP_KERNEL_CRITICAL_ENTER,&site)==TSFP_CRITICAL_INVALID);
    CHECK(tsfp_kernel_rtl_critical(&vm,UINT32_MAX,TSFP_KERNEL_CRITICAL_ENTER,&site)==TSFP_CRITICAL_INVALID);
    CHECK(tsfp_kernel_rtl_critical(&vm,17,TSFP_KERNEL_CRITICAL_INIT,&site)==TSFP_CRITICAL_OK);
    CHECK(tsfp_kernel_rtl_critical(&vm,17,TSFP_KERNEL_CRITICAL_ENTER,&site)==TSFP_CRITICAL_OK);
    CHECK(tsfp_kernel_rtl_critical(&vm,900,TSFP_KERNEL_CRITICAL_INIT,&site)==TSFP_CRITICAL_OK);
    CHECK(tsfp_kernel_rtl_critical(&vm,900,TSFP_KERNEL_CRITICAL_TRY,&site)==TSFP_CRITICAL_OK);
#ifdef _WIN32
    {
        HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL);CHECK(thread);
        CHECK(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread));
    }
#else
    {
        pthread_t thread;CHECK(!pthread_create(&thread,NULL,worker,NULL));CHECK(!pthread_join(thread,NULL));
    }
#endif
    CHECK(tsfp_kernel_rtl_critical(&vm,17,TSFP_KERNEL_CRITICAL_LEAVE,&site)==TSFP_CRITICAL_OK);
    CHECK(tsfp_kernel_rtl_critical(&vm,900,TSFP_KERNEL_CRITICAL_LEAVE,&site)==TSFP_CRITICAL_OK);
    CHECK(tsfp_kernel_rtl_critical(&vm,17,TSFP_KERNEL_CRITICAL_LEAVE,&site)==TSFP_CRITICAL_NOT_OWNER);
    CHECK(tsfp_kernel_rtl_error(0x103)==997 && tsfp_kernel_rtl_error(0xc0000021)==1455);
#ifdef _WIN32
    CHECK(tsfp_kernel_rtl_error(0xc0000005)==998);
#else
    CHECK(tsfp_kernel_rtl_error(0xc0000005)==317);
#endif
    tsfp_kernel_rtl_rip(&vm,129,161,193);CHECK(!strcmp(last_log,"RtlRip: api %n - x > 0: hello"));
    tsfp_kernel_rtl_rip(&vm,0,UINT32_MAX,1023);CHECK(!strcmp(last_log,"RtlRip: ? - ?: ?"));
    CHECK(!memcmp(before,ram,sizeof(ram)));
    puts("PASS: actual mapped kernel critical/try/leave aliases and independent ownership, native status fallback, bounded Rip and untouched guest bytes");return 0;
}
