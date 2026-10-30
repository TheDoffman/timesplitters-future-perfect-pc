/* Original counted-string and prefix-memory services over checked guest spans. */
#ifndef TSFP_GUEST_STRINGS_H
#define TSFP_GUEST_STRINGS_H
#include "guest_virtual_memory.h"
#include <stdint.h>
/* Case policy is supplied by the host's standard character library. The unit
 * width is 1 (bytes) or 2 (little-endian UTF-16 code units), never sizeof(wchar_t).
 * These operations allocate nothing. VM mappings must remain stable throughout.
 * Descriptor: Length16, MaximumLength16, Buffer32, all little-endian.
 * Invalid spans/odd UTF-16 lengths preserve output. Copy truncates at complete
 * units; append is all-or-nothing. Body overlap is supported via memmove.
 * Descriptor/body output overlap is rejected. Comparisons include embedded NULs.
 * Source MaximumLength is not a read bound: Length is the counted read contract. */
typedef uint32_t (*tsfp_string_case)(uint32_t unit,unsigned width);
enum tsfp_string_operation {TSFP_STRING_COPY,TSFP_STRING_UPPER,TSFP_STRING_APPEND,TSFP_STRING_APPEND_RAW};
uint32_t tsfp_string_initialize(const tsfp_guest_vm *,uint32_t destination,uint32_t source,unsigned width);
int tsfp_string_compare(const tsfp_guest_vm *,uint32_t a,uint32_t b,unsigned width,tsfp_string_case,int32_t *result);
uint32_t tsfp_string_update(const tsfp_guest_vm *,uint32_t destination,uint32_t source,unsigned width,enum tsfp_string_operation,tsfp_string_case);
uint32_t tsfp_memory_match(const tsfp_guest_vm *,uint32_t a,uint32_t b,uint32_t bytes);
uint32_t tsfp_memory_pattern(const tsfp_guest_vm *,uint32_t address,uint32_t bytes,uint32_t pattern);
#endif
