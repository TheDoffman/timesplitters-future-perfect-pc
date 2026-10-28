/* Host page operations. No guest addresses, toolkit types or allocation policy. */
#ifndef TSFP_HOST_PAGES_H
#define TSFP_HOST_PAGES_H
#include <stddef.h>
#include <stdint.h>

typedef struct tsfp_host_page_region {
    uintptr_t base;
    size_t bytes;
    uint32_t protection;
} tsfp_host_page_region;

/* Access values are NT's base protection bits, without modifiers. Unsupported
 * values fail. WRITECOPY is unavailable for private anonymous Windows pages;
 * POSIX private writable pages have the corresponding read/write access. */
size_t tsfp_host_page_size(void);
void *tsfp_host_pages_map(size_t bytes, uint32_t protection);
int tsfp_host_pages_unmap(void *base, size_t bytes);
/* Round an interior span out to host-page boundaries. The caller owns those
 * pages and must keep their mappings stable. Do not pass unrelated memory.
 * Bad arguments fail before entering the host. Host errors are returned. */
int tsfp_host_pages_protect(void *address, size_t bytes, uint32_t protection);
/* Snapshot one mapped region, including inaccessible pages. Failure leaves
 * output untouched. This is not a lifetime/access pin, nor allocation metadata.
 * Windows reports its actual protection (including modifiers); Darwin maps its
 * actual R/W/X access to NT base bits. Regions can split on protection changes. */
int tsfp_host_pages_query(const void *address, tsfp_host_page_region *out);
#endif
