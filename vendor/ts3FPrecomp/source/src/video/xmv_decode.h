/*
 * xmv_decode.h -- WMV2 frames from an XMV file to BGRA pixels, through
 * FFmpeg's libavcodec loaded at run time. Without it every call fails and the
 * movie is skipped the way it always was.
 */
#ifndef XBOXRECOMP_XMV_DECODE_H
#define XBOXRECOMP_XMV_DECODE_H

#include <stdint.h>

typedef struct xmv_video_decoder xmv_video_decoder;

/* NULL if the decoder cannot be made; the reason goes to stderr once. */
xmv_video_decoder *xmv_decoder_create(uint32_t width, uint32_t height,
                                      const uint8_t extradata[4]);

/* Feed one frame (already byte-swapped by xmv_next_video). When a picture
 * comes out, it is written to `bgra` (width*height*4 bytes, top row first)
 * and 1 is returned; 0 means the decoder wants more input first; -1 an
 * error. */
int xmv_decoder_decode(xmv_video_decoder *dec, const uint8_t *data, uint32_t size,
                       uint32_t pts_ms, uint8_t *bgra);

void xmv_decoder_destroy(xmv_video_decoder *dec);

#endif
