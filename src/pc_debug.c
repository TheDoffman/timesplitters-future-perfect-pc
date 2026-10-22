#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "pc_debug.h"
#include "pc_debug_output.h"
#include "pc_native_rate.h"
#ifdef __APPLE__
#include "platform/win32_compat.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#else
#include <shellapi.h>
#include "pc_crash.h"
#endif
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>
#include <math.h>
#include "pc_network_build.h"
#ifndef TSFP_PC_VERSION
#define TSFP_PC_VERSION "development"
#endif

static SRWLOCK debug_lock = SRWLOCK_INIT;
static PcDebugOptions options;
static PcDebugSnapshot stats;
static char history[12][384];
static unsigned history_count, history_next;
static LARGE_INTEGER frequency, previous_frame;
static HWND debug_window;
#ifdef __APPLE__
static FILE *log_file;
static char log_path[4096];
static dev_t log_device;static ino_t log_inode;
#else
static HANDLE log_file = INVALID_HANDLE_VALUE;
static WCHAR log_path[1024];
#endif
static char partial[1024];
static size_t partial_size;
static ULONGLONG latency_updated;

const char *pc_debug_watermark(void)
{
    static const char identity[] = TSFP_NETWORK_COMPATIBILITY_HEX;
    static char text[96];
    /* Called on the presentation thread; fixed after the first frame. */
    if(!text[0]) snprintf(text,sizeof text,"TSFP PC v%s  %.8s",TSFP_PC_VERSION,identity);
    return text;
}

void pc_debug_latency(int mode, double milliseconds, unsigned peers)
{
    AcquireSRWLockExclusive(&debug_lock);
    stats.network_mode=mode;
    stats.latency_ms=isfinite(milliseconds) && milliseconds>=0 ? milliseconds : -1;
    stats.latency_peers=peers;
    latency_updated=GetTickCount64();
    ReleaseSRWLockExclusive(&debug_lock);
}

void pc_debug_get(PcDebugOptions *out)
{
    AcquireSRWLockShared(&debug_lock); *out = options; ReleaseSRWLockShared(&debug_lock);
}

void pc_debug_set(const PcDebugOptions *value)
{
    AcquireSRWLockExclusive(&debug_lock); options = *value; ReleaseSRWLockExclusive(&debug_lock);
}

void pc_debug_toggle_fps(void)
{
    AcquireSRWLockExclusive(&debug_lock); options.show_fps = !options.show_fps; ReleaseSRWLockExclusive(&debug_lock);
}

void pc_debug_init(HWND window)
{
    PcDebugOptions value;
#ifndef __APPLE__
    DWORD length;
#endif
    pc_debug_shutdown();
    pc_debug_config_read(&value);
    pc_debug_set(&value);
    QueryPerformanceFrequency(&frequency);
    previous_frame.QuadPart = 0;
#ifdef __APPLE__
    const char *path=getenv("RECOMP_LOG_FILE");
    log_path[0]=0;
    if(path && strlen(path)<sizeof log_path)snprintf(log_path,sizeof log_path,"%s",path);
    debug_window=window;
#else
    length = GetEnvironmentVariableW(L"RECOMP_LOG_FILE", log_path, (DWORD)(sizeof log_path / sizeof *log_path));
    if (!length || length >= sizeof log_path / sizeof *log_path) log_path[0] = 0;
    debug_window = window;
    if (window && !pc_debug_output_quiet()) SetTimer(window, PC_DEBUG_TIMER, 500, NULL);
#endif
    pc_debug_poll();
}

void pc_debug_shutdown(void)
{
#ifdef __APPLE__
    if(log_file)fclose(log_file);log_file=NULL;
#else
    if (debug_window) KillTimer(debug_window, PC_DEBUG_TIMER);
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    log_file = INVALID_HANDLE_VALUE;
#endif
    debug_window = NULL;
    partial_size = 0;
    AcquireSRWLockExclusive(&debug_lock);
    memset(&stats, 0, sizeof stats);
    history_count = history_next = 0;
    ReleaseSRWLockExclusive(&debug_lock);
}

void pc_debug_frame(void)
{
#ifndef __APPLE__
    pc_crash_frame();
#endif
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (previous_frame.QuadPart && frequency.QuadPart &&
        (double)(now.QuadPart - previous_frame.QuadPart) / frequency.QuadPart > .050) {
        AcquireSRWLockExclusive(&debug_lock); stats.slow_frames++; ReleaseSRWLockExclusive(&debug_lock);
    }
    previous_frame = now;
}

