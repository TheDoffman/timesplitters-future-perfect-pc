/* Compile-only compatibility probe: actual installed runtime headers/types. */
#include "kernel.h"
#include "guest_virtual_memory.h"
#include "xbox_memory_layout.h"
#include <stdlib.h>
#include <string.h>
static uint32_t g_eax;
static uint32_t probe_argument(unsigned index) {return index;}
#define STACK_ARG(n) probe_argument(n)
static void *bridge_object_output(void *context,uint32_t address,uint32_t bytes) {
    tsfp_guest_vm memory;(void)context;xbox_GetGuestVirtualMemory(&memory);
    return memory.resolve(memory.host,address,bytes,1);
}
#include "guest_kernel_requests.inc"
uint32_t tsfp_request_adapter_header_probe(void) {
    bridge_request_init();bridge_IoAllocateIrp();bridge_IoInitializeIrp();bridge_IoFreeIrp();return g_eax;
}
