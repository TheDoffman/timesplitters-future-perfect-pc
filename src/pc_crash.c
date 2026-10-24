#define _WIN32_WINNT 0x0601
#include "pc_crash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static PcCrashPacket *packet;
static HANDLE mapping, request, done, ready, helper;
static PcCrashGuestCapture guest_capture;
static LPTOP_LEVEL_EXCEPTION_FILTER previous_filter;

static LONG WINAPI fatal_exception(EXCEPTION_POINTERS *ep)
{
    if (!packet) return EXCEPTION_EXECUTE_HANDLER;
    if (InterlockedCompareExchange(&packet->entered, 1, 0)) {
        /* Another faulting thread must not terminate the process while the
         * helper is capturing the first thread's exception and memory. */
        WaitForSingleObject(done, PC_CRASH_WAIT_MS);
        return EXCEPTION_EXECUTE_HANDLER;
    }
    packet->thread_id = GetCurrentThreadId();
    packet->exception = *ep->ExceptionRecord;
    packet->exception.ExceptionRecord = NULL;
    packet->context = *ep->ContextRecord;
    if (guest_capture) guest_capture(&packet->guest);
    MemoryBarrier();
    SetEvent(request);
    /* No stdio, symbol lookup, heap allocation or game locks on this thread. */
    WaitForSingleObject(done, PC_CRASH_WAIT_MS);
    return EXCEPTION_EXECUTE_HANDLER;
}

void pc_crash_shutdown(void)
{
    if (packet) SetUnhandledExceptionFilter(previous_filter);
    if (packet) { pc_crumb_add(&packet->breadcrumbs,GetTickCount64()-packet->started_ms,PC_CRUMB_GUEST_STOP,0); InterlockedExchange(&packet->watchdog_armed, 0); }
    /* A rendering thread may still hold this view during CRT exit. Keep its
     * small mapping alive until process teardown, rather than racing an unmap. */
    packet = NULL;
    if (mapping) CloseHandle(mapping);
    if (request) CloseHandle(request);
    if (done) CloseHandle(done);
    if (ready) CloseHandle(ready);
    if (helper) CloseHandle(helper);
    mapping = request = done = ready = helper = NULL;
}

