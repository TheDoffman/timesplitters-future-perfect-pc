#include "guest_thread_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
typedef HANDLE NativeThread;
#define WORKER_RESULT DWORD WINAPI
#define WORKER_RETURN 0
#else
#include <pthread.h>
typedef pthread_t NativeThread;
#define WORKER_RESULT void *
#define WORKER_RETURN NULL
#endif

enum { RAM_SIZE = 40 * 1024 * 1024, RECORDS = 2048 };
static unsigned char *ram, *expected;
static tsfp_allocations allocations;
static tsfp_allocation_record records[RECORDS];
static tsfp_thread_memory state;
static tsfp_image_info info;

static uint32_t word(uint32_t address) {
    uint32_t result; memcpy(&result, ram + address, 4); return result;
}
static void put(unsigned char *bytes, uint32_t address, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[address+i] = (unsigned char)(value >> (i*8));
}
static void same_memory(void) {
    if (!memcmp(ram, expected, RAM_SIZE)) return;
    for (size_t i = 0; i < RAM_SIZE; ++i) if (ram[i] != expected[i]) {
        fprintf(stderr, "guest byte %zu: got %u expected %u\n", i, ram[i], expected[i]);
        abort();
    }
}
static void setup(unsigned tls_bytes, unsigned zero, int has_tls, unsigned shift) {
    tsfp_allocation_config config = {0};
    memset(ram, 0xa5, RAM_SIZE);
    memset(&allocations, 0, sizeof(allocations));
    memset(&state, 0, sizeof(state));
    memset(&info, 0, sizeof(info));
    info.base = 0x10000; info.image_end = shift ? 0x700000 + shift - 0x100 : 0x650000;
    if (has_tls) {
        info.tls_directory = 0x11000; info.tls_start = 0x20000;
        info.tls_bytes = tls_bytes; info.tls_zero_fill = zero;
        for (unsigned i = 0; i < tls_bytes; ++i) ram[0x20000+i] = (unsigned char)(0x31+i);
    }
    config.ram = ram; config.ram_size = RAM_SIZE;
    config.heap_begin = shift + 0xf80000; config.reserve_begin = RAM_SIZE;
    assert(tsfp_allocations_init(&allocations, &config, records, RECORDS, NULL, 0));
}
static void expected_main(unsigned shift, unsigned total) {
    uint32_t object = shift + 0x760000, tls = shift + 0x770000, data = shift + 0x770200;
    memcpy(expected, ram, RAM_SIZE);
    memset(expected+0x1000, 0, 64);
    memset(expected+object, 0, 512);
    memset(expected+shift+0x761000, 0, 1024);
    put(expected, 0x1000, 0xffffffffu);
    put(expected, 0x1004, total ? tls+total : shift+0xf7fff0);
    put(expected, 0x1008, shift+0x780000);
    put(expected, 0x1018, 0x1000);
    put(expected, 0x1020, shift+0x761000);
    put(expected, 0x1028, object);
    put(expected, object+0x28, total ? tls : shift+0x700000);
    put(expected, object+0x12c, 0x1000);
    if (total) {
        memset(expected+tls, 0, total);
        memset(expected+data, 0, 64);
        memcpy(expected+tls, expected+info.tls_start, info.tls_bytes);
        put(expected, tls, data);
    }
}
static void main_contracts(void) {
    static const unsigned initial[] = {0, 1, 4, 16, 31, 496};
    static const unsigned totals[] = {4, 20, 20, 20, 36, 500};
    for (unsigned shifted = 0; shifted < 2; ++shifted) {
        unsigned shift = shifted ? 0x1a0000 : 0;
        setup(0, 0, 0, shift); expected_main(shift, 0);
        assert(tsfp_thread_memory_init(&state, &allocations, &info)); same_memory();
        assert(!tsfp_thread_tib_create(&state));
        for (unsigned i = 0; i < sizeof(initial)/sizeof(initial[0]); ++i) {
            setup(initial[i], 0, 1, shift); expected_main(shift, totals[i]);
            assert(tsfp_thread_memory_init(&state, &allocations, &info)); same_memory();
            assert(state.tls_total == totals[i] && state.low_shift == shift);
        }
        setup(7, 10, 1, shift); expected_main(shift, 36);
        assert(tsfp_thread_memory_init(&state, &allocations, &info)); same_memory();
    }
}
static void private_contracts(void) {
    uint32_t prior = 0;
    setup(16, 0, 1, 0x1a0000);
    assert(tsfp_thread_memory_init(&state, &allocations, &info));
    /* Guest changes outside the provider's rebinding fields must survive. */
    put(ram, 0x1018, 0x12345678);
    put(ram, 0x900028, 0x22334455);
    ram[0x910008] = 0xd4; ram[0x900040] = 0xf1;
    for (unsigned i = 0; i < 32; ++i) {
        uint32_t tib = tsfp_thread_tib_create(&state), tls, data, object;
        unsigned char tib_expected[64], object_expected[512], tls_expected[20];
        assert(tib && !(tib & 15u) && tib != prior);
        tls = word(tib+4)-20; data = word(tls); object = word(tib+0x28);
        assert(tls == tib+64 && data == tls+20 && object >= data+64 && !(object & 15u));
        assert(tsfp_allocation_remaining(&allocations, tib) >= object+512-tib);
        assert(tsfp_thread_current_object(&state, tib) == object);
        memcpy(tib_expected, ram+0x1000, 64);
        put(tib_expected, 0, 0xffffffffu); put(tib_expected, 4, tls+20); put(tib_expected, 0x28, object);
        assert(!memcmp(tib_expected, ram+tib, 64));
        memcpy(object_expected, ram+0x900000, 512); put(object_expected, 0x12c, 0x1001+i);
        assert(!memcmp(object_expected, ram+object, 512));
        memcpy(tls_expected, ram+0x910000, 20); put(tls_expected, 0, data);
        assert(!memcmp(tls_expected, ram+tls, 20));
        for (unsigned n = 0; n < 64; ++n) assert(ram[data+n] == 0);
        ram[data+4] = (unsigned char)i; prior = tib;
    }
    assert(!tsfp_thread_current_object(&state, 0));
    assert(!tsfp_thread_current_object(&state, 1));
    assert(!tsfp_thread_current_object(&state, UINT32_MAX));
    state.next_id = UINT32_MAX;
    {
        uint32_t last = tsfp_thread_tib_create(&state);
        assert(last && word(word(last+0x28)+0x12c) == UINT32_MAX);
        memcpy(expected, ram, RAM_SIZE);
        assert(!tsfp_thread_tib_create(&state)); same_memory();
    }
    tsfp_thread_memory_stop(&state);
    assert(!tsfp_thread_tib_create(&state) && !tsfp_thread_current_object(&state, 0x1000));
}
static void failure_contracts(void) {
    setup(16, 0, 1, 0x1a0000);
    assert(tsfp_thread_memory_init(&state, &allocations, &info));
    for (unsigned mode = 0; mode < 9; ++mode) {
        tsfp_thread_memory before = state;
        tsfp_image_info bad = info;
        uint32_t old_begin = allocations.heap.begin, old_origin = allocations.heap.origin;
        unsigned old_active = allocations.active;
        memcpy(expected, ram, RAM_SIZE);
        if (mode == 0) bad.base = 0x1000;
        if (mode == 1) bad.image_end = RAM_SIZE;
        if (mode == 2) bad.tls_bytes = 497;
        if (mode == 3) bad.tls_start = bad.image_end-8;
        if (mode == 4) bad.tls_directory = 0;
        if (mode == 5) allocations.heap.begin = 0x10000;
        if (mode == 6) allocations.active = 0;
        if (mode == 7) allocations.heap.origin = 4;
        if (mode == 8) bad.tls_zero_fill = UINT32_MAX;
        assert(!tsfp_thread_memory_init(&state, &allocations, &bad));
        assert(!memcmp(&state, &before, sizeof(state))); same_memory();
        allocations.heap.begin = old_begin; allocations.heap.origin = old_origin;
        allocations.active = old_active;
    }
    memcpy(expected, ram, RAM_SIZE);
    assert(!tsfp_thread_memory_init((tsfp_thread_memory *)(ram+0x1800000), &allocations, &info));
    same_memory();
    {
        uint32_t occupied;
        allocations.heap.capacity = 1;
        occupied = tsfp_heap_allocate(&allocations, 16, 16); assert(occupied);
        memcpy(expected, ram, RAM_SIZE);
        assert(!tsfp_thread_tib_create(&state) && state.next_id == 0x1001); same_memory();
        assert(tsfp_heap_release(&allocations, occupied));
        assert(tsfp_thread_tib_create(&state));
    }
}
static void stack_contracts(void) {
    uint32_t tops[8];
    setup(0, 0, 0, 0);
    assert(tsfp_thread_memory_init(&state, &allocations, &info));
    for (unsigned i = 0; i < 8; ++i) {
        tops[i] = tsfp_thread_stack_acquire(&state); assert(tops[i]);
        assert(!((tops[i]+16) & 4095u));
        assert(tsfp_allocation_remaining(&allocations, tops[i]+16-524288) == 524288);
        for (unsigned j = 0; j < i; ++j) assert(tops[i] != tops[j]);
    }
    assert(!tsfp_thread_stack_acquire(&state));
    assert(!tsfp_thread_stack_release(&state, tops[0]+4));
    assert(!tsfp_thread_stack_release(&state, 0));
    assert(!tsfp_thread_stack_acquire(&state));
    {
        uint32_t foreign = tsfp_heap_allocate(&allocations, 524288, 4096);
        assert(foreign);
        assert(!tsfp_thread_stack_release(&state, foreign+524288-16));
        assert(tsfp_allocation_remaining(&allocations, foreign) == 524288);
        assert(tsfp_heap_release(&allocations, foreign));
    }
    assert(tsfp_thread_stack_release(&state, tops[3]));
    assert(!tsfp_thread_stack_release(&state, tops[3]));
    assert(tsfp_thread_stack_acquire(&state) == tops[3]);
    for (unsigned i = 0; i < 8; ++i) assert(tsfp_thread_stack_release(&state, tops[i]));
    assert(allocations.heap.count == 0);
    for (int i = 0; i < 16; ++i) assert(tsfp_worker_stack_acquire(&state) == i);
    assert(tsfp_worker_stack_acquire(&state) == -1);
    assert(!tsfp_worker_stack_release(&state, -1) && !tsfp_worker_stack_release(&state, 16));
    assert(tsfp_worker_stack_release(&state, 7));
    assert(!tsfp_worker_stack_release(&state, 7));
    assert(tsfp_worker_stack_acquire(&state) == 7);
    tsfp_thread_memory_stop(&state);
    assert(!tsfp_thread_stack_acquire(&state) && tsfp_worker_stack_acquire(&state) == -1);
    assert(!tsfp_worker_stack_release(&state, 7));
    assert(tsfp_thread_memory_init(&state, &allocations, &info));
    assert(tsfp_worker_stack_acquire(&state) == 0 && state.next_id == 0x1001);
}

