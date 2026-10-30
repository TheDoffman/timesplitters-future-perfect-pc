#ifndef TSFP_NATIVE_CRASH_H
#define TSFP_NATIVE_CRASH_H
#include <signal.h>
#include <stdint.h>
typedef void (*NativeGuestCapture)(uint32_t registers[8]);
int tsfp_native_crash_init(NativeGuestCapture capture);
int tsfp_native_crash_helper(int argc,char **argv);
void tsfp_native_crash_signal(int sig,siginfo_t *info,void *context);
void tsfp_native_crash_frame(void);
void tsfp_native_crash_memory(uint64_t base);
void tsfp_native_crash_armed(int armed);
void tsfp_native_crash_focus(int focused);
void tsfp_native_crash_inhibit(int entering);
void tsfp_native_crash_stop(void);
#endif
