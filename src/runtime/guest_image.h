/* Project XBE loading interface. No host mapping or guest scheduling policy. */
#ifndef TSFP_GUEST_IMAGE_H
#define TSFP_GUEST_IMAGE_H
#include <stddef.h>
#include <stdint.h>

typedef enum tsfp_image_result {
    TSFP_IMAGE_OK, TSFP_IMAGE_ARGUMENT, TSFP_IMAGE_HEADER, TSFP_IMAGE_SECTION,
    TSFP_IMAGE_OVERLAP, TSFP_IMAGE_ENTRY, TSFP_IMAGE_IMPORT,
    TSFP_IMAGE_CERTIFICATE, TSFP_IMAGE_TLS, TSFP_IMAGE_DESTINATION
} tsfp_image_result;

typedef struct tsfp_image_info {
    uint32_t base, header_size, image_end, section_count;
    uint32_t image_lo, image_hi, code_lo, code_hi;
    uint32_t entry, thunk, thunk_count, debug;
    uint32_t title_id, region;
    uint32_t tls_directory, tls_start, tls_bytes, tls_zero_fill, tls_index;
    char title[121]; /* At most 40 UTF-16 units, encoded as UTF-8 plus NUL. */
} tsfp_image_info;

/* Input bytes must remain immutable throughout either call. Metadata is
 * published only on success. Inspection validates every byte the loader uses;
 * signatures, library names and optional debug metadata are not authenticated.
 * guest_limit is an exclusive guest address bound, at most UINT32_MAX. */
tsfp_image_result tsfp_image_inspect(const void *file, size_t size,
                                    size_t guest_limit, tsfp_image_info *info);
/* ram addresses guest byte zero, and is writable for ram_size bytes. Neither
 * file nor info may overlap ram, or each other. Validation/argument failure
 * leaves both ram and info unchanged. Holes outside headers/sections retain
 * their original bytes; section virtual tails are zeroed. */
tsfp_image_result tsfp_image_load(const void *file, size_t size, void *ram,
                                 size_t ram_size, tsfp_image_info *info);
/* Transitional runtime layout: low services begin at 0x700000, stack ends at
 * 0xf80000, and the initial TLS template has 0x200 bytes before thread data.
 * Validate this reservation before host mapping. Outputs change only on success.
 * This does not allocate memory or initialize/schedule guest threads. */
tsfp_image_result tsfp_image_bootstrap(const tsfp_image_info *info, size_t limit,
                                      uint32_t *low_shift, uint32_t *tls_total);
const char *tsfp_image_result_name(tsfp_image_result result);
#endif
