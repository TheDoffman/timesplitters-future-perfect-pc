#define _WIN32_WINNT 0x0601
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_crash.h"
#include "pc_hang_policy.h"
#include "pc_capture_health.h"
#include <io.h>
#include <fcntl.h>
#include <dbghelp.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include "pc_refresh_guard.h"

/* One helper per game. DbgHelp is used only on this process's single thread. */
static int directory(const WCHAR *path)
{
    WCHAR full[PC_CRASH_PATH];
    DWORD count = GetFullPathNameW(path, _countof(full), full, NULL);
    if (!count || count >= _countof(full) || full[1] != L':' || full[2] != L'\\') return 0;
    for (WCHAR *p = full + 3; ; ++p) {
        if (*p && *p != L'\\') continue;
        WCHAR saved = *p; *p = 0;
        DWORD attributes = GetFileAttributesW(full);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectoryW(full, NULL)) return 0;
            attributes = GetFileAttributesW(full);
        }
        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
        *p = saved; if (!saved) break;
    }
    return 1;
}
static void join(WCHAR *out, const WCHAR *root, const WCHAR *name)
{ swprintf_s(out, PC_CRASH_PATH, L"%s\\%s", root, name); }
/* A current-session heartbeat confirms helper startup and an actual writable
 * report directory. Atomic replacement prevents readers seeing partial JSON. */
