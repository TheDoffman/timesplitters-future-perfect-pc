/* Original counted conversion policy over explicit guest-width records. */
#ifndef TSFP_GUEST_TEXT_CODEC_H
#define TSFP_GUEST_TEXT_CODEC_H
#include "guest_virtual_memory.h"
enum tsfp_text_direction {TSFP_TEXT_TO_UNICODE=0,TSFP_TEXT_TO_ANSI=1};
typedef struct tsfp_text_codec {
    void *host;
    /* Source is a stable host copy; Unicode bytes are little-endian UTF-16.
     * NULL output queries required bytes. Otherwise capacity is sufficient;
     * callback must stay within it and return the exact written byte count. */
    int (*convert)(void *,unsigned,const unsigned char *,uint32_t,unsigned char *,uint32_t,uint32_t *);
    /* Bytes of full encoded output corresponding to the largest source prefix
     * fitting limit. Used only for ANSI overflow; UINT32_MAX means failure. */
    uint32_t (*ansi_prefix)(void *,const unsigned char *,uint32_t,uint32_t);
} tsfp_text_codec;
/* Eight-byte LE Length16/MaximumLength16/Buffer32 records. Source Length defines
 * its read span; source MaximumLength is not a bound. Allocate is a BOOLEAN byte.
 * ANSI->Unicode requires whole output+terminator; Unicode->ANSI may publish a
 * bounded prefix+terminator with BUFFER_OVERFLOW. Structural failures preserve
 * descriptor/body. Fresh allocation validation failure releases ownership; the
 * allocator's normal zeroing of unpublished/free storage is not rolled back.
 * Caller owns stable mappings/access/data and valid descriptor/source storage
 * disjoint from free allocator storage. Output body/descriptor overlap rejects.
 * Native locale policy belongs to the supplied codec, not host wchar_t records. */
uint32_t tsfp_text_string_convert(const tsfp_guest_vm *,const tsfp_text_codec *,uint32_t destination,uint32_t source,uint8_t allocate,unsigned direction);
/* Free only exact heap ownership, then clear the descriptor. NULL record/buffer
 * is an unchanged success; malformed/unowned/self-overlapping records reject.
 * Records must outlive their body; no VM storage is unmapped or lifetime pinned. */
int tsfp_text_string_free(const tsfp_guest_vm *,uint32_t descriptor);
#endif
