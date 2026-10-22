#include "pc_native_contact_trace.h"
#include "pc_guest_memory.h"
/* Mapping cache is local to one synchronous observation; never retain it
 * across guest execution. The diagnostic fails closed on an unreadable span. */
static PcGuestRegion pc_contact_region;
static int pc_contact_read(uint32_t address,void *out,size_t bytes)
{
    if(address<0x10000u || address>=0x84000000u || bytes>0x84000000u-address)return 0;
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    if(pc_guest_region_remaining(&pc_contact_region,host,0)<bytes)
        pc_contact_region=pc_guest_query_region(host);
    if(pc_guest_region_remaining(&pc_contact_region,host,0)<bytes)return 0;
    memcpy(out,(const void *)host,bytes);return 1;
}
#if defined(_MSC_VER)
#define PC_CONTACT_NOINLINE __declspec(noinline)
#else
#define PC_CONTACT_NOINLINE __attribute__((noinline))
#endif
/* Keep the bounded snapshot's stack storage off the disabled hot path. */
static PC_CONTACT_NOINLINE void pc_native_contact_capture(PcContactTrace *trace)
{
    PcContactSnapshot snapshot;
    pc_contact_region=(PcGuestRegion){0};
    if(pc_contact_collect(pc_contact_read,g_esp,trace,&snapshot))
        pc_contact_record(trace,&snapshot,g_tsfp_native_timing_active);
    pc_contact_region=(PcGuestRegion){0};
}
static void pc_native_contact_trace(void)
{
    static PcContactTrace trace;
    if(pc_contact_enabled(&trace))pc_native_contact_capture(&trace);
}
#undef PC_CONTACT_NOINLINE
#define PC_NATIVE_PHYSICS_OBSERVE() pc_native_contact_trace()
