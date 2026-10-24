#ifndef TSFP_PC_GPU_ACK_H
#define TSFP_PC_GPU_ACK_H
#include <stdint.h>
void xbox_Nv2aNotifyPfbFlush(uint32_t guest_address);
/* Supported US XBE: sub_003D6690 writes the PFB flush request at 0x3D66C3
 * and polls it at 0x3D66D0. Wake the existing acknowledgement worker
 * after that exact store; the runtime also checks the register address.
 * Constant instruction sites remove this hook from every other store.
 * This is independent of game rate and does not advance any game clock. */
#define RECOMP_AFTER_STORE32(site, address) \
    do { if ((site) == 0x3D66C3u) \
        xbox_Nv2aNotifyPfbFlush((uint32_t)(address)); } while (0)
#endif
