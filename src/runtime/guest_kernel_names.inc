/* Original guest ordinal 247 adapter. The including bridge supplies STACK_ARG,
 * g_eax and the initialized project-owned bridge_name_service. */
#include "guest_name_service.h"
static void bridge_ObReferenceObjectByName(void) {
    uint32_t descriptor=STACK_ARG(0);
    uint32_t attributes=STACK_ARG(1);
    uint32_t type=STACK_ARG(2);
    uint32_t parse=STACK_ARG(3);
    uint32_t output=STACK_ARG(4);
    g_eax=tsfp_guest_name_reference(&bridge_name_service,descriptor,attributes,
                                  type,parse,output);
}
