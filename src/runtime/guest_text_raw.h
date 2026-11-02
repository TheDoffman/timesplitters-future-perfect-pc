/* Original bounded raw charset policy; no native or guest string descriptors. */
#ifndef TSFP_GUEST_TEXT_RAW_H
#define TSFP_GUEST_TEXT_RAW_H
#include "guest_text_codec.h"
#include "guest_string_transform.h"
enum tsfp_raw_text_operation {TSFP_RAW_TO_UNICODE,TSFP_RAW_UNICODE_SIZE,TSFP_RAW_TO_ANSI,TSFP_RAW_ANSI_SIZE,TSFP_RAW_UPPER_ANSI};
/* N operations copy a capacity-bounded prefix without adding NUL; Unicode output
 * truncates at complete16-bit units, ANSI at codec prefix boundaries. Ordinary N
 * truncation succeeds; uppercase N returns BUFFER_OVERFLOW. Size operations need
 * count and ignore output/capacity. N count may be NULL. Unicode input ignores an
 * incomplete last byte. Only effective input/actual output spans are resolved.
 * Source is staged before publication, supporting aliases. Output/count overlap
 * rejects; callbacks/access/mappings/data must stay stable. No heap owner changes.
 * Upper mapping keeps the existing host character policy, not a native NLS table. */
uint32_t tsfp_text_raw_convert(const tsfp_guest_vm *,const tsfp_text_codec *,tsfp_case_mapper,
    uint32_t output,uint32_t capacity,uint32_t count,uint32_t source,uint32_t bytes,unsigned operation);
#endif
