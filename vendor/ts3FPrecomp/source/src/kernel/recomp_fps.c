/**
 * Frame rate, measured at the one place every configuration shares.
 *
 * Every counter the runtime had was per subsystem -- shadow swaps, executor
 * flips, pushbuffer segments -- so two configurations could not be compared on
 * the number that matters. This counts the title's own D3DDevice_Swap at the
 * HLE boundary, before the replacement does anything, so it means the same
 * thing whether host drawing is on or off. It also counts delivered vblanks,
 * because a title that paces on vblank cannot present faster than they arrive.
 *
 * RECOMP_FPS=<seconds> prints one line per window to stderr:
 *
 *   [FPS] t=  20.0s  14.9 fps (149 swaps)  vblank  40.0 Hz | run mean 15.1 fps
 *
 * The window is measured from QueryPerformanceCounter, not from the number of
 * seconds asked for, so a reporter thread that wakes late does not read as a
 * slow frame. The run mean is total swaps over the time since the first swap:
 * frame rate swings with what is on screen, so a single window is noise and
 * the mean over the whole run is what two runs should be compared on.
 *
 * Off unless RECOMP_FPS is set; on, it costs one interlocked increment per
 * swap and one per vblank.
 */

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "xbox_memory_layout.h"

static volatile LONG64 s_swaps;
static volatile LONG64 s_vblanks;
static volatile LONG   s_started;
static volatile LONG   s_span[5];        /* frames spanning 0, 1, 2, 3, 4+ vblanks */

/* RECOMP_WAIT_PROFILE=1: where the swapping thread's time goes, frame by frame.
 *
 * Each kernel call the thread that swaps makes is timed (xbox_FpsNoteKernel,
 * from the bridge dispatch), and at each swap the frame's totals go into one
 * of two sums: frames that took one vblank, and frames that took more. The
 * report compares them. Time not spent in a kernel call is the title's own
 * code, the HLE, and the renderer. For KeWaitForSingleObject (159) the object
 * waited on is kept too, since "waited" is only half an answer. */
#define WP_ORDINALS 400
#define WP_OBJECTS  8
typedef struct {
    long long ticks[WP_ORDINALS];
    long long frame_ticks;
    uint32_t  obj[WP_OBJECTS];
    long long obj_ticks[WP_OBJECTS];
    unsigned long frames;
} wait_sums;

static int        s_wp_on = -1;
static DWORD      s_wp_tid;               /* the thread that swaps */
static wait_sums  s_wp_cur, s_wp_slow, s_wp_fast;
static long long  s_wp_frame_start;
static CRITICAL_SECTION s_wp_cs;

static int wp_on(void)
{
    if (s_wp_on < 0) {
        const char *v = getenv("RECOMP_WAIT_PROFILE");
        InitializeCriticalSection(&s_wp_cs);
        s_wp_on = v && *v && *v != '0';
    }
    return s_wp_on;
}

static void wp_add_obj(wait_sums *s, uint32_t obj, long long t)
{
    int i, free_slot = -1;
    for (i = 0; i < WP_OBJECTS; i++) {
        if (s->obj[i] == obj) { s->obj_ticks[i] += t; return; }
        if (!s->obj[i] && free_slot < 0) free_slot = i;
    }
    if (free_slot >= 0) { s->obj[free_slot] = obj; s->obj_ticks[free_slot] = t; }
}

static volatile uint32_t s_wp_hot;
static volatile LONG     s_wp_hot_other_waits;
static volatile LONG     s_wp_hot_other_tid;

void xbox_FpsNoteKernel(unsigned ordinal, long long ticks, uint32_t object)
{
    if (!wp_on() || ordinal >= WP_ORDINALS)
        return;
    if (GetCurrentThreadId() != s_wp_tid) {
        /* Another thread waiting on the event the frame waits for. */
        if (ordinal == 159 && object && object == s_wp_hot) {
            InterlockedIncrement(&s_wp_hot_other_waits);
            InterlockedExchange(&s_wp_hot_other_tid, (LONG)GetCurrentThreadId());
        }
        return;
    }
    s_wp_cur.ticks[ordinal] += ticks;
    if (ordinal == 159 && object)
        wp_add_obj(&s_wp_cur, object, ticks);
}

int xbox_FpsWaitProfileOn(void)
{
    return wp_on();
}

/* Sets of the object the swapping thread waits on longest, from any thread
 * (bridge_KeSetEvent). Against the vblanks delivered in the same window, it
 * says whether the event the frame waits for is set every vblank. */
static volatile LONG     s_wp_hot_sets, s_wp_hot_sets_nonzero;

