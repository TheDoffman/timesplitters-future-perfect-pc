/*
 * exit_trace.c -- say who ended the process.
 *
 * A title that faults prints [CRASH] or [EXCEPTION] from the vectored
 * handler, a title that asks the kernel to quit prints [KERNEL]
 * HalReturnToFirmware, and a title killed from outside leaves the killer's
 * exit code. TimeSplitters 2, driven through its menus, exited with
 * 0xFFFFFFFF about one run in two on 22 Sep 2026 with none of those: no
 * fault, no kernel call, no line of any kind after the last heap free.
 * Nothing in this runtime calls exit or ExitProcess with -1, so the call
 * came from somewhere that does not log -- a C runtime path, a host library,
 * a thread ending with that status as the last one alive.
 *
 * This hooks the process-ending imports -- ExitProcess, TerminateProcess on
 * the current process, ExitThread with status -1 or on the main thread -- in
 * every loaded module's import table, and prints the code, the host stack
 * with symbols, and the guest return addresses on the current guest stack
 * before letting the call through. Import-table patching rather than an
 * inline hook, so the real function runs unchanged afterwards and shutdown
 * semantics are untouched. What it cannot see: a call resolved at run time
 * with GetProcAddress, or one made from inside kernelbase itself.
 *
 * RECOMP_EXIT_TRACE=0 turns it off.
 */

#ifdef _WIN32

#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "xbox_memory_layout.h"     /* RECOMP_TLS */

extern RECOMP_TLS uint32_t g_esp;   /* the calling thread's guest stack */
extern ptrdiff_t g_xbox_mem_offset;

typedef VOID (WINAPI *ExitProcess_t)(UINT);
typedef BOOL (WINAPI *TerminateProcess_t)(HANDLE, UINT);
typedef VOID (WINAPI *ExitThread_t)(DWORD);

static ExitProcess_t      real_ExitProcess;
static TerminateProcess_t real_TerminateProcess;
static ExitThread_t       real_ExitThread;
static DWORD              g_main_tid;
static volatile LONG      g_reporting;

static void module_name_of(void *addr, char *out, size_t cap, uintptr_t *base)
{
    HMODULE m = NULL;
    out[0] = 0;
    *base = 0;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &m) && m) {
        char path[MAX_PATH];
        DWORD n = GetModuleFileNameA(m, path, sizeof path);
        const char *slash;
        *base = (uintptr_t)m;
        if (n) {
            slash = strrchr(path, '\\');
            strncpy(out, slash ? slash + 1 : path, cap - 1);
            out[cap - 1] = 0;
        }
    }
}

static void report(const char *what, unsigned code)
{
    void *frames[48];
    USHORT n, i;
    DWORD tid = GetCurrentThreadId();

    static LONG reports;

    /* One at a time, and six in all: an exit goes ExitProcess ->
     * RtlExitUserProcess -> NtTerminateProcess and every layer is hooked, so
     * the first report is the one that names the caller and the rest repeat
     * it from lower down. Six leaves room for a worker or two ending with -1
     * before the exit that matters. */
    if (InterlockedCompareExchange(&g_reporting, 1, 0) != 0)
        return;
    if (InterlockedIncrement(&reports) > 6) {
        InterlockedExchange(&g_reporting, 0);
        return;
    }

    fprintf(stderr, "[EXIT] %s(0x%08X) on host thread %lu%s; host stack:\n",
            what, code, (unsigned long)tid, tid == g_main_tid ? " (main)" : "");
    n = CaptureStackBackTrace(1, 48, frames, NULL);
    for (i = 0; i < n; i++) {
        char mod[64];
        uintptr_t base;
        char buf[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO *sym = (SYMBOL_INFO *)buf;
        DWORD64 disp = 0;

        module_name_of(frames[i], mod, sizeof mod, &base);
        memset(buf, 0, sizeof buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        if (SymFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)frames[i], &disp, sym))
            fprintf(stderr, "    %-20s %s+0x%llx\n", mod[0] ? mod : "?", sym->Name,
                    (unsigned long long)disp);
        else
            fprintf(stderr, "    %-20s +0x%llx\n", mod[0] ? mod : "?",
                    (unsigned long long)((uintptr_t)frames[i] - base));
    }
    /* The guest side: whatever on the current guest stack looks like a
     * return address into the image. g_esp is per thread, so this is the
     * calling thread's guest stack, if it has one. */
    {
        uint32_t esp = g_esp;
        int shown = 0;
        fprintf(stderr, "  guest esp 0x%08X; candidates:", esp);
        if (esp >= 0x10000u && esp < 0x08000000u) {
            uint32_t a;
            for (a = esp; a < esp + 0x400u && shown < 12; a += 4) {
                uint32_t v = *(volatile uint32_t *)((uintptr_t)a + g_xbox_mem_offset);
                if (v >= 0x00010000u && v < 0x01000000u) {
                    fprintf(stderr, " 0x%08X", v);
                    shown++;
                }
            }
        }
        fprintf(stderr, "%s\n", shown ? "" : " none");
    }
    fflush(stderr);
    InterlockedExchange(&g_reporting, 0);
}

static VOID WINAPI hook_ExitProcess(UINT code)
{
    report("ExitProcess", code);
    real_ExitProcess(code);
}

static BOOL WINAPI hook_TerminateProcess(HANDLE h, UINT code)
{
    if (h == GetCurrentProcess() || GetProcessId(h) == GetCurrentProcessId())
        report("TerminateProcess", code);
    return real_TerminateProcess(h, code);
}

static VOID WINAPI hook_ExitThread(DWORD code)
{
    if (code == 0xFFFFFFFFu || GetCurrentThreadId() == g_main_tid)
        report("ExitThread", code);
    real_ExitThread(code);
}

