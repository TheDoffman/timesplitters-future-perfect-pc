#include "guest_allocation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
typedef HANDLE NativeThread;
static CRITICAL_SECTION monitor;
static void monitor_enter(void) { EnterCriticalSection(&monitor); }
static void monitor_leave(void) { LeaveCriticalSection(&monitor); }
#define WORKER_RESULT DWORD WINAPI
#define WORKER_RETURN 0
#else
#include <pthread.h>
#include <sched.h>
typedef pthread_t NativeThread;
static pthread_mutex_t monitor = PTHREAD_MUTEX_INITIALIZER;
static void monitor_enter(void) { assert(!pthread_mutex_lock(&monitor)); }
static void monitor_leave(void) { assert(!pthread_mutex_unlock(&monitor)); }
#define WORKER_RESULT void *
#define WORKER_RETURN NULL
#endif

static unsigned char ram[65536], physical[131072];
static tsfp_allocation_record hr[128], pr[64];
static tsfp_allocations state;
typedef struct Worker { uint32_t id, heap, physical, size; } Worker;
static Worker workers[4];

static WORKER_RESULT worker(void *argument) {
    Worker *w = argument;
    for (unsigned round = 0; round < 2000; ++round) {
        uint32_t size = 16 + (round * 17 + w->id * 11) % 1024;
        uint32_t h = tsfp_heap_allocate(&state, size, 16);
        uint32_t p = tsfp_physical_allocate(&state, size, 4096);
        assert(h && p);
        monitor_enter();
        for (unsigned i = 0; i < 4; ++i) {
            Worker *other = workers + i;
            if (!other->heap) continue;
            assert(h + size <= other->heap || other->heap + other->size <= h);
            assert(p + 4096 <= other->physical || other->physical + 4096 <= p);
        }
        w->heap = h; w->physical = p; w->size = size;
        monitor_leave();
        assert(tsfp_allocation_remaining(&state, h) == size);
        assert(tsfp_allocation_remaining(&state, p + size - 1) == 1);
        for (uint32_t i = 0; i < size; ++i) {
            assert(ram[h+i] == 0); assert(physical[p-0x80000000u+i] == 0);
        }
        memset(ram+h, (int)w->id, size);
        memset(physical+(p-0x80000000u), (int)w->id, size);
#if defined(_WIN32)
        SwitchToThread();
#else
        sched_yield();
#endif
        for (uint32_t i = 0; i < size; ++i) {
            assert(ram[h+i] == w->id); assert(physical[p-0x80000000u+i] == w->id);
        }
        monitor_enter();
        assert(tsfp_heap_release(&state, h));
        assert(tsfp_physical_release(&state, p));
        w->heap = w->physical = 0;
        monitor_leave();
    }
    return WORKER_RETURN;
}

int main(void) {
    tsfp_allocation_config c = {0};
    NativeThread threads[4];
    c.ram = ram; c.ram_size = sizeof(ram); c.heap_begin = 256; c.reserve_begin = sizeof(ram);
    c.physical = physical; c.physical_base = 0x80000000u; c.physical_size = sizeof(physical);
    c.physical_begin = 0x80004000u; c.physical_end = 0x8001e000u;
    assert(tsfp_allocations_init(&state, &c, hr, 128, pr, 64));
#if defined(_WIN32)
    InitializeCriticalSection(&monitor);
#endif
    for (unsigned i = 0; i < 4; ++i) {
        workers[i].id = i+1;
#if defined(_WIN32)
        threads[i] = CreateThread(NULL, 0, worker, workers+i, 0, NULL);
        assert(threads[i]);
#else
        assert(!pthread_create(threads+i, NULL, worker, workers+i));
#endif
    }
    for (unsigned i = 0; i < 4; ++i) {
#if defined(_WIN32)
        assert(WaitForSingleObject(threads[i], 30000) == WAIT_OBJECT_0);
        assert(CloseHandle(threads[i]));
#else
        assert(!pthread_join(threads[i], NULL));
#endif
    }
    assert(!tsfp_physical_block(&state, 0, NULL, NULL));
    assert(tsfp_physical_extent(&state) == 16384);
    assert(tsfp_heap_allocate(&state, sizeof(ram)-256, 4) == 256);
#if defined(_WIN32)
    DeleteCriticalSection(&monitor);
#else
    assert(!pthread_mutex_destroy(&monitor));
#endif
    puts("8000 concurrent heap/physical allocation-write-query-release cycles passed");
    return 0;
}
