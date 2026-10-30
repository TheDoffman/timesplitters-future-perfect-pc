/* Original project kernel lifecycle and diagnostic sink. Initialization and
 * shutdown require caller quiescence; this is not a worker drain/join API.
 * Guest import dispatch belongs to the bridge, not a native pointer table. */
#include "kernel.h"
#if !defined(_WIN32)
#include "platform/win32_compat.h"
#endif
#include "xbox_memory_layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static CRITICAL_SECTION diagnostic_lock;
static FILE *diagnostic_file;
static int diagnostic_ready,diagnostic_quiet,diagnostic_level=XBOX_LOG_INFO;

void xbox_log(int level,const char *subsystem,const char *format,...) {
    static const char *const labels[]={"ERROR","WARN ","INFO ","DEBUG","TRACE"};
    FILE *sink;SYSTEMTIME now;va_list values;
    if(diagnostic_quiet || level>diagnostic_level || !format) return;
    if(diagnostic_ready) EnterCriticalSection(&diagnostic_lock);
    sink=diagnostic_file?diagnostic_file:stderr;
    GetLocalTime(&now);
    fprintf(sink,"[%02u:%02u:%02u.%03u] %s [%-6s] ",
        (unsigned)now.wHour,(unsigned)now.wMinute,(unsigned)now.wSecond,
        (unsigned)now.wMilliseconds,
        level>=XBOX_LOG_ERROR && level<=XBOX_LOG_TRACE?labels[level-XBOX_LOG_ERROR]:"?????",
        subsystem?subsystem:"");
    va_start(values,format);vfprintf(sink,format,values);va_end(values);
    fputc('\n',sink);fflush(sink);
    if(diagnostic_ready) LeaveCriticalSection(&diagnostic_lock);
}

void xbox_kernel_init(void) {
    const char *value;
    if(diagnostic_ready) return;
    xbox_GuestLockInit();xbox_GuestLockEnter();xbox_GuestLiftedEnter();
    xbox_hal_init_timers();
    InitializeCriticalSection(&diagnostic_lock);
    diagnostic_level=XBOX_LOG_INFO;
    value=getenv("XBOX_LOG_LEVEL");
    if(value) {
        long requested=strtol(value,NULL,10);
        diagnostic_level=requested<XBOX_LOG_ERROR?XBOX_LOG_ERROR:
                         requested>XBOX_LOG_TRACE?XBOX_LOG_TRACE:(int)requested;
    }
    value=getenv("TSFP_DEBUG_QUIET");diagnostic_quiet=value && !strcmp(value,"1");
    diagnostic_file=diagnostic_quiet?NULL:fopen("xbox_kernel.log","w");
    diagnostic_ready=1;
    xbox_log(XBOX_LOG_INFO,"THUNK","Project kernel services ready; imports use guest bridge dispatch");
}

void xbox_kernel_shutdown(void) {
    if(!diagnostic_ready) return;
    xbox_log(XBOX_LOG_INFO,"THUNK","Project kernel diagnostic sink stopping");
    if(diagnostic_file) fclose(diagnostic_file);
    diagnostic_file=NULL;diagnostic_ready=0;
    DeleteCriticalSection(&diagnostic_lock);
}
