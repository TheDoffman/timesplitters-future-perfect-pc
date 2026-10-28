/* Original guest counted-name ABI; device parsing is adapter policy. */
#ifndef TSFP_GUEST_NAME_SERVICE_H
#define TSFP_GUEST_NAME_SERVICE_H
#include "guest_object_names.h"
#include "guest_object_services.h"
#define TSFP_NAME_BAD_DESCRIPTOR UINT32_C(0xc0000033)
#define TSFP_NAME_NOT_FOUND UINT32_C(0xc0000034)
typedef struct tsfp_guest_name_service {
    tsfp_guest_names *names;
    tsfp_guest_object_services *references;
    void *context;
    /* Read the entire span without MMIO effects. Input/output bytes and mappings
     * remain stable through the call. Callbacks never enter guest execution. */
    const void *(*resolve_input)(void *,uint32_t address,uint32_t bytes);
} tsfp_guest_name_service;
/* Eight-byte guest ANSI_STRING is Length16,MaximumLength16,Buffer32. Only the
 * counted Length bytes are read. Current policy accepts attributes0/0x40 and
 * no parse context. Other policies need an explicit device/namespace adapter.
 * Failure preserves the four-byte output. Success owns one guest body reference. */
uint32_t tsfp_guest_name_reference(tsfp_guest_name_service *,uint32_t descriptor,
    uint32_t attributes,uint32_t expected_type,uint32_t parse_context,uint32_t output);
#endif
