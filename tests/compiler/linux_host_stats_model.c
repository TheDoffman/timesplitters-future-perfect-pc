#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#undef _WIN32
#undef __APPLE__
#ifndef __linux__
#define __linux__ 1
#endif
#include "host_memory_stats.c"
static uint64_t pages;
static unsigned unit;
static int failed;
int sysinfo(struct sysinfo *out) {
    assert(!out->freeram && !out->mem_unit);
    out->freeram=pages;out->mem_unit=unit;return failed;
}
int main(void) {
    uint64_t value;
    unit=4096;pages=UINT64_C(0x1000001);
    assert(tsfp_host_available_memory(&value) && value==pages*unit);
    pages=0;assert(tsfp_host_available_memory(&value) && value==0);
    pages=UINT64_MAX;unit=1;
    assert(tsfp_host_available_memory(&value) && value==UINT64_MAX);
    value=42;unit=2;
    assert(!tsfp_host_available_memory(&value) && value==42);
    pages=1;unit=0;
    assert(!tsfp_host_available_memory(&value) && value==42);
    unit=4096;failed=-1;
    assert(!tsfp_host_available_memory(&value) && value==42);
    assert(!tsfp_host_available_memory(NULL));
    puts("Linux physical-memory units, overflow and failure preservation passed");
    return 0;
}
