#ifndef TSFP_PC_CRASH_H
#define TSFP_PC_CRASH_H
#include <windows.h>
#include <stdint.h>
#include "pc_breadcrumbs.h"

#define PC_CRASH_SCHEMA 3
#define PC_CRASH_PATH 1024
#define PC_CRASH_DUMP_LIMIT (16u * 1024u * 1024u)
#define PC_CRASH_WAIT_MS 12000
#define PC_HANG_TIMEOUT_MS 60000
#define PC_HANG_STARTUP_MS 180000
typedef struct PcCrashGuest {
    uint32_t valid, eax, ebx, ecx, edx, esi, edi, esp, ebp;
    uint64_t memory_base;
} PcCrashGuest;
/* Fixed shared-memory protocol; no target pointers are dereferenced by the helper. */
typedef struct PcCrashPacket {
    DWORD schema, process_id, thread_id;
    PcBreadcrumbs breadcrumbs;
    uint64_t started_ms;
    volatile LONG entered;
    EXCEPTION_RECORD exception;
    CONTEXT context;
    PcCrashGuest guest;
    /* Watchdog observes real frame progress, never a background timer beat. */
    volatile LONG watchdog_armed, watchdog_inhibited, snapshot_sequence, snapshot_writer;
    volatile LONG64 frame_sequence;
    DWORD watchdog_thread, hang_timeout_ms, startup_timeout_ms;
    uint64_t game_window;
    PcCrashGuest frame_guest;
    WCHAR root[PC_CRASH_PATH], executable[PC_CRASH_PATH], log[PC_CRASH_PATH];
} PcCrashPacket;
typedef void (*PcCrashGuestCapture)(PcCrashGuest *out);
int pc_crash_init(PcCrashGuestCapture capture);
void pc_crash_shutdown(void);
void pc_crash_watchdog_start(void);
void pc_crash_watchdog_stop(void);
void pc_crash_frame(void);
void pc_crash_window(HWND window);
void pc_crash_inhibit(int entering);
/* Exact command-line-only diagnostic modes; run before game/config initialization. */
int pc_crash_test_command(const char *command);
#endif