static void health(PcCrashPacket *p)
{
    char session[33];
    if(!pc_capture_session(session) || !directory(p->root))return;
    WCHAR path[PC_CRASH_PATH],temporary[PC_CRASH_PATH],name[80];
    join(path,p->root,L"capture-health.json");
    DWORD attr=GetFileAttributesW(path);
    if(attr!=INVALID_FILE_ATTRIBUTES && (attr&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))return;
    swprintf_s(name,_countof(name),L"health-%lu-%llu.tmp",GetCurrentProcessId(),GetTickCount64());
    join(temporary,p->root,name);
    HANDLE file=CreateFileW(temporary,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return;
    int fd=_open_osfhandle((intptr_t)file,_O_BINARY);
    if(fd<0){CloseHandle(file);DeleteFileW(temporary);return;}
    FILE *out=_fdopen(fd,"wb");
    if(!out){_close(fd);DeleteFileW(temporary);return;}
    pc_capture_health_json(out,session,p->process_id,GetCurrentProcessId(),InterlockedCompareExchange(&p->watchdog_armed,0,0));
    int ok=fflush(out)==0 && !ferror(out) && FlushFileBuffers(file);
    if(fclose(out))ok=0;
    if(ok)MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    DeleteFileW(temporary);
}
static int incident_name(const WCHAR *name)
{
    if (wcslen(name) != 42 || wcsncmp(name, L"crash-", 6)) return 0;
    for (int i = 6; i < 42; ++i)
        if (name[i] != L'-' && (name[i] < L'0' || name[i] > L'9')) return 0;
    return 1;
}
static int compare_names(const void *a, const void *b) { return wcscmp(a, b); }
static void rotate(const WCHAR *root)
{
    /* Bounded inventory; never traverse links or remove unknown files. */
    WCHAR pattern[PC_CRASH_PATH], names[128][43];
    WIN32_FIND_DATAW data;
    join(pattern, root, L"crash-*");
    HANDLE search = FindFirstFileW(pattern, &data);
    unsigned count = 0;
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !(data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && incident_name(data.cFileName) && count < 128)
            wcscpy_s(names[count++], 43, data.cFileName);
    } while (FindNextFileW(search, &data));
    FindClose(search);
    qsort(names, count, sizeof names[0], compare_names);
    for (unsigned i = 0; i + 4 < count; ++i) {
        WCHAR folder[PC_CRASH_PATH], file[PC_CRASH_PATH];
        join(folder, root, names[i]); join(file, folder, L"capture.lock");
        HANDLE lock = CreateFileW(file, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
        if (lock == INVALID_HANDLE_VALUE) continue;
        CloseHandle(lock);
        const WCHAR *files[] = {L"crash.dmp", L"report.json", L"report.txt", L"game-log-tail.txt", L"breadcrumbs.txt", L"capture.lock"};
        for (unsigned n = 0; n < _countof(files); ++n) {
            join(file, folder, files[n]);
            DWORD attr = GetFileAttributesW(file);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
                DeleteFileW(file);
        }
        RemoveDirectoryW(folder);
    }
}
typedef struct DumpWriter { HANDLE file; ULONGLONG started; HANDLE priority_request; } DumpWriter;
static BOOL CALLBACK dump_callback(void *opaque, const MINIDUMP_CALLBACK_INPUT *in, MINIDUMP_CALLBACK_OUTPUT *out)
{
    DumpWriter *writer = opaque;
    switch (in->CallbackType) {
    case IoStartCallback: out->Status = S_FALSE; break;
    case IoWriteAllCallback: {
        LARGE_INTEGER offset;
        DWORD written;
        if (in->Io.Offset > PC_CRASH_DUMP_LIMIT ||
            in->Io.BufferBytes > PC_CRASH_DUMP_LIMIT - in->Io.Offset ||
            GetTickCount64() - writer->started > 8000 ||
            (writer->priority_request && WaitForSingleObject(writer->priority_request, 0) == WAIT_OBJECT_0)) {
            out->Status = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE); return FALSE;
        }
        offset.QuadPart = in->Io.Offset;
        if (!SetFilePointerEx(writer->file, offset, NULL, FILE_BEGIN) ||
            !WriteFile(writer->file, in->Io.Buffer, in->Io.BufferBytes, &written, NULL) || written != in->Io.BufferBytes) {
            out->Status = E_FAIL; return FALSE;
        }
        out->Status = S_OK; break;
    }
    case IoFinishCallback: out->Status = S_OK; break;
    case CancelCallback:
        out->CheckCancel = TRUE;
        out->Cancel = GetTickCount64() - writer->started > 8000;
        if (writer->priority_request && WaitForSingleObject(writer->priority_request, 0) == WAIT_OBJECT_0) out->Cancel = TRUE;
        break;
    default: break;
    }
    return TRUE;
}
static void log_tail(const WCHAR *source, const WCHAR *folder)
{
    if (!source[0]) return;
    HANDLE input = CreateFileW(source, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (input == INVALID_HANDLE_VALUE) return;
    BY_HANDLE_FILE_INFORMATION info;
    LARGE_INTEGER length, offset;
    if (!GetFileInformationByHandle(input, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || !GetFileSizeEx(input, &length)) {
        CloseHandle(input); return;
    }
    offset.QuadPart = length.QuadPart > 65536 ? length.QuadPart - 65536 : 0;
    char buffer[65536]; DWORD read = 0;
    if (SetFilePointerEx(input, offset, NULL, FILE_BEGIN) && ReadFile(input, buffer, sizeof buffer, &read, NULL)) {
        WCHAR path[PC_CRASH_PATH]; join(path, folder, L"game-log-tail.txt");
        HANDLE output = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (output != INVALID_HANDLE_VALUE) { DWORD written; WriteFile(output, buffer, read, &written, NULL); CloseHandle(output); }
    }
    CloseHandle(input);
}
static int file_hash(const WCHAR *path, char *hex)
{
    BCRYPT_ALG_HANDLE algorithm = NULL; BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char digest[32], buffer[65536]; DWORD count; int ok = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0 ||
        BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) < 0) goto finish;
    while (1) {
        if (!ReadFile(file, buffer, sizeof buffer, &count, NULL)) goto finish;
        if (!count) break;
        if (BCryptHashData(hash, buffer, count, 0) < 0) goto finish;
    }
    if (BCryptFinishHash(hash, digest, sizeof digest, 0) < 0) goto finish;
    for (int i = 0; i < 32; ++i) sprintf_s(hex + i * 2, 3, "%02x", digest[i]);
    ok = 1;
finish:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file); return ok;
}
static int capture(HANDLE process, PcCrashPacket *p, int hang, ULONGLONG stalled_ms, HANDLE crash_request, PcBreadcrumbs *breadcrumbs)
{
    WCHAR folder[PC_CRASH_PATH], name[64], path[PC_CRASH_PATH];
    SYSTEMTIME now; GetSystemTime(&now);
    if (!directory(p->root)) return 2;
    rotate(p->root);
    swprintf_s(name, _countof(name), L"crash-%04u%02u%02u-%02u%02u%02u%03u-%010lu-%06lu",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
        p->process_id, GetCurrentProcessId() % 1000000);
    join(folder, p->root, name);
    if (!CreateDirectoryW(folder, NULL)) return 3;
    join(path, folder, L"capture.lock");
    HANDLE lock = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (lock == INVALID_HANDLE_VALUE) return 4;
    join(path, folder, L"crash.dmp");
    HANDLE dump = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL saved = FALSE; DWORD error = GetLastError();
    if (dump != INVALID_HANDLE_VALUE) {
        EXCEPTION_POINTERS ep = { &p->exception, &p->context };
        MINIDUMP_EXCEPTION_INFORMATION exception = {p->thread_id, &ep, FALSE};
        DumpWriter writer = {dump, GetTickCount64(), hang ? crash_request : NULL};
        MINIDUMP_CALLBACK_INFORMATION callback = {dump_callback, &writer};
        MINIDUMP_USER_STREAM stream = {LastReservedStream + 1, sizeof p->guest, &p->guest};
        MINIDUMP_USER_STREAM_INFORMATION streams = {1, &stream};
        saved = MiniDumpWriteDump(process, p->process_id, dump,
            MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules,
            hang ? NULL : &exception, &streams, &callback);
        error = saved ? 0 : GetLastError();
        FlushFileBuffers(dump); CloseHandle(dump);
        if (!saved) DeleteFileW(path);
    }
    log_tail(p->log, folder);
    join(path,folder,L"breadcrumbs.txt");
    FILE *crumbs=NULL;_wfopen_s(&crumbs,path,L"wb");
    if(crumbs){pc_crumb_report(crumbs,breadcrumbs);fclose(crumbs);}
    char sha[65] = "unavailable";
    file_hash(p->executable, sha);
    join(path, folder, L"report.json");
    FILE *output = NULL; _wfopen_s(&output, path, L"wb");
    if (output) {
        char exception_code[24] = "null", host_rip[24] = "null", host_rsp[24] = "null";
        if (!hang) {
            sprintf_s(exception_code, sizeof exception_code, "\"0x%08lX\"", p->exception.ExceptionCode);
            sprintf_s(host_rip, sizeof host_rip, "\"0x%llX\"", p->context.Rip);
            sprintf_s(host_rsp, sizeof host_rsp, "\"0x%llX\"", p->context.Rsp);
        }
        fprintf(output, "{\n  \"schema\": 1,\n  \"kind\": \"%s\",\n"
            "  \"pid\": %lu,\n  \"thread_id\": %lu,\n  \"exception_code\": %s,\n"
            "  \"host_rip\": %s,\n  \"host_rsp\": %s,\n"
            "  \"stalled_ms\": %llu,\n  \"frame_sequence\": %llu,\n  \"guest_is_cached\": %s,\n"
            "  \"dump_saved\": %s,\n  \"dump_error\": %lu,\n  \"executable_sha256\": \"%s\",\n"
            "  \"guest\": {\"valid\": %s, \"eax\": %u, \"ebx\": %u, \"ecx\": %u, \"edx\": %u, "
            "\"esi\": %u, \"edi\": %u, \"esp\": %u, \"ebp\": %u, \"memory_base\": \"0x%llX\"}\n}\n",
            hang ? "suspected_hang" : "unhandled_exception", p->process_id, p->thread_id, exception_code, host_rip, host_rsp,
            stalled_ms, (unsigned long long)p->frame_sequence, hang ? "true" : "false",
            saved ? "true" : "false", error, sha, p->guest.valid ? "true" : "false",
            p->guest.eax, p->guest.ebx, p->guest.ecx, p->guest.edx, p->guest.esi, p->guest.edi,
            p->guest.esp, p->guest.ebp, p->guest.memory_base);
        fclose(output);
    }
    join(path, folder, L"report.txt"); _wfopen_s(&output, path, L"wb");
    if (output) {
        if (hang) {
            fprintf(output, "TimeSplitters suspected hang report\n"
                "No completed game frame for %llu ms. The game was left running.\n"
                "Observed game thread: %lu; completed frame sequence: %llu\n"
                "Minidump: %s (Windows error %lu)\nExecutable SHA-256: %s\n"
                "The dump contains live native thread stacks, without a crash exception.\n"
                "Xbox registers are a cached snapshot from the last completed frame.\n"
                "A long load or external debugger can also look like a stall; recovery is possible.\n"
                "Nothing was uploaded. Review the dump and log before sharing.\n",
                stalled_ms, p->thread_id, (unsigned long long)p->frame_sequence,
                saved ? "saved" : "FAILED", error, sha);
        } else {
        fprintf(output, "TimeSplitters Windows crash report (UTC %04u-%02u-%02u %02u:%02u:%02u)\n"
            "Unhandled exception 0x%08lX, thread %lu, native RIP 0x%llX\n"
            "Minidump: %s (Windows error %lu)\nExecutable SHA-256: %s\n"
            "Xbox register snapshot: %s\n"
            "Open crash.dmp with this executable's matching PDB.\n"
            "The dump and log excerpt may contain personal information. Nothing was uploaded.\n",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
            p->exception.ExceptionCode, p->thread_id, p->context.Rip,
            saved ? "saved" : "FAILED", error, sha, p->guest.valid ? "available" : "unavailable (pre-game test)");
        }
        fclose(output);
    }
    CloseHandle(lock);
    return saved ? 0 : 5;
}