void pc_debug_sample(double fps, double shown_fps)
{
    int log;
    double ms;
    if (!isfinite(fps) || !isfinite(shown_fps) || fps <= 0 || shown_fps < 0) return;
    ms = 1000.0 / fps;
    AcquireSRWLockExclusive(&debug_lock);
    stats.fps = fps; stats.shown_fps = shown_fps; stats.frame_ms = ms;
    log = options.log_performance && !pc_debug_output_quiet();
    ReleaseSRWLockExclusive(&debug_lock);
    if (log) {
        fprintf(stderr, "[PC-DEBUG] %.1f game fps, %.1f shown fps, %.2f ms average frame interval\n", fps, shown_fps, ms);
        fflush(stderr);
    }
}

int pc_debug_overlay(char *text, size_t bytes, const char *cap)
{
    char fps[48] = "", ms[32] = "", issues[64] = "", cap_text[24] = "", latency[64]="";
    int shown;
    AcquireSRWLockShared(&debug_lock);
    shown = options.show_fps || options.show_frame_time || options.show_issues ||
            (options.show_latency && stats.network_mode);
    if (options.show_fps) {
        int native_rate = pc_native_rate();
        if (native_rate) snprintf(cap_text, sizeof cap_text, " target %d%s", native_rate,
                                  native_rate > 60 ? " (Beta)" : "");
        else if(cap && *cap)snprintf(cap_text, sizeof cap_text, " cap %s", cap);
        if (!stats.fps) snprintf(fps, sizeof fps, "-- fps  ");
        else if (stats.shown_fps > stats.fps + .1)
            snprintf(fps, sizeof fps, "%.1f/%.1f fps  ", stats.fps, stats.shown_fps);
        else snprintf(fps, sizeof fps, "%.1f fps  ", stats.fps);
    }
    if (options.show_frame_time) {
        if (!stats.fps) snprintf(ms, sizeof ms, "-- ms  ");
        else snprintf(ms, sizeof ms, "%.1f ms  ", stats.frame_ms);
    }
    if (options.show_issues) {
        if (stats.log_available) snprintf(issues, sizeof issues, "E:%lu W:%lu", stats.errors, stats.warnings);
        else snprintf(issues, sizeof issues, "Log unavailable");
    }
    if(options.show_latency && stats.network_mode) {
        if(GetTickCount64()-latency_updated>5000 || stats.latency_ms<0 || !stats.latency_peers)
            snprintf(latency,sizeof latency,"  RTT: waiting");
        else snprintf(latency,sizeof latency,"  RTT: %.1f ms",stats.latency_ms);
    }
    snprintf(text, bytes, "%s%s%s%s%s", fps, ms, issues, cap_text,latency);
    ReleaseSRWLockShared(&debug_lock);
    return shown;
}

static void record_line(const char *line)
{
    char lower[1024];
    size_t i;
    int severity = 0;
    for (i = 0; line[i] && i < sizeof lower - 1; i++) lower[i] = (char)tolower((unsigned char)line[i]);
    lower[i] = 0;
    /* These are log pattern matches, not an assertion that gameplay is broken. */
    if (strstr(lower, "[pc-debug]")) return;
    if (strstr(lower, "[native-fault]") || strstr(lower, "[crash]") || strstr(lower, "[error]") || strstr(lower, "fatal") ||
        strstr(lower, " failed") || strstr(lower, "[icall] unresolved") || strstr(lower, "exception")) severity = 2;
    else if (strstr(lower, "warning") || strstr(lower, "[warn") || strstr(lower, "unsupported") ||
             strstr(lower, "unimplemented") || strstr(lower, "anomaly") || strstr(lower, "unavailable")) severity = 1;
    if (!severity) return;
    AcquireSRWLockExclusive(&debug_lock);
    if (severity == 2) stats.errors++; else stats.warnings++;
    snprintf(history[history_next], sizeof history[history_next], "%s: %.370s", severity == 2 ? "Error" : "Warning", line);
    history_next = (history_next + 1) % 12;
    if (history_count < 12) history_count++;
    ReleaseSRWLockExclusive(&debug_lock);
}

