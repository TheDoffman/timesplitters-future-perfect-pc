/* Original guest device call adapters. Include after the initialized project
 * device factory, STACK_ARG and guest result register. Factory lifetime, driver
 * callbacks, type export and embedded-event ownership must be wired separately.
 * The shared patch selects this adapter in the kernel bridge. */
#include "guest_device_factory.h"
static uint32_t bridge_device_status(int result) {
    switch(result) {
    case TSFP_DEVICE_OK:return UINT32_C(0);
    case TSFP_DEVICE_BAD_ADDRESS:return UINT32_C(0xc0000005);
    case TSFP_DEVICE_NO_MEMORY:return UINT32_C(0xc000009a);
    case TSFP_DEVICE_COLLISION:return UINT32_C(0xc0000035);
    case TSFP_DEVICE_CLOSED:return UINT32_C(0xc0000056);
    default:return UINT32_C(0xc000000d);
    }
}
static void bridge_IoCreateDevice(void) {
    uint32_t driver=STACK_ARG(0),extension=STACK_ARG(1),name=STACK_ARG(2);
    uint32_t type=STACK_ARG(3),exclusive=(uint8_t)STACK_ARG(4),output=STACK_ARG(5);
    g_eax=bridge_device_status(tsfp_device_factory_create(&bridge_devices,
        driver,extension,name,type,exclusive!=0,output));
}
static void bridge_IoDeleteDevice(void) {
    uint32_t body=STACK_ARG(0);
    (void)tsfp_device_factory_delete(&bridge_devices,body);
    g_eax=0;
}
