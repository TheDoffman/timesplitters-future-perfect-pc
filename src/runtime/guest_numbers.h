/* Original checked 32-bit integer/string conversions, without native records. */
#ifndef TSFP_GUEST_NUMBERS_H
#define TSFP_GUEST_NUMBERS_H
#include "guest_virtual_memory.h"
/* Bases0/2/8/10/16; format0 means decimal. Parse0 recognizes lowercase
 * 0x/0o/0b, otherwise decimal. ASCII control/space prefix, optional sign,
 * digit prefix and modulo2^32 accumulation. Raw ANSI stops at NUL; Unicode
 * uses the counted little-endian Length16/MaximumLength16/Buffer32 record.
 * Empty Unicode and unsupported Unicode bases return INVALID_PARAMETER with
 * value0 for valid spans. Structural/mapping failures and other unsupported
 * bases preserve outputs.
 * ANSI writes digits and NUL only if spare capacity exists, without padding.
 * Unicode requires terminator room and publishes Length excluding that NUL.
 * Insufficient capacity returns BUFFER_OVERFLOW with all outputs unchanged.
 * Input/output overlap is buffered; Unicode output body/record overlap,
 * including host aliases, is rejected. No allocation or lifetime pin occurs;
 * caller owns stable mappings/access/data throughout. */
uint32_t tsfp_number_from_ansi(const tsfp_guest_vm *,uint32_t source,uint32_t base,uint32_t destination);
uint32_t tsfp_number_from_unicode(const tsfp_guest_vm *,uint32_t source,uint32_t base,uint32_t destination);
uint32_t tsfp_number_to_ansi(const tsfp_guest_vm *,uint32_t value,uint32_t base,uint32_t capacity,uint32_t destination);
uint32_t tsfp_number_to_unicode(const tsfp_guest_vm *,uint32_t value,uint32_t base,uint32_t destination);
#endif
