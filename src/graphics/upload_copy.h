#ifndef TSFP_UPLOAD_COPY_H
#define TSFP_UPLOAD_COPY_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Copy the source's row/slice layout to a tightly packed upload allocation.
 * Keep padded/short rows byte-for-byte equivalent to the original row loop.
 * The caller owns validated allocation/format extents. Return copy count for
 * bounded diagnostics/tests; optimized callers can discard it. */
static inline size_t tsfp_upload_rows(void *destination,const void *source,
                                     size_t row_bytes,size_t rows,size_t depth,
                                     size_t row_pitch,size_t slice_pitch,int batch)
{
    uint8_t *out=(uint8_t *)destination;
    const uint8_t *in=(const uint8_t *)source;
    size_t slice=row_bytes*rows,k,r;
    if(!row_bytes || !rows || !depth)return 0;
    if(batch && row_pitch==row_bytes) {
        if(slice_pitch==slice) {
            memcpy(out,in,slice*depth);
            return 1;
        }
        for(k=0;k<depth;k++)memcpy(out+k*slice,in+k*slice_pitch,slice);
        return depth;
    }
    for(k=0;k<depth;k++)for(r=0;r<rows;r++)
        memcpy(out+k*slice+r*row_bytes,in+k*slice_pitch+r*row_pitch,
               row_bytes<row_pitch?row_bytes:row_pitch);
    return rows*depth;
}
/* Exact byte identity includes NaN payloads and signed zero. Preserve an
 * already-dirty shadow; only skip making a clean snapshot dirty again. */
static inline int tsfp_upload_uniform(void *shadow,const void *source,size_t size,int *dirty)
{
    if(!memcmp(shadow,source,size))return 0;
    memcpy(shadow,source,size);*dirty=1;
    return 1;
}
#endif
