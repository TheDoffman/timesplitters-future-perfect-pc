/* Thin standard-OS encoding adapter, independent of toolkit compatibility code. */
#ifndef TSFP_HOST_TEXT_CODEC_H
#define TSFP_HOST_TEXT_CODEC_H
#include "guest_text_codec.h"
typedef struct tsfp_host_text_codec {uint32_t codepage;} tsfp_host_text_codec;
/* Windows captures the system ANSI code page; native Mac preserves the existing
 * single-byte/Latin-1 fallback with '?' for non-byte UTF-16 units. No native NT
 * string record or host heap allocation is exposed as a guest address. */
void tsfp_host_text_codec_init(tsfp_host_text_codec *,tsfp_text_codec *);
#endif
