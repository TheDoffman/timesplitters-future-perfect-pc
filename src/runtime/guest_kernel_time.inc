/* Original calendar import adapters. Validate complete guest spans before writes. */
#include "guest_time.h"
#include "guest_virtual_memory.h"
static void *bridge_time_span(const tsfp_guest_vm *memory,uint32_t address,uint32_t bytes,int writable) {
    if(!address || address>UINT32_MAX-(bytes-1) || !memory->resolve)return NULL;
    return memory->resolve(memory->host,address,bytes,writable);
}
static void bridge_RtlTimeFieldsToTime(void) {
    uint32_t fields=STACK_ARG(0),time=STACK_ARG(1);tsfp_guest_vm memory;
    const void *input;void *output;unsigned char result[8];
    xbox_GetGuestVirtualMemory(&memory);
    input=bridge_time_span(&memory,fields,16,0);output=bridge_time_span(&memory,time,8,1);
    g_eax=0;
    if(input && output && tsfp_time_fields_to_ticks(input,16,result,sizeof(result))) {
        memcpy(output,result,sizeof(result));g_eax=1;
    }
}
static void bridge_RtlTimeToTimeFields(void) {
    uint32_t time=STACK_ARG(0),fields=STACK_ARG(1);tsfp_guest_vm memory;
    const void *input;void *output;unsigned char result[16]={0};
    xbox_GetGuestVirtualMemory(&memory);
    input=bridge_time_span(&memory,time,8,0);output=bridge_time_span(&memory,fields,16,1);
    if(input && output) {
        (void)tsfp_time_ticks_to_fields(input,8,result,sizeof(result));
        memcpy(output,result,sizeof(result));
    }
    g_eax=0;
}