void xbox_FpsNoteSet(uint32_t object, int was_signalled)
{
    if (s_wp_on > 0 && object && object == s_wp_hot) {
        InterlockedIncrement(&s_wp_hot_sets);
        if (was_signalled)
            InterlockedIncrement(&s_wp_hot_sets_nonzero);
    }
}

static void wp_frame_end(LONG64 span)
{
    LARGE_INTEGER now;
    wait_sums *into;
    int i;

    QueryPerformanceCounter(&now);
    if (!s_wp_frame_start) {
        s_wp_frame_start = now.QuadPart;
        s_wp_tid = GetCurrentThreadId();
        return;
    }
    s_wp_tid = GetCurrentThreadId();
    s_wp_cur.frame_ticks = now.QuadPart - s_wp_frame_start;
    s_wp_frame_start = now.QuadPart;
    EnterCriticalSection(&s_wp_cs);
    into = span >= 2 ? &s_wp_slow : &s_wp_fast;
    for (i = 0; i < WP_ORDINALS; i++)
        into->ticks[i] += s_wp_cur.ticks[i];
    for (i = 0; i < WP_OBJECTS; i++)
        if (s_wp_cur.obj[i])
            wp_add_obj(into, s_wp_cur.obj[i], s_wp_cur.obj_ticks[i]);
    into->frame_ticks += s_wp_cur.frame_ticks;
    into->frames++;
    LeaveCriticalSection(&s_wp_cs);
    memset(&s_wp_cur, 0, sizeof s_wp_cur);
}

static void wp_print(const char *name, const wait_sums *s, double ms_per_tick)
{
    int top[4] = { -1, -1, -1, -1 }, i, k;
    long long in_kernel = 0;

    if (!s->frames)
        return;
    for (i = 0; i < WP_ORDINALS; i++) {
        in_kernel += s->ticks[i];
        for (k = 0; k < 4; k++)
            if (top[k] < 0 || s->ticks[i] > s->ticks[top[k]]) {
                memmove(&top[k + 1], &top[k], (3 - k) * sizeof top[0]);
                top[k] = i;
                break;
            }
    }
    fprintf(stderr, "[WAITPROF] %s frames: %lu, %.1f ms each; in kernel %.1f ms, "
            "outside %.1f ms. Top:",
            name, s->frames, s->frame_ticks * ms_per_tick / s->frames,
            in_kernel * ms_per_tick / s->frames,
            (s->frame_ticks - in_kernel) * ms_per_tick / s->frames);
    for (k = 0; k < 4; k++)
        if (top[k] >= 0 && s->ticks[top[k]])
            fprintf(stderr, " ordinal %d %.2f ms", top[k],
                    s->ticks[top[k]] * ms_per_tick / s->frames);
    fprintf(stderr, " | 159 on:");
    for (i = 0; i < WP_OBJECTS; i++)
        if (s->obj[i])
            fprintf(stderr, " 0x%08X %.2f ms", s->obj[i],
                    s->obj_ticks[i] * ms_per_tick / s->frames);
    fputc('\n', stderr);
}