/* Offline verification: resolve the exception address from the dump itself,
 * require exact symbols and compare the PDB GUID/age to the dump's RSDS record. */
static int resolve(const WCHAR *dump_path, const WCHAR *image, const WCHAR *symbols, const WCHAR *expected, DWORD hang_thread)
{
    int result = 10; HANDLE file, mapping = NULL; void *view = NULL;
    HANDLE session = (HANDLE)(uintptr_t)0x54534650;
    file = CreateFileW(dump_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 10;
    LARGE_INTEGER length;
    if (!GetFileSizeEx(file, &length) || length.QuadPart < sizeof(MINIDUMP_HEADER) || length.QuadPart > PC_CRASH_DUMP_LIMIT) goto finish;
    mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mapping || !(view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0))) goto finish;
    MINIDUMP_DIRECTORY *entry; ULONG size;
    MINIDUMP_EXCEPTION_STREAM *exception = NULL; MINIDUMP_MODULE_LIST *modules = NULL;
    DWORD64 address = 0; DWORD thread_id = hang_thread;
    if (hang_thread) {
        MINIDUMP_THREAD_LIST *threads = NULL;
        if (!MiniDumpReadDumpStream(view, ThreadListStream, &entry, (void **)&threads, &size) || size < sizeof(ULONG) ||
            threads->NumberOfThreads > (size - sizeof(ULONG)) / sizeof(MINIDUMP_THREAD)) goto finish;
        for (ULONG i = 0; i < threads->NumberOfThreads; ++i) {
            MINIDUMP_THREAD *thread = &threads->Threads[i];
            if (thread->ThreadId != hang_thread) continue;
            if (thread->ThreadContext.DataSize < sizeof(CONTEXT) ||
                (ULONGLONG)thread->ThreadContext.Rva + thread->ThreadContext.DataSize > (ULONGLONG)length.QuadPart) goto finish;
            CONTEXT context;
            memcpy(&context, (unsigned char *)view + thread->ThreadContext.Rva, sizeof context);
            address = context.Rip;
        }
        if (!address) goto finish;
        if (MiniDumpReadDumpStream(view, ExceptionStream, &entry, (void **)&exception, &size)) goto finish;
    } else {
        if (!MiniDumpReadDumpStream(view, ExceptionStream, &entry, (void **)&exception, &size) || size < sizeof *exception) goto finish;
        address = exception->ExceptionRecord.ExceptionAddress; thread_id = exception->ThreadId;
    }
    if (!MiniDumpReadDumpStream(view, ModuleListStream, &entry, (void **)&modules, &size) || size < sizeof(ULONG) ||
        modules->NumberOfModules > (size - sizeof(ULONG)) / sizeof(MINIDUMP_MODULE)) goto finish;
    MINIDUMP_MODULE *module = NULL;
    for (ULONG i = 0; i < modules->NumberOfModules; ++i)
        if (address >= modules->Modules[i].BaseOfImage && address - modules->Modules[i].BaseOfImage < modules->Modules[i].SizeOfImage)
            module = &modules->Modules[i];
    if (!module || module->CvRecord.DataSize < 24 ||
        (ULONGLONG)module->CvRecord.Rva + module->CvRecord.DataSize > (ULONGLONG)length.QuadPart) goto finish;
    const unsigned char *cv = (unsigned char *)view + module->CvRecord.Rva;
    if (memcmp(cv, "RSDS", 4)) goto finish;
    SymSetOptions(SYMOPT_EXACT_SYMBOLS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_IGNORE_NT_SYMPATH | SYMOPT_LOAD_LINES);
    if (!SymInitializeW(session, symbols, FALSE)) goto finish;
    result = 11;
    if (!SymLoadModuleExW(session, NULL, image, NULL, module->BaseOfImage, module->SizeOfImage, NULL, 0)) goto cleanup;
    unsigned char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)storage; ZeroMemory(storage, sizeof storage);
    symbol->SizeOfStruct = sizeof *symbol; symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement;
    if (!SymFromAddr(session, address, &displacement, symbol)) goto cleanup;
    IMAGEHLP_MODULEW64 info = {0}; info.SizeOfStruct = sizeof info;
    if (!SymGetModuleInfoW64(session, module->BaseOfImage, &info) || info.SymType != SymPdb || info.PdbUnmatched ||
        memcmp(&info.PdbSig70, cv + 4, sizeof(GUID)) || memcmp(&info.PdbAge, cv + 20, sizeof(DWORD))) goto cleanup;
    char wanted[MAX_SYM_NAME];
    if (!WideCharToMultiByte(CP_UTF8, 0, expected, -1, wanted, sizeof wanted, NULL, NULL)) goto cleanup;
    if (hang_thread) printf("dump kind=suspected_hang thread=%lu RIP=0x%llX (no exception stream)\n", thread_id, address);
    else printf("dump exception=0x%08lX thread=%lu RIP=0x%llX\n", exception->ExceptionRecord.ExceptionCode, thread_id, address);
    printf("resolved=%s+0x%llX\nPDB GUID/age matches dump: yes\n", symbol->Name, displacement);
    result = strcmp(wanted, symbol->Name) ? 12 : 0;
