#include "pc_native_character_trace.h"
#include "pc_guest_memory.h"
#ifdef RECOMP_MANUAL_TLS
/* The manual translation unit declares ESI later than its clock include. */
extern RECOMP_MANUAL_TLS uint32_t g_esi;
#endif
extern void sub_00120AB0(void);
static PcCharacterTrace pc_character_trace;
static PcGuestRegion pc_character_region;
static int pc_character_read(uint32_t address,void *out,size_t bytes)
{
    if(!pc_character_span(address,bytes))return 0;
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    if(!pc_guest_memory_in_region(&pc_character_region,host,bytes,0))return 0;
    memcpy(out,(const void *)host,bytes);return 1;
}
int pc_native_character_trace_requested(void) { return pc_character_enabled(&pc_character_trace); }
void pc_native_character_update(void)
{
    PcCharacterSnapshot before,after;uint32_t state=g_esi;
    pc_character_region=(PcGuestRegion){0};
    int valid=pc_character_collect(pc_character_read,state,&before)==1;
    pc_character_region=(PcGuestRegion){0};
    sub_00120AB0();
    /* Mapping snapshots must never survive guest calls or model teardown. */
    pc_character_region=(PcGuestRegion){0};
    if(valid) {
        int status=pc_character_collect(pc_character_read,state,&after);
        pc_character_record(&pc_character_trace,&before,&after,status,g_tsfp_native_timing_active);
    }
    pc_character_region=(PcGuestRegion){0};
}
