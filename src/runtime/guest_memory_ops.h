/* Original bounded memory and mask operations over guest-width values. */
#ifndef TSFP_GUEST_MEMORY_OPS_H
#define TSFP_GUEST_MEMORY_OPS_H
#include "guest_virtual_memory.h"
/* Return1 on success,0 on invalid spans/width; no partial writes on failure.
 * Zero actual byte count is a no-op without resolving either address. Width4
 * repeats a little-endian DWORD and ignores a final0..3 bytes. Width1 uses the
 * low byte. Caller owns stable mapping/access/data; no lifetime pin or MMIO. */
int tsfp_memory_fill(const tsfp_guest_vm *,uint32_t destination,uint32_t bytes,uint32_t pattern,unsigned width);
int tsfp_memory_move(const tsfp_guest_vm *,uint32_t destination,uint32_t source,uint32_t bytes);
/* Snapshot the16-byte LE mapping before publication, permitting output aliases.
 * Expand generic flags in read/write/execute/all order, then clear all four. */
int tsfp_memory_map_mask(const tsfp_guest_vm *,uint32_t mask,uint32_t mapping);
uint32_t tsfp_memory_byte_swap(uint32_t value,unsigned width);
#endif