static DWORD WINAPI fps_reporter(LPVOID param)
{
    double secs = *(double *)param;
    LARGE_INTEGER qpf, t0, prev, now;
    LONG64 prev_swaps, prev_vblanks, base_vblanks;
    HANDLE timer;

    free(param);
    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&t0);
    prev = t0;
    prev_swaps = InterlockedCompareExchange64(&s_swaps, 0, 0);
    /* Vblanks start before the first swap. Counting the ones delivered
     * during the boot against the time since the first swap read as a pump
     * running at 62-67 Hz in the first windows' run mean, when every window
     * measured 60.0; the run mean counts from here too. */
    prev_vblanks = base_vblanks = InterlockedCompareExchange64(&s_vblanks, 0, 0);

    /* Sleep is only a wake-up call here; the window is measured, not assumed. */
    timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    for (;;) {
        LONG64 swaps, vblanks;
        double window, elapsed;

        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)(secs * 10000000.0);
            SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep((DWORD)(secs * 1000.0));
        }

        QueryPerformanceCounter(&now);
        swaps   = InterlockedCompareExchange64(&s_swaps, 0, 0);
        vblanks = InterlockedCompareExchange64(&s_vblanks, 0, 0);
        window  = (double)(now.QuadPart - prev.QuadPart) / (double)qpf.QuadPart;
        elapsed = (double)(now.QuadPart - t0.QuadPart) / (double)qpf.QuadPart;
        if (window <= 0.0)
            window = secs;

        fprintf(stderr, "[FPS] t=%7.1fs %6.1f fps (%lld swaps)  vblank %6.1f Hz"
                " | run mean %.2f fps, vblank %.2f Hz over %.0fs\n",
                elapsed,
                (double)(swaps - prev_swaps) / window, swaps - prev_swaps,
                (double)(vblanks - prev_vblanks) / window,
                (double)swaps / elapsed,
                (double)(vblanks - base_vblanks) / elapsed, elapsed);
        fprintf(stderr, "[FPS]   vblanks per frame: 0:%ld 1:%ld 2:%ld 3:%ld 4+:%ld\n",
                InterlockedExchange(&s_span[0], 0), InterlockedExchange(&s_span[1], 0),
                InterlockedExchange(&s_span[2], 0), InterlockedExchange(&s_span[3], 0),
                InterlockedExchange(&s_span[4], 0));
        if (wp_on()) {
            double ms_per_tick = 1000.0 / (double)qpf.QuadPart;
            EnterCriticalSection(&s_wp_cs);
            wp_print("late (2+ vblanks)", &s_wp_slow, ms_per_tick);
            wp_print("on time (1 vblank)", &s_wp_fast, ms_per_tick);
            if (s_wp_hot) {
                extern ptrdiff_t g_xbox_mem_offset;
                unsigned type = *(volatile uint8_t *)((uintptr_t)s_wp_hot + g_xbox_mem_offset);
                fprintf(stderr, "[WAITPROF] KeSetEvent on 0x%08X (type %u): %ld in this "
                        "window (%ld of them on an event already set), vblanks %lld; "
                        "waits on it from other threads %ld (last tid %ld)\n",
                        s_wp_hot, type, InterlockedExchange(&s_wp_hot_sets, 0),
                        InterlockedExchange(&s_wp_hot_sets_nonzero, 0),
                        vblanks - prev_vblanks,
                        InterlockedExchange(&s_wp_hot_other_waits, 0),
                        (long)s_wp_hot_other_tid);
            }
            {
                /* The object to count sets on next window: the one waited on
                 * longest in the frames just reported. */
                int i;
                long long best = 0;
                for (i = 0; i < WP_OBJECTS; i++)
                    if (s_wp_slow.obj_ticks[i] + s_wp_fast.obj_ticks[i] > best) {
                        best = s_wp_slow.obj_ticks[i] + s_wp_fast.obj_ticks[i];
                        s_wp_hot = s_wp_slow.obj[i] ? s_wp_slow.obj[i] : s_wp_fast.obj[i];
                    }
            }
            memset(&s_wp_slow, 0, sizeof s_wp_slow);
            memset(&s_wp_fast, 0, sizeof s_wp_fast);
            LeaveCriticalSection(&s_wp_cs);
        }
        fflush(stderr);
        prev = now;
        prev_swaps = swaps;
        prev_vblanks = vblanks;
    }
    return 0;
}

static void fps_start_once(void)
{
    const char *v;
    double *secs;
    HANDLE h;

    if (InterlockedCompareExchange(&s_started, 1, 0) != 0)
        return;
    v = getenv("RECOMP_FPS");
    if (!v || !*v)
        return;
    secs = (double *)malloc(sizeof *secs);
    if (!secs)
        return;
    *secs = atof(v);
    if (*secs <= 0.0)
        *secs = 10.0;
    h = CreateThread(NULL, 0, fps_reporter, secs, 0, NULL);
    if (h)
        CloseHandle(h);
    else
        free(secs);
}

/* The title presented a frame. The reporter starts at the first one, so the
 * first window is not padded with the boot. */
/* How many vblanks each presented frame spanned: 1 everywhere is 60 fps,
 * and a frame rate between 30 and 60 is a mix of 1s and 2s. Which frames
 * took two says whether the title was late or chose to wait. Printed with
 * the [FPS] line; reset each window. s_span is declared at the top. */
void xbox_FpsCountSwap(void)
{
    static LONG64 last = -1;
    LONG64 v = InterlockedCompareExchange64(&s_vblanks, 0, 0);

    if (!s_started)
        fps_start_once();
    InterlockedIncrement64(&s_swaps);
    if (last >= 0) {
        LONG64 d = v - last;
        InterlockedIncrement(&s_span[d < 0 ? 0 : d > 4 ? 4 : (int)d]);
        if (wp_on())
            wp_frame_end(d);
    }
    last = v;
}

/* The kernel delivered a vblank to the title's ISR. */
void xbox_FpsCountVblank(void)
{
    InterlockedIncrement64(&s_vblanks);
}

#else  /* !_WIN32 */

void xbox_FpsCountSwap(void)   {}
int  xbox_FpsWaitProfileOn(void) { return 0; }
void xbox_FpsNoteKernel(unsigned ordinal, long long ticks, uint32_t object)
{ (void)ordinal; (void)ticks; (void)object; }
void xbox_FpsNoteSet(uint32_t object, int was_signalled)
{ (void)object; (void)was_signalled; }
void xbox_FpsCountVblank(void) {}

#endif