int pc_crash_init(PcCrashGuestCapture capture)
{
    SECURITY_ATTRIBUTES security = {sizeof security, NULL, TRUE};
    STARTUPINFOEXW startup = {0};
    PROCESS_INFORMATION process = {0};
    WCHAR helper_path[PC_CRASH_PATH], command[PC_CRASH_PATH + 180];
    HANDLE parent = NULL, inherited[5];
    SIZE_T bytes = 0;
    DWORD count;
    int success = 0;
    if (packet) return 1;
    mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE, 0, sizeof *packet, NULL);
    request = CreateEventW(&security, TRUE, FALSE, NULL);
    done = CreateEventW(&security, TRUE, FALSE, NULL);
    ready = CreateEventW(&security, TRUE, FALSE, NULL);
    if (!mapping || !request || !done || !ready) goto finish;
    packet = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof *packet);
    if (!packet) goto finish;
    packet->schema = PC_CRASH_SCHEMA;
    packet->started_ms = GetTickCount64();
    pc_crumb_add(&packet->breadcrumbs,0,PC_CRUMB_STARTED,0);
    packet->process_id = GetCurrentProcessId();
    packet->hang_timeout_ms = PC_HANG_TIMEOUT_MS;
    packet->startup_timeout_ms = PC_HANG_STARTUP_MS;
    count = GetModuleFileNameW(NULL, packet->executable, PC_CRASH_PATH);
    if (!count || count >= PC_CRASH_PATH - 64) goto finish;
    wcscpy_s(helper_path, PC_CRASH_PATH, packet->executable);
    WCHAR *slash = wcsrchr(helper_path, L'\\');
    if (!slash) goto finish;
    slash[1] = 0;
    count = GetEnvironmentVariableW(L"TSFP_CRASH_DIR", packet->root, PC_CRASH_PATH);
    if (count >= PC_CRASH_PATH) goto finish;
    if (!count) swprintf_s(packet->root, PC_CRASH_PATH, L"%sLogs\\Crashes", helper_path);
    if (wcslen(packet->root) >= PC_CRASH_PATH - 100) goto finish;
    count = GetEnvironmentVariableW(L"RECOMP_LOG_FILE", packet->log, PC_CRASH_PATH);
    if (count >= PC_CRASH_PATH) packet->log[0] = 0;
    wcscat_s(helper_path, PC_CRASH_PATH, L"tsfp_crash_helper.exe");
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &parent,
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, TRUE, 0)) goto finish;
    inherited[0] = mapping; inherited[1] = request; inherited[2] = done;
    inherited[3] = ready; inherited[4] = parent;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
    startup.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, bytes);
    if (!startup.lpAttributeList) goto finish;
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes)) {
        HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
        startup.lpAttributeList = NULL; goto finish;
    }
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            inherited, sizeof inherited, NULL, NULL)) goto finish;
    startup.StartupInfo.cb = sizeof startup;
    swprintf_s(command, _countof(command), L"\"%s\" --capture %llu %llu %llu %llu %llu", helper_path,
        (unsigned long long)(uintptr_t)mapping, (unsigned long long)(uintptr_t)request,
        (unsigned long long)(uintptr_t)done, (unsigned long long)(uintptr_t)ready,
        (unsigned long long)(uintptr_t)parent);
    if (!CreateProcessW(helper_path, command, NULL, NULL, TRUE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, NULL, NULL, &startup.StartupInfo, &process)) goto finish;
    CloseHandle(process.hThread);
    helper = process.hProcess;
    HANDLE waits[2] = {ready, helper};
    if (WaitForMultipleObjects(2, waits, FALSE, 5000) != WAIT_OBJECT_0) goto finish;
    guest_capture = capture;
    previous_filter = SetUnhandledExceptionFilter(fatal_exception);
    SetErrorMode(GetErrorMode() | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    success = 1;
finish:
    if (startup.lpAttributeList) {
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
    }
    if (parent) CloseHandle(parent);
    /* The helper needs inherited handles; future children do not. */
    if (mapping) SetHandleInformation(mapping, HANDLE_FLAG_INHERIT, 0);
    if (request) SetHandleInformation(request, HANDLE_FLAG_INHERIT, 0);
    if (done) SetHandleInformation(done, HANDLE_FLAG_INHERIT, 0);
    if (ready) SetHandleInformation(ready, HANDLE_FLAG_INHERIT, 0);
    if (!success) {
        /* Do not restore a filter that we never installed. */
        if (packet) { UnmapViewOfFile(packet); packet = NULL; }
        if (helper) { TerminateProcess(helper, 2); WaitForSingleObject(helper, 1000); }
        pc_crash_shutdown();
    }
    return success;
}

void pc_crash_watchdog_start(void)
{
    if (!packet) return;
    pc_crumb_add(&packet->breadcrumbs,GetTickCount64()-packet->started_ms,PC_CRUMB_GUEST_START,0);
    packet->watchdog_thread = GetCurrentThreadId();
    InterlockedExchange(&packet->watchdog_armed, 1);
}
void pc_crash_watchdog_stop(void)
{
    if (packet) { pc_crumb_add(&packet->breadcrumbs,GetTickCount64()-packet->started_ms,PC_CRUMB_GUEST_STOP,0); InterlockedExchange(&packet->watchdog_armed, 0); }
}
void pc_crash_frame(void)
{
    PcCrashPacket *p = packet;
    if (!p || !InterlockedCompareExchange(&p->watchdog_armed, 0, 0)) return;
    if (!InterlockedCompareExchange(&p->snapshot_writer, 1, 0)) {
        InterlockedIncrement(&p->snapshot_sequence);
        if (guest_capture) guest_capture(&p->frame_guest);
        p->watchdog_thread = GetCurrentThreadId();
        InterlockedIncrement(&p->snapshot_sequence);
        InterlockedExchange(&p->snapshot_writer, 0);
    }
    InterlockedIncrement64(&p->frame_sequence);
}
void pc_crash_window(HWND window)
{
    if (packet) InterlockedExchange64((volatile LONG64 *)&packet->game_window, (LONG64)(uintptr_t)window);
}
void pc_crash_inhibit(int entering)
{
    if (!packet) return;
    if (entering) InterlockedIncrement(&packet->watchdog_inhibited);
    else {
        LONG count;
        do { count = InterlockedCompareExchange(&packet->watchdog_inhibited, 0, 0); }
        while (count > 0 && InterlockedCompareExchange(&packet->watchdog_inhibited, count - 1, count) != count);
    }
    pc_crumb_add(&packet->breadcrumbs,GetTickCount64()-packet->started_ms,PC_CRUMB_MODAL,packet->watchdog_inhibited);
}

