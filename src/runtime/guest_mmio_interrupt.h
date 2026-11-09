/* Native interrupt producer writes bypass guest write-one-to-clear semantics. */
#ifndef TSFP_GUEST_MMIO_INTERRUPT_H
#define TSFP_GUEST_MMIO_INTERRUPT_H
#include "guest_arena_service.h"
#include <stdint.h>
static inline int tsfp_guest_raise_vblank(uintptr_t base) {
    uint32_t *crtc=tsfp_guest_mmio_alias((void *)(base+UINT32_C(0xfd600100)),4);
    uint32_t *pmc=tsfp_guest_mmio_alias((void *)(base+UINT32_C(0xfd000100)),4);
    if(!crtc||!pmc)return 0;
    __atomic_fetch_or(crtc,1u,__ATOMIC_SEQ_CST);
    __atomic_fetch_or(pmc,0x01000000u,__ATOMIC_SEQ_CST);
    return 1;
}
#endif
