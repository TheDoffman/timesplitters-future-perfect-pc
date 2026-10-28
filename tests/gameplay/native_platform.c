/* Native host primitives required by the Xbox runtime; no game or save files. */
#include "platform/win32_compat.h"
#include "shared_views.h"
#include "guest_arena_service.h"
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %d: %s (error %u)\n", __LINE__, #test, (unsigned)GetLastError()); \
    return 1; } } while (0)

static DWORD WINAPI signal_event(void *event)
{
    SetEvent(event);
    return 0;
}

static DWORD WINAPI delayed_signal_event(void *event)
{
    Sleep(10);
    SetEvent(event);
    return 0;
}

int main(void)
{
    const size_t size = 64u * 1024u * 1024u;
    int input_was_open = fcntl(STDIN_FILENO, F_GETFD) >= 0;
    tsfp_shared_backend backend;
    tsfp_shared_views mapping = {0};
    tsfp_shared_view records[3];
    size_t page, granularity;
    CHECK(tsfp_shared_host_backend(&backend, &page, &granularity));
    CHECK(tsfp_shared_views_init(&mapping, &backend, size, page, granularity, records, 3));
    CHECK(!input_was_open || fcntl(STDIN_FILENO, F_GETFD) >= 0);

    /* Original ownership providers supply the live mapping contract. */
    uint32_t *low = tsfp_shared_views_map(&mapping, NULL, 0, size, TSFP_VIEW_WRITE);
    CHECK(low != NULL);
    uint32_t *mirror = tsfp_shared_views_map(&mapping, NULL, 0, size, TSFP_VIEW_WRITE);
    CHECK(mirror != NULL && mirror != low);
    low[0] = 0x58424f58u;
    CHECK(mirror[0] == 0x58424f58u);
    mirror[size / sizeof(uint32_t) - 1] = 0x12345678u;
    CHECK(low[size / sizeof(uint32_t) - 1] == 0x12345678u);
    CHECK(tsfp_shared_views_map(&mapping, low, 0, size, TSFP_VIEW_WRITE) == NULL);
    CHECK(low[0] == 0x58424f58u && mirror[0] == 0x58424f58u);
    CHECK(tsfp_shared_views_release(&mapping, mirror));
    CHECK(tsfp_shared_views_release(&mapping, low));
    CHECK(tsfp_shared_views_stop(&mapping));

#ifdef __APPLE__
    void *arena = w32_guest_arena_reserve(0x100000000ull);
    CHECK(arena != NULL);
    CHECK(tsfp_guest_arena_backend(&backend, &page, &granularity));
    CHECK(tsfp_shared_views_init(&mapping, &backend, size, page, granularity, records, 3));
    low = tsfp_shared_views_map(&mapping, arena, 0, size, TSFP_VIEW_WRITE);
    mirror = tsfp_shared_views_map(&mapping, (uint8_t *)arena + 0x80000000ull,
                                   0, size, TSFP_VIEW_WRITE);
    CHECK(low == arena && mirror != NULL);
    low[0] = 0xaabbccdd;
    CHECK(mirror[0] == 0xaabbccdd);
    CHECK(tsfp_shared_views_map(&mapping, arena, 0, size, TSFP_VIEW_WRITE) == NULL);
    CHECK(VirtualAlloc(arena, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
    CHECK(low[0] == 0xaabbccdd);
    CHECK(tsfp_shared_views_release(&mapping, low));
    low = tsfp_shared_views_map(&mapping, arena, 0, size, TSFP_VIEW_WRITE);
    CHECK(low == arena && low[0] == 0xaabbccdd);
    CHECK(tsfp_shared_views_release(&mapping, low) && tsfp_shared_views_release(&mapping, mirror));
    CHECK(tsfp_shared_views_stop(&mapping));
    CHECK(w32_guest_arena_release());
#endif

    HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(event != NULL);
    CHECK(WaitForSingleObject(event, 0) == WAIT_TIMEOUT);
    HANDLE thread = CreateThread(NULL, 0, signal_event, event, 0, NULL);
    CHECK(thread != NULL);
    CHECK(WaitForSingleObject(event, 2000) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    CHECK(CloseHandle(thread));
    /* A queued notification survives until consumed; auto reset consumes once.
     * A sleeping waiter is interruptible without the multi-wait polling loop. */
    CHECK(w32_WaitForEventMicroseconds(event, 500) == WAIT_TIMEOUT);
    CHECK(SetEvent(event));
    CHECK(w32_WaitForEventMicroseconds(event, 0) == WAIT_OBJECT_0);
    CHECK(w32_WaitForEventMicroseconds(event, 0) == WAIT_TIMEOUT);
    thread = CreateThread(NULL, 0, delayed_signal_event, event, 0, NULL);
    CHECK(thread != NULL);
    CHECK(w32_WaitForEventMicroseconds(event, 2000000) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    CHECK(CloseHandle(thread));
    CHECK(w32_WaitForEventMicroseconds(event, 0) == WAIT_TIMEOUT);
    CHECK(CloseHandle(event));
    event = CreateEventW(NULL, TRUE, TRUE, NULL);
    CHECK(event != NULL);
    CHECK(w32_WaitForEventMicroseconds(event, 0) == WAIT_OBJECT_0);
    CHECK(w32_WaitForEventMicroseconds(event, 0) == WAIT_OBJECT_0);
    CHECK(ResetEvent(event));
    CHECK(w32_WaitForEventMicroseconds(event, 500) == WAIT_TIMEOUT);
    CHECK(CloseHandle(event));
    CHECK(w32_WaitForEventMicroseconds(NULL, 0) == WAIT_FAILED);
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    CHECK(timer != NULL);
    CHECK(WaitForSingleObject(timer, 0) == WAIT_TIMEOUT);
    LARGE_INTEGER due; due.QuadPart = -200000; /* 20 ms */
    CHECK(SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE));
    CHECK(WaitForSingleObject(timer, 1) == WAIT_TIMEOUT);
    CHECK(WaitForSingleObject(timer, 2000) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(timer, 0) == WAIT_TIMEOUT); /* auto reset */
    CHECK(SetWaitableTimer(timer, &due, 10, NULL, NULL, FALSE));
    CHECK(WaitForSingleObject(timer, 2000) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(timer, 2000) == WAIT_OBJECT_0); /* periodic */
    CHECK(CancelWaitableTimer(timer));
    CHECK(WaitForSingleObject(timer, 30) == WAIT_TIMEOUT);
    CHECK(CloseHandle(timer));
    timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    CHECK(timer != NULL);
    due.QuadPart = 1; /* absolute time in the past */
    CHECK(SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE));
    CHECK(WaitForSingleObject(timer, 100) == WAIT_OBJECT_0);
    CHECK(CancelWaitableTimer(timer));
    CHECK(WaitForSingleObject(timer, 0) == WAIT_OBJECT_0); /* cancel preserves signal */
    due.QuadPart = -1000000;
    CHECK(SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE));
    CHECK(WaitForSingleObject(timer, 0) == WAIT_TIMEOUT); /* rearm clears signal */
    CHECK(CancelWaitableTimer(timer));
    CHECK(CloseHandle(timer));
    puts("PASS: relative/absolute, periodic, auto/manual-reset, cancel/rearm timers");
    puts("PASS: native 64 MiB mapping, alias coherence, collision safety, threads and events");
    return 0;
}
