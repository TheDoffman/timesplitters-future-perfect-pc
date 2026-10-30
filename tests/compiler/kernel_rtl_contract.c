#include "guest_kernel_rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"RTL ABI line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t args[3],g_eax,g_esp=0x11223344,g_xbox_kernel_caller=0x55667788,g_xbox_code_lo=0x8000,g_xbox_code_hi=0x9000;
static unsigned reads[3],allowed,lookups,faults,services;static enum tsfp_kernel_critical_action expected;
static enum tsfp_critical_result forced=TSFP_CRITICAL_OK;
static uint32_t argument(unsigned i){CHECK(i<allowed);++reads[i];return args[i];}
#define STACK_ARG(i) argument(i)
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    unsigned i;for(i=0;i<3;++i)CHECK(reads[i]==(i<allowed?1u:0u));++lookups;memset(vm,0,sizeof(*vm));vm->ram_size=0xabcdef;
}
enum tsfp_critical_result tsfp_kernel_rtl_critical(const tsfp_guest_vm *vm,uint32_t address,enum tsfp_kernel_critical_action action,const tsfp_kernel_rtl_site *site) {
    CHECK(vm->ram_size==0xabcdef && address==0xfedcba98 && action==expected);
    CHECK(site->caller==0x55667788 && site->stack==0x11223344 && site->code_begin==0x8000 && site->code_end==0x9000);++services;return forced;
}
void tsfp_kernel_rtl_fault(enum tsfp_critical_result result){CHECK(result==forced);++faults;}
uint32_t tsfp_kernel_rtl_error(uint32_t status){CHECK(status==0xfedcba98 && reads[0]==1);++services;return 0x87654321;}
void tsfp_kernel_rtl_rip(const tsfp_guest_vm *vm,uint32_t a,uint32_t b,uint32_t c){CHECK(vm->ram_size==0xabcdef && a==0xfedcba98 && b==0x76543210 && c==0x12345678);++services;}
#include "guest_kernel_rtl.inc"
static uint32_t invoke(void (*fn)(void),unsigned count,int vm_lookup) {
    unsigned old=lookups,called=services,i;allowed=count;memset(reads,0,sizeof(reads));args[0]=0xfedcba98;args[1]=0x76543210;args[2]=0x12345678;g_eax=0xdeadbeef;fn();
    CHECK(services==called+1 && lookups==old+(unsigned)vm_lookup);
    for(i=0;i<3;++i)CHECK(reads[i]==(i<count?1u:0u));
    CHECK(g_esp==0x11223344 && g_xbox_kernel_caller==0x55667788 && g_xbox_code_lo==0x8000 && g_xbox_code_hi==0x9000);return g_eax;
}
int main(void) {
    expected=TSFP_KERNEL_CRITICAL_INIT;CHECK(!invoke(bridge_RtlInitializeCriticalSection,1,1));
    expected=TSFP_KERNEL_CRITICAL_ENTER;CHECK(!invoke(bridge_RtlEnterCriticalSection,1,1));CHECK(!invoke(bridge_RtlEnterCriticalSectionAndRegion,1,1));
    expected=TSFP_KERNEL_CRITICAL_LEAVE;CHECK(!invoke(bridge_RtlLeaveCriticalSection,1,1));CHECK(!invoke(bridge_RtlLeaveCriticalSectionAndRegion,1,1));
    expected=TSFP_KERNEL_CRITICAL_TRY;CHECK(invoke(bridge_RtlTryEnterCriticalSection,1,1)==1);
    forced=TSFP_CRITICAL_BUSY;CHECK(!invoke(bridge_RtlTryEnterCriticalSection,1,1) && !faults);
    forced=TSFP_CRITICAL_NO_MEMORY;CHECK(!invoke(bridge_RtlTryEnterCriticalSection,1,1) && !faults);
    expected=TSFP_KERNEL_CRITICAL_ENTER;CHECK(!invoke(bridge_RtlEnterCriticalSection,1,1) && faults==1);
    forced=TSFP_CRITICAL_INVALID;CHECK(!invoke(bridge_RtlEnterCriticalSection,1,1) && faults==1);
    expected=TSFP_KERNEL_CRITICAL_LEAVE;forced=TSFP_CRITICAL_NOT_OWNER;CHECK(!invoke(bridge_RtlLeaveCriticalSection,1,1) && faults==1);
    CHECK(invoke(bridge_RtlNtStatusToDosError,1,0)==0x87654321);CHECK(!invoke(bridge_RtlRip,3,1));
    puts("PASS: eight RTL import ABIs, shared critical actions, fail-closed hook, boolean/void/status returns and argument snapshots");return 0;
}