/* The ntdll funnel underneath: kernelbase's ExitProcess ends in
 * RtlExitUserProcess, its TerminateProcess in NtTerminateProcess, and a
 * thread routine that *returns* ends in RtlExitUserThread from
 * BaseThreadInitThunk without any ExitThread call. Patching kernelbase's
 * and kernel32's imports of these catches a module loaded after this ran
 * (d3d11, the GPU driver, the shader compiler, XAudio2), which the first
 * pass could not see, and the last-thread-returned case, whose status
 * becomes the process exit code with nothing else logged. TimeSplitters 2
 * exited 0xFFFFFFFF 4.7 s into a run with none of the three hooks above
 * firing, which is what sent this layer down a level. */
typedef VOID     (NTAPI *RtlExitUserProcess_t)(NTSTATUS);
typedef NTSTATUS (NTAPI *NtTerminateProcess_t)(HANDLE, NTSTATUS);
typedef VOID     (NTAPI *RtlExitUserThread_t)(NTSTATUS);
static RtlExitUserProcess_t real_RtlExitUserProcess;
static NtTerminateProcess_t real_NtTerminateProcess;
static RtlExitUserThread_t  real_RtlExitUserThread;

static VOID NTAPI hook_RtlExitUserProcess(NTSTATUS status)
{
    report("RtlExitUserProcess", (unsigned)status);
    real_RtlExitUserProcess(status);
}

static NTSTATUS NTAPI hook_NtTerminateProcess(HANDLE h, NTSTATUS status)
{
    /* NULL means "every other thread of this process", the first half of a
     * process exit; the current process handle is the second half. */
    if (!h || h == GetCurrentProcess() || GetProcessId(h) == GetCurrentProcessId())
        report("NtTerminateProcess", (unsigned)status);
    return real_NtTerminateProcess(h, status);
}

static VOID NTAPI hook_RtlExitUserThread(NTSTATUS status)
{
    if (status == (NTSTATUS)0xFFFFFFFF || GetCurrentThreadId() == g_main_tid)
        report("RtlExitUserThread", (unsigned)status);
    real_RtlExitUserThread(status);
}

/* Replace every import-table entry in `module` that points at `from` with
 * `to`. Returns how many. */
static int patch_imports(HMODULE module, const void *from, const void *to)
{
    uint8_t *base = (uint8_t *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    int patched = 0;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress || !dir->Size)
        return 0;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress);
         imp->Name; imp++) {
        IMAGE_THUNK_DATA *thunk = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        for (; thunk->u1.Function; thunk++) {
            if ((const void *)thunk->u1.Function == from) {
                DWORD old;
                if (VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function),
                                   PAGE_READWRITE, &old)) {
                    thunk->u1.Function = (ULONG_PTR)to;
                    VirtualProtect(&thunk->u1.Function, sizeof(thunk->u1.Function),
                                   old, &old);
                    patched++;
                }
            }
        }
    }
    return patched;
}

void recomp_exit_trace_init(void)
{
    const char *env = getenv("RECOMP_EXIT_TRACE");
    HMODULE mods[512];
    DWORD needed = 0, count, i;
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE kb  = GetModuleHandleA("kernelbase.dll");
    struct { const char *name; void **real; void *hook; } hooks[] = {
        { "ExitProcess",      (void **)&real_ExitProcess,      (void *)hook_ExitProcess },
        { "TerminateProcess", (void **)&real_TerminateProcess, (void *)hook_TerminateProcess },
        { "ExitThread",       (void **)&real_ExitThread,       (void *)hook_ExitThread },
    };
    int total = 0;
    size_t h;

    if (env && atoi(env) == 0)
        return;
    g_main_tid = GetCurrentThreadId();
    if (!k32 || !EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &needed))
        return;
    count = needed / sizeof(HMODULE);
    if (count > sizeof mods / sizeof mods[0])
        count = sizeof mods / sizeof mods[0];

    for (h = 0; h < sizeof hooks / sizeof hooks[0]; h++) {
        void *from32 = (void *)GetProcAddress(k32, hooks[h].name);
        void *frombase = kb ? (void *)GetProcAddress(kb, hooks[h].name) : NULL;

        if (!from32)
            continue;
        /* Call the kernelbase one when there is one: kernel32's export is a
         * forwarder to it on current Windows, and a module may import either. */
        *hooks[h].real = frombase ? frombase : from32;
        for (i = 0; i < count; i++) {
            total += patch_imports(mods[i], from32, hooks[h].hook);
            if (frombase && frombase != from32)
                total += patch_imports(mods[i], frombase, hooks[h].hook);
        }
    }
    {
        HMODULE nt = GetModuleHandleA("ntdll.dll");
        struct { const char *name; void **real; void *hook; } low[] = {
            { "RtlExitUserProcess", (void **)&real_RtlExitUserProcess, (void *)hook_RtlExitUserProcess },
            { "NtTerminateProcess", (void **)&real_NtTerminateProcess, (void *)hook_NtTerminateProcess },
            { "RtlExitUserThread",  (void **)&real_RtlExitUserThread,  (void *)hook_RtlExitUserThread },
        };
        for (h = 0; nt && h < sizeof low / sizeof low[0]; h++) {
            void *from = (void *)GetProcAddress(nt, low[h].name);
            if (!from)
                continue;
            *low[h].real = from;
            for (i = 0; i < count; i++)
                total += patch_imports(mods[i], from, low[h].hook);
        }
    }
    fprintf(stderr, "  [EXIT] tracing process exit: %d import(s) hooked across %lu "
                    "module(s) (RECOMP_EXIT_TRACE=0 to stop)\n", total, (unsigned long)count);
}

#else

void recomp_exit_trace_init(void) {}

#endif