cleanup:
    SymCleanup(session);
finish:
    if (view) UnmapViewOfFile(view);
    if (mapping) CloseHandle(mapping);
    CloseHandle(file);
    if (result) fprintf(stderr, "Dump resolution failed (%d): missing/mismatched symbols or unexpected crash function.\n", result);
    return result;
}
int wmain(int argc, WCHAR **argv)
{
    if (argc > 1 && !wcscmp(argv[1], L"--restore-refresh")) return pc_refresh_guard(argc,argv);
    if (argc == 6 && !wcscmp(argv[1], L"--resolve")) return resolve(argv[2], argv[3], argv[4], argv[5], 0);
    if (argc == 7 && !wcscmp(argv[1], L"--resolve-hang")) {
        WCHAR *end; unsigned long id = wcstoul(argv[5], &end, 10);
        if (*end || !id) return 1;
        return resolve(argv[2], argv[3], argv[4], argv[6], id);
    }
    if (argc != 7 || wcscmp(argv[1], L"--capture")) return 1;
    HANDLE handles[5];
    for (int i = 0; i < 5; ++i) {
        WCHAR *end; handles[i] = (HANDLE)(uintptr_t)wcstoull(argv[i + 2], &end, 10);
        if (*end || !handles[i]) return 1;
    }
    PcCrashPacket *p = MapViewOfFile(handles[0], FILE_MAP_ALL_ACCESS, 0, 0, sizeof *p);
    if (!p || p->schema != PC_CRASH_SCHEMA) return 1;
    health(p);
    ULONGLONG last_health=GetTickCount64();
    SetEvent(handles[3]);
    HANDLE waits[2] = {handles[1], handles[4]};
    int result = 0;
    PcHangPolicy policy = {0};
    for (;;) {
        DWORD waited = WaitForMultipleObjects(2, waits, FALSE, 250);
        if (waited == WAIT_OBJECT_0) {
            MemoryBarrier(); result = capture(handles[4], p, 0, 0, NULL, &p->breadcrumbs);
            SetEvent(handles[2]); break;
        }
        if (waited != WAIT_TIMEOUT) break;
        ULONGLONG now = GetTickCount64();
        if(now-last_health>=2000){health(p);last_health=now;}
        LONG64 frames = InterlockedCompareExchange64(&p->frame_sequence, 0, 0);
        int suppressed = InterlockedCompareExchange(&p->watchdog_inhibited, 0, 0) > 0;
        HWND window = (HWND)(uintptr_t)InterlockedCompareExchange64((volatile LONG64 *)&p->game_window, 0, 0);
        if (window) {
            DWORD foreground_pid = 0;
            GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
            GUITHREADINFO gui = {sizeof gui};
            GetGUIThreadInfo(GetWindowThreadProcessId(window, NULL), &gui);
            suppressed |= !IsWindow(window) || IsIconic(window) || foreground_pid != p->process_id ||
                (gui.flags & (GUI_INMENUMODE | GUI_INMOVESIZE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE)) != 0;
        }
        if (!pc_hang_poll(&policy, now, frames, InterlockedCompareExchange(&p->watchdog_armed, 0, 0),
                suppressed, p->hang_timeout_ms, p->startup_timeout_ms)) continue;
        if (InterlockedCompareExchange(&p->entered, 0, 0) ||
            frames != InterlockedCompareExchange64(&p->frame_sequence, 0, 0)) continue;
        /* Never overwrite fatal exception memory with an asynchronous snapshot. */
        PcCrashPacket report = {0};
        report.schema = p->schema; report.process_id = p->process_id; report.frame_sequence = frames;
        memcpy(report.root, p->root, sizeof report.root);
        memcpy(report.executable, p->executable, sizeof report.executable);
        memcpy(report.log, p->log, sizeof report.log);
        report.thread_id = p->watchdog_thread;
        for (int i = 0; i < 3; ++i) {
            LONG sequence = InterlockedCompareExchange(&p->snapshot_sequence, 0, 0);
            if (sequence & 1) continue;
            DWORD thread_id = p->watchdog_thread;
            PcCrashGuest guest = p->frame_guest;
            MemoryBarrier();
            if (sequence == InterlockedCompareExchange(&p->snapshot_sequence, 0, 0)) { report.guest = guest; report.thread_id = thread_id; break; }
        }
        capture(handles[4], &report, 1, now - policy.last_progress, handles[1], &p->breadcrumbs);
    }
    UnmapViewOfFile(p);
    for (int i = 0; i < 5; ++i) CloseHandle(handles[i]);
    return result;
}
