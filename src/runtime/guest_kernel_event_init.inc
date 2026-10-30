/* Original KeInitializeEvent bridge. The transitional shadow registry consumes
 * the host handle, including its table-full close path; wait scheduling remains
 * in the existing adapters until the complete dispatcher replacement. */
#include "guest_event.h"
static void bridge_KeInitializeEvent(void) {
    uint32_t address=STACK_ARG(0),type=STACK_ARG(1);int initial_state=(uint8_t)STACK_ARG(2)!=0;
    unsigned char candidate[16];void *mapping;HANDLE event;
    g_eax=0;
    if(!tsfp_event_initialize(candidate,sizeof(candidate),address,type,initial_state) ||
       !(mapping=bridge_object_output(NULL,address,16)))return;
    event=CreateEventW(NULL,type==0?TRUE:FALSE,initial_state?TRUE:FALSE,NULL);
    if(!event) {xbox_log(XBOX_LOG_WARN,XBOX_LOG_IO,"Guest event host creation failed at %08x",(unsigned)address);return;}
    ke_shadow_insert(address,event);memcpy(mapping,candidate,sizeof(candidate));
}
