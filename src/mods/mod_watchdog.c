/* Last-resort worker wall limit, including non-interruptible Lua C functions.
   The game-side supervisor must independently bound/kill this untrusted process. */
#define _POSIX_C_SOURCE 200809L
#include "mod_watchdog.h"
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static volatile LONG64 deadline;
static uint64_t clock_ms(void){return GetTickCount64();}
static DWORD WINAPI watch(void *unused) {
    (void)unused;
    for(;;){LONG64 value=InterlockedCompareExchange64(&deadline,0,0);
        if(value && clock_ms()>=(uint64_t)value && InterlockedCompareExchange64(&deadline,-1,value)==value)ExitProcess(124);
        Sleep(5);
    }
}
int tsfp_mod_watchdog_start(void){HANDLE thread=CreateThread(NULL,0,watch,NULL,0,NULL);if(!thread)return 0;CloseHandle(thread);return 1;}
void tsfp_mod_watchdog_arm(void){InterlockedExchange64(&deadline,(LONG64)(clock_ms()+500));}
void tsfp_mod_watchdog_disarm(void){InterlockedExchange64(&deadline,0);}
#else
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <unistd.h>
static atomic_uint_fast64_t deadline;
static uint64_t clock_ms(void){struct timespec time;clock_gettime(CLOCK_MONOTONIC,&time);return (uint64_t)time.tv_sec*1000+(uint64_t)time.tv_nsec/1000000;}
static void *watch(void *unused) {
    struct timespec delay={0,5000000};(void)unused;
    for(;;){uint_fast64_t value=atomic_load(&deadline);
        if(value && clock_ms()>=value && atomic_compare_exchange_strong(&deadline,&value,UINT64_MAX))_exit(124);
        nanosleep(&delay,NULL);
    }
    return NULL;
}
int tsfp_mod_watchdog_start(void){pthread_t thread;if(pthread_create(&thread,NULL,watch,NULL))return 0;pthread_detach(thread);return 1;}
void tsfp_mod_watchdog_arm(void){atomic_store(&deadline,clock_ms()+500);}
void tsfp_mod_watchdog_disarm(void){atomic_store(&deadline,0);}
#endif
