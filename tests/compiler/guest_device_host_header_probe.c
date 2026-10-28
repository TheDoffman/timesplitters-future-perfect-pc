/* Compile-only actual runtime header/host-binding boundary. */
#include "kernel.h"
#include "recomp_types.h"
#include "guest_virtual_memory.h"
#include "xbox_memory_layout.h"
#include "guest_object_services.h"
#include "guest_object_scope.h"
#include <stdlib.h>
#include <string.h>
static tsfp_guest_objects bridge_objects;
static tsfp_guest_object_services bridge_object_services;
static RECOMP_TLS tsfp_guest_object_scopes bridge_object_scopes;
static int bridge_objects_initialized;
static void *bridge_object_output(void *context,uint32_t address,uint32_t bytes) {
    tsfp_guest_vm memory;(void)context;xbox_GetGuestVirtualMemory(&memory);
    return memory.resolve(memory.host,address,bytes,1);
}
static void bridge_object_word(void *target,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)((unsigned char *)target)[i]=(unsigned char)(value>>(i*8));
}
#include "guest_kernel_device_host.inc"
static uint32_t probe_argument(unsigned index) {return index;}
#define STACK_ARG(n) probe_argument(n)
#include "guest_kernel_devices.inc"
#include "guest_kernel_names.inc"
uint32_t tsfp_device_host_header_probe(void) {
    tsfp_device_event_scope frame;
    bridge_device_init();bridge_IoCreateDevice();bridge_IoDeleteDevice();bridge_ObReferenceObjectByName();
    if(bridge_device_event_begin(&frame,0x10034))
        (void)tsfp_device_event_scope_end(&bridge_object_scopes,&frame);
    bridge_device_pump();return g_eax+(uint32_t)bridge_device_free_body(0x10000,TSFP_KIND_DEVICE,0);
}