typedef struct Worker { uint32_t identities[100]; } Worker;
static Worker workers[4];
static WORKER_RESULT work(void *parameter) {
    Worker *worker = parameter;
    for (unsigned n = 0; n < 100; ++n) {
        uint32_t tib = tsfp_thread_tib_create(&state), top = tsfp_thread_stack_acquire(&state);
        int fixed = tsfp_worker_stack_acquire(&state);
        assert(tib && top && fixed >= 0);
        worker->identities[n] = word(word(tib+0x28)+0x12c);
        put(ram, top-4, worker->identities[n]);
        assert(word(top-4) == worker->identities[n]);
        assert(tsfp_worker_stack_release(&state, fixed));
        assert(tsfp_thread_stack_release(&state, top));
    }
    return WORKER_RETURN;
}
static void concurrent_contracts(void) {
    NativeThread handles[4]; unsigned char seen[400] = {0};
    setup(16, 0, 1, 0x1a0000);
    assert(tsfp_thread_memory_init(&state, &allocations, &info));
    for (unsigned i = 0; i < 4; ++i) {
#if defined(_WIN32)
        handles[i] = CreateThread(NULL, 0, work, workers+i, 0, NULL); assert(handles[i]);
#else
        assert(!pthread_create(handles+i, NULL, work, workers+i));
#endif
    }
    for (unsigned i = 0; i < 4; ++i) {
#if defined(_WIN32)
        assert(WaitForSingleObject(handles[i], 30000) == WAIT_OBJECT_0);
        assert(CloseHandle(handles[i]));
#else
        assert(!pthread_join(handles[i], NULL));
#endif
    }
    for (unsigned i = 0; i < 4; ++i) for (unsigned j = 0; j < 100; ++j) {
        uint32_t id = workers[i].identities[j];
        assert(id >= 0x1001 && id < 0x1001+400 && !seen[id-0x1001]); seen[id-0x1001] = 1;
    }
    assert(state.next_id == 0x1001+400 && !state.worker_mask);
    for (unsigned i = 0; i < 8; ++i) assert(!state.stacks[i]);
    assert(allocations.heap.count == 400); /* published identity storage remains live */
}
static void supported_contracts(const char *path) {
    FILE *file;
    long length;
    unsigned char *bytes;
    tsfp_allocation_config config = {0};
#if defined(_WIN32)
    assert(!fopen_s(&file, path, "rb"));
#else
    file = fopen(path, "rb");
#endif
    assert(file && !fseek(file, 0, SEEK_END));
    length = ftell(file); assert(length > 0 && length < 64*1024*1024);
    rewind(file); bytes = malloc((size_t)length); assert(bytes);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length);
    assert(!fclose(file)); memset(ram, 0xa5, RAM_SIZE);
    assert(tsfp_image_load(bytes, (size_t)length, ram, RAM_SIZE, &info) == TSFP_IMAGE_OK);
    free(bytes);
    assert(info.title_id == 0x45410066 && info.section_count == 23 && info.thunk_count == 151);
    config.ram = ram; config.ram_size = RAM_SIZE; config.heap_begin = 0x1120000;
    config.reserve_begin = RAM_SIZE;
    assert(tsfp_allocations_init(&allocations, &config, records, RECORDS, NULL, 0));
    expected_main(0x1a0000, 20);
    assert(tsfp_thread_memory_init(&state, &allocations, &info)); same_memory();
    assert(state.low_shift == 0x1a0000 && state.tls_total == 20);
    assert(word(0x1004) == 0x910014 && word(0x1028) == 0x900000);
    assert(tsfp_thread_tib_create(&state) && tsfp_thread_stack_acquire(&state));
    puts("supported US bootstrap passed");
}
int main(int argc, char **argv) {
    assert(argc == 2 || argc == 3);
    ram = malloc(RAM_SIZE); expected = malloc(RAM_SIZE); assert(ram && expected);
    if (!strcmp(argv[1], "main")) main_contracts();
    else if (!strcmp(argv[1], "private")) private_contracts();
    else if (!strcmp(argv[1], "failure")) failure_contracts();
    else if (!strcmp(argv[1], "stacks")) stack_contracts();
    else if (!strcmp(argv[1], "concurrent")) concurrent_contracts();
    else if (!strcmp(argv[1], "us") && argc == 3) supported_contracts(argv[2]);
    else abort();
    free(expected); free(ram); puts("thread memory contracts passed"); return 0;
}