void pc_capture_gameplay(int32_t scene,uint32_t mode,uint32_t menu,int paused,int network,int cutscene)
{
    if(!packet)return;
    const int32_t values[PC_CRUMB_FIELDS]={scene,(int32_t)mode,(int32_t)menu,!!paused,network,!!cutscene};
    pc_crumb_game(&packet->breadcrumbs,GetTickCount64()-packet->started_ms,values);
}

static volatile LONG test_hanging;
__declspec(noinline) void tsfp_crash_test_site(void);
__declspec(noinline) void tsfp_hang_test_site(void)
{
    while (InterlockedCompareExchange(&test_hanging, 0, 0)) YieldProcessor();
}
static DWORD WINAPI release_test_hang(void *unused)
{
    (void)unused; Sleep(3500); InterlockedExchange(&test_hanging, 0); return 0;
}
static void test_progress_for(DWORD duration)
{
    ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < duration) { pc_crash_frame(); Sleep(100); }
}
static int hang_test_command(const char *mode)
{
    if (!packet) return 4;
    packet->hang_timeout_ms = 1000;
    packet->startup_timeout_ms = 4000;
    if (!strcmp(mode, "unarmed")) { Sleep(2500); return 0; }
    pc_crash_watchdog_start();
    if (!strcmp(mode, "startup")) { Sleep(2500); pc_crash_frame(); return 0; }
    pc_crash_frame();
    if (!strcmp(mode, "responsive")) { test_progress_for(2500); return 0; }
    if (!strcmp(mode, "inhibited")) {
        pc_crash_inhibit(1); Sleep(2500); pc_crash_inhibit(0); pc_crash_frame(); return 0;
    }
    if (!strcmp(mode, "transient")) { Sleep(400); pc_crash_frame(); return 0; }
    if (strcmp(mode, "hang") && strcmp(mode, "recover") && strcmp(mode, "twice") && strcmp(mode, "then-crash")) return 5;
    int passes = !strcmp(mode, "twice") ? 2 : 1;
    for (int i = 0; i < passes; ++i) {
        InterlockedExchange(&test_hanging, 1);
        HANDLE release = NULL;
        if (strcmp(mode, "hang")) release = CreateThread(NULL, 0, release_test_hang, NULL, 0, NULL);
        if (strcmp(mode, "hang") && !release) return 3;
        tsfp_hang_test_site();
        if (release) { WaitForSingleObject(release, 1000); CloseHandle(release); }
        test_progress_for(1500);
    }
    if (!strcmp(mode, "then-crash")) tsfp_crash_test_site();
    return 0;
}

__declspec(noinline) void tsfp_crash_test_site(void)
{
    volatile uintptr_t address = 1;
    *(volatile DWORD *)address = 0x54534650;
}
static DWORD WINAPI test_worker(void *unused)
{
    (void)unused;
    tsfp_crash_test_site();
    return 0;
}
static DWORD WINAPI test_concurrent_worker(void *start)
{
    WaitForSingleObject((HANDLE)start, 5000);
    tsfp_crash_test_site();
    return 0;
}
int pc_crash_test_command(const char *command)
{
    if (!strncmp(command, "--tsfp-hang-test=", 17)) return hang_test_command(command + 17);
    if (!packet && !strncmp(command, "--tsfp-crash-test=", 18)) return 4;
    if (!strcmp(command, "--tsfp-crash-test=main")) tsfp_crash_test_site();
    else if (!strcmp(command, "--tsfp-crash-test=concurrent")) {
        HANDLE start = CreateEventW(NULL, TRUE, FALSE, NULL), threads[4] = {0};
        if (!start) return 3;
        for (int i = 0; i < 4; ++i) {
            threads[i] = CreateThread(NULL, 0, test_concurrent_worker, start, 0, NULL);
            if (!threads[i]) return 3; /* Diagnostic process exits immediately. */
        }
        SetEvent(start);
        WaitForMultipleObjects(4, threads, TRUE, 20000);
        for (int i = 0; i < 4; ++i) CloseHandle(threads[i]);
        CloseHandle(start);
    }
    else if (!strcmp(command, "--tsfp-crash-test=worker")) {
        HANDLE thread = CreateThread(NULL, 0, test_worker, NULL, 0, NULL);
        if (!thread) return 3;
        WaitForSingleObject(thread, 20000); CloseHandle(thread);
    } else if (!strcmp(command, "--tsfp-crash-test=handled")) {
        __try { tsfp_crash_test_site(); }
        __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
    } else if (strcmp(command, "--tsfp-crash-test=normal")) return -1;
    return 0;
}