void pc_debug_poll(void)
{
    if (pc_debug_output_quiet()) return;
#ifdef __APPLE__
    char buffer[4096];size_t budget=65536;int available=0;
    struct stat current,opened;
    if(log_file && (stat(log_path,&current) || current.st_dev!=log_device || current.st_ino!=log_inode)) {
        fclose(log_file);log_file=NULL;partial_size=0;
    }
    if(!log_file && log_path[0]) {
        int fd=open(log_path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
        if(fd>=0 && !fstat(fd,&opened) && S_ISREG(opened.st_mode)) {
            log_file=fdopen(fd,"rb");if(!log_file)close(fd);
            else {log_device=opened.st_dev;log_inode=opened.st_ino;}
        } else if(fd>=0)close(fd);
    }
    if(log_file && !fstat(fileno(log_file),&current)) {
        off_t position=ftello(log_file);available=1;
        if(position<0)available=0;
        else if(current.st_size<position){fseeko(log_file,0,SEEK_SET);partial_size=0;}
        clearerr(log_file);
        while(available && budget) {
            size_t count=fread(buffer,1,budget<sizeof buffer?budget:sizeof buffer,log_file),i;
            if(ferror(log_file))available=0;
            if(!count)break;
            budget-=count;
            for (i = 0; i < count; i++) {
                if (buffer[i] == '\n') {
                    partial[partial_size] = 0;
                    record_line(partial);
                    partial_size = 0;
                } else if (buffer[i] != '\r' && partial_size < sizeof partial - 1)
                    partial[partial_size++] = buffer[i];
            }
        }
    }
    AcquireSRWLockExclusive(&debug_lock);stats.log_available=available;ReleaseSRWLockExclusive(&debug_lock);
#else

    char buffer[4096];
    DWORD count, budget = 65536, i;
    LARGE_INTEGER size, position, zero = {0};
    int available = 0;
    if (log_file == INVALID_HANDLE_VALUE && log_path[0])
        log_file = CreateFileW(log_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log_file != INVALID_HANDLE_VALUE && GetFileSizeEx(log_file, &size) &&
        SetFilePointerEx(log_file, zero, &position, FILE_CURRENT)) {
        available = 1;
        if (size.QuadPart < position.QuadPart) {
            SetFilePointerEx(log_file, zero, NULL, FILE_BEGIN);
            partial_size = 0;
        }
        /* Bounded work keeps a noisy log from stalling the window's input loop. */
        while (budget) {
            DWORD wanted = budget < sizeof buffer ? budget : (DWORD)sizeof buffer;
            if (!ReadFile(log_file, buffer, wanted, &count, NULL)) { available = 0; break; }
            if (!count) break;
            budget -= count;
            for (i = 0; i < count; i++) {
                if (buffer[i] == '\n') {
                    partial[partial_size] = 0;
                    record_line(partial);
                    partial_size = 0;
                } else if (buffer[i] != '\r' && partial_size < sizeof partial - 1)
                    partial[partial_size++] = buffer[i];
            }
        }
    }
    AcquireSRWLockExclusive(&debug_lock); stats.log_available = available; ReleaseSRWLockExclusive(&debug_lock);
#endif
}

void pc_debug_snapshot(PcDebugSnapshot *out)
{
    unsigned i;
    size_t used = 0;
    AcquireSRWLockShared(&debug_lock);
    *out = stats;
    if(GetTickCount64()-latency_updated>5000) { out->latency_ms=-1; out->latency_peers=0; }
    out->recent[0] = 0;
    for (i = 0; i < history_count; i++) {
        unsigned index = (history_next + 12 - history_count + i) % 12;
        used += (size_t)snprintf(out->recent + used, sizeof out->recent - used, "%s\r\n", history[index]);
    }
    ReleaseSRWLockShared(&debug_lock);
}

void pc_debug_clear(void)
{
#ifdef __APPLE__
    if(log_file){clearerr(log_file);fseeko(log_file,0,SEEK_END);}
#else
    LARGE_INTEGER zero = {0};
    if (log_file != INVALID_HANDLE_VALUE) SetFilePointerEx(log_file, zero, NULL, FILE_END);
#endif
    partial_size = 0;
    AcquireSRWLockExclusive(&debug_lock);
    stats.errors = stats.warnings = stats.slow_frames = 0;
    history_count = history_next = 0;
    ReleaseSRWLockExclusive(&debug_lock);
}

#ifndef __APPLE__
int pc_debug_open_logs(HWND owner)
{
    WCHAR folder[1024], *slash;
    if (!log_path[0]) return 0;
    wcscpy_s(folder, sizeof folder / sizeof *folder, log_path);
    slash = wcsrchr(folder, L'\\');
    if (!slash) slash = wcsrchr(folder, L'/');
    if (!slash) return 0;
    *slash = 0;
    return (INT_PTR)ShellExecuteW(owner, L"open", folder, NULL, NULL, SW_SHOWNORMAL) > 32;
}

#endif
