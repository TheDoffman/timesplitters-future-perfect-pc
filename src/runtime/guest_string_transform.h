/* Original creation and case transformation of guest-width string records. */
#ifndef TSFP_GUEST_STRING_TRANSFORM_H
#define TSFP_GUEST_STRING_TRANSFORM_H
#include "guest_virtual_memory.h"
typedef uint32_t (*tsfp_case_mapper)(uint32_t,unsigned,int);
/* Both services require stable caller-owned objects/mappings, disjoint from free
 * heap storage. Source/destination aliases are staged before allocation/writes.
 * Creation scans at most32767 UTF-16 units including a required NUL, then owns a
 * Length+2 byte buffer. Failure returns false with the descriptor unchanged. */
int tsfp_string_create(const tsfp_guest_vm *,uint32_t destination,uint32_t source);
/* Case transform copies exactly counted Length bytes, with NO terminator. Source
 * MaximumLength is not a bound. Odd Unicode lengths reject. Allocation publishes
 * MaximumLength=Length, even zero, and a real shared heap owner. BOOLEAN is a byte.
 * Nonallocating insufficient capacity returns BUFFER_OVERFLOW unchanged. Invalid
 * spans/descriptor-body aliases reject; failed fresh ownership is released, but
 * normal heap zeroing of unpublished free storage is not undone. Existing target
 * ownership is never implicitly released. Mapping is one code unit at a time. */
uint32_t tsfp_string_transform(const tsfp_guest_vm *,tsfp_case_mapper,uint32_t destination,uint32_t source,uint8_t allocate,int upper);
#endif
