/* Native host primitives required by the Xbox runtime; no game or save files. */
#include "platform/win32_compat.h"
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
    const DWORD size = 64u * 1024u * 1024u;
    int input_was_open = fcntl(STDIN_FILENO, F_GETFD) >= 0;
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                       PAGE_READWRITE, 0, size, NULL);
    CHECK(mapping != NULL);
    CHECK(!input_was_open || fcntl(STDIN_FILENO, F_GETFD) >= 0);

    /* Use normal native addresses. This checks the mapping primitive, not
     * the game's complete relocated Xbox address-space initialization. */
    uint32_t *low = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CHECK(low != NULL);
    uint32_t *mirror = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    CHECK(mirror != NULL && mirror != low);
    low[0] = 0x58424f58u;
    CHECK(mirror[0] == 0x58424f58u);
    mirror[size / sizeof(uint32_t) - 1] = 0x12345678u;
    CHECK(low[size / sizeof(uint32_t) - 1] == 0x12345678u);

    /* An occupied-address request must fail without destroying either view. */
    CHECK(MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, low) == NULL);
    CHECK(low[0] == 0x58424f58u && mirror[0] == 0x58424f58u);
    CHECK(UnmapViewOfFile(mirror));
    CHECK(UnmapViewOfFile(low));
    CHECK(CloseHandle(mapping));

#ifdef __APPLE__
    void *arena = w32_guest_arena_reserve(0x100000000ull);
    CHECK(arena != NULL);
    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, NULL);
    CHECK(mapping != NULL);
    low = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, arena);
    mirror = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, (uint8_t *)arena + 0x80000000ull);
    CHECK(low == arena && mirror != NULL);
    low[0] = 0xaabbccdd;
    CHECK(mirror[0] == 0xaabbccdd);
    CHECK(MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, arena) == NULL);
    CHECK(VirtualAlloc(arena, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
    CHECK(low[0] == 0xaabbccdd);
    CHECK(UnmapViewOfFile(low));
    low = MapViewOfFileEx(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size, arena);
    CHECK(low == arena && low[0] == 0xaabbccdd);
    CHECK(UnmapViewOfFile(low) && UnmapViewOfFile(mirror));
    CHECK(CloseHandle(mapping));
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
