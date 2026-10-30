/* Original pool import adapters. Guest addresses belong to the shared guest
 * allocator; no native process-heap pointer is returned, queried or freed. */
#include "guest_virtual_memory.h"
static uint32_t bridge_pool_allocate(uint32_t bytes) {
    tsfp_guest_vm memory;xbox_GetGuestVirtualMemory(&memory);
    return tsfp_heap_allocate(memory.allocations,bytes,16);
}
static void bridge_ExAllocatePool(void) {g_eax=bridge_pool_allocate(STACK_ARG(0));}
static void bridge_ExAllocatePoolWithTag(void) {
    uint32_t bytes=STACK_ARG(0),tag=STACK_ARG(1);
    /* The tag is diagnostic data, not a second allocation/lifetime authority. */
    (void)tag;g_eax=bridge_pool_allocate(bytes);
}
static void bridge_ExFreePool(void) {
    uint32_t address=STACK_ARG(0);tsfp_guest_vm memory;
    xbox_GetGuestVirtualMemory(&memory);(void)tsfp_heap_release(memory.allocations,address);g_eax=0;
}
static void bridge_ExQueryPoolBlockSize(void) {
    uint32_t address=STACK_ARG(0);tsfp_guest_vm memory;
    xbox_GetGuestVirtualMemory(&memory);g_eax=tsfp_allocation_remaining(memory.allocations,address);
}
