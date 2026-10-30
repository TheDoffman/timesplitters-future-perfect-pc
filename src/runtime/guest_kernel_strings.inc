/* Original counted-string imports. Host character mapping uses standard libc. */
#include "guest_strings.h"
#include <ctype.h>
#include <wctype.h>
static uint32_t bridge_string_upper(uint32_t value,unsigned width) {
    return width==1?(uint32_t)toupper((unsigned char)value):(uint32_t)towupper((wint_t)value);
}
static void bridge_string_initialize(unsigned width) {
    uint32_t destination=STACK_ARG(0),source=STACK_ARG(1);tsfp_guest_vm vm;
    xbox_GetGuestVirtualMemory(&vm);(void)tsfp_string_initialize(&vm,destination,source,width);g_eax=0;
}
static void bridge_string_compare(unsigned width,int equality) {
    uint32_t a=STACK_ARG(0),b=STACK_ARG(1),nocase=STACK_ARG(2);tsfp_guest_vm vm;int32_t difference=0;int valid;
    xbox_GetGuestVirtualMemory(&vm);valid=tsfp_string_compare(&vm,a,b,width,nocase?bridge_string_upper:NULL,&difference);
    g_eax=equality?(uint32_t)(valid && difference==0):(valid?(uint32_t)difference:0);
}
static void bridge_string_update(unsigned width,enum tsfp_string_operation operation,int status) {
    uint32_t destination=STACK_ARG(0),source=STACK_ARG(1),result;tsfp_guest_vm vm;
    xbox_GetGuestVirtualMemory(&vm);result=tsfp_string_update(&vm,destination,source,width,operation,bridge_string_upper);g_eax=status?result:0;
}
static void bridge_RtlInitAnsiString(void){bridge_string_initialize(1);}
static void bridge_RtlInitUnicodeString(void){bridge_string_initialize(2);}
static void bridge_RtlEqualString(void){bridge_string_compare(1,1);}
static void bridge_RtlEqualUnicodeString(void){bridge_string_compare(2,1);}
static void bridge_RtlCompareString(void){bridge_string_compare(1,0);}
static void bridge_RtlCompareUnicodeString(void){bridge_string_compare(2,0);}
static void bridge_RtlCopyString(void){bridge_string_update(1,TSFP_STRING_COPY,0);}
static void bridge_RtlCopyUnicodeString(void){bridge_string_update(2,TSFP_STRING_COPY,0);}
static void bridge_RtlUpperString(void){bridge_string_update(1,TSFP_STRING_UPPER,0);}
static void bridge_RtlAppendStringToString(void){bridge_string_update(1,TSFP_STRING_APPEND,1);}
static void bridge_RtlAppendUnicodeStringToString(void){bridge_string_update(2,TSFP_STRING_APPEND,1);}
static void bridge_RtlAppendUnicodeToString(void){bridge_string_update(2,TSFP_STRING_APPEND_RAW,1);}
static void bridge_RtlCompareMemory(void) {
    uint32_t a=STACK_ARG(0),b=STACK_ARG(1),bytes=STACK_ARG(2);tsfp_guest_vm vm;
    xbox_GetGuestVirtualMemory(&vm);g_eax=tsfp_memory_match(&vm,a,b,bytes);
}
static void bridge_RtlCompareMemoryUlong(void) {
    uint32_t address=STACK_ARG(0),bytes=STACK_ARG(1),pattern=STACK_ARG(2);tsfp_guest_vm vm;
    xbox_GetGuestVirtualMemory(&vm);g_eax=tsfp_memory_pattern(&vm,address,bytes,pattern);
}
