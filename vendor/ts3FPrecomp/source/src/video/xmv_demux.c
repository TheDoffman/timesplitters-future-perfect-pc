/*
 * xmv_demux.c -- see xmv_demux.h.
 *
 * Checked against every XMV file on hand before it was written: all six of
 * TimeSplitters: Future Perfect's walk to the last byte with this layout.
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "xmv_demux.h"

#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* Read the packet of `size` bytes at the current position and lay out where
 * its video and audio sit. */
static int load_packet(xmv_demux *d, uint32_t size)
{
    uint32_t hdr, vword, vsize, pos, i;

    if (size < 12u + 4u * (uint32_t)d->audio_count)
        return -1;
    free(d->packet);
    d->packet = (uint8_t *)malloc(size);
    if (!d->packet || fread(d->packet, 1, size, d->f) != size)
        return -1;
    d->packet_size = size;
    d->next_packet_size = rd32(d->packet);
    /* Eight bytes of video header, of which only the first word is known. */
    vword = rd32(d->packet + 4);
    hdr = 12u + 4u * (uint32_t)d->audio_count;
    /* FFmpeg: the stated video size runs four bytes long per audio track. */
    vsize = (vword & 0x007FFFFFu) - 4u * (uint32_t)d->audio_count;
    d->frames_left = (vword >> 23) & 0xFFu;
    pos = hdr;
    if (vword & 0x80000000u) {           /* codec data first */
        if (pos + 4u > size || vsize < 4u)
            return -1;
        if (!d->have_extradata) {
            /* XMV packs the WMV2 flags into its own bit layout; rebuild the
             * standard WMV2 codec data from it, big-endian (FFmpeg's
             * xmv_read_extradata). Handing a decoder the raw word decodes
             * every frame to blocks. */
            uint32_t x = rd32(d->packet + pos), w = 0;
            w |= (x & 0x01u) << 15;          /* mspel */
            w |= ((x >> 1) & 1u) << 14;      /* loop filter */
            w |= ((x >> 2) & 1u) << 13;      /* abt */
            w |= ((x >> 3) & 1u) << 12;      /* j-type */
            w |= ((x >> 4) & 1u) << 11;      /* top-left mv */
            w |= ((x >> 5) & 1u) << 10;      /* per-mb rl */
            w |= ((x >> 6) & 7u) << 7;       /* slice count */
            d->extradata[0] = (uint8_t)(w >> 24);
            d->extradata[1] = (uint8_t)(w >> 16);
            d->extradata[2] = (uint8_t)(w >> 8);
            d->extradata[3] = (uint8_t)w;
            d->have_extradata = 1;
        }
        pos += 4u;
        vsize -= 4u;
    }
    if (pos + vsize > size)
        return -1;
    d->video_off = pos;
    d->video_left = vsize;
    pos += vsize;
    for (i = 0; i < (uint32_t)d->audio_count; i++) {
        uint32_t a = rd32(d->packet + 12u + 4u * i) & 0x007FFFFFu;
        if (a == 0u && i > 0u)
            a = d->audio_size[i - 1];    /* FFmpeg: a repeated track */
        if (pos + a > size)
            a = size - pos;
        d->audio_size[i] = a;
        if (a && d->aq_len[i] + a > d->aq_cap[i]) {
            uint32_t cap = (d->aq_len[i] + a) * 2u;
            uint8_t *q = (uint8_t *)realloc(d->aq[i], cap);
            if (!q)
                return -1;
            d->aq[i] = q;
            d->aq_cap[i] = cap;
        }
        if (a) {
            memcpy(d->aq[i] + d->aq_len[i], d->packet + pos, a);
            d->aq_len[i] += a;
        }
        pos += a;
    }
    return 0;
}

int xmv_open(xmv_demux *d, const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f) {
        memset(d, 0, sizeof(*d));
        return -1;
    }
    return xmv_open_file(d, f);
}

int xmv_open_file(xmv_demux *d, FILE *f)
{
    uint8_t h[36 + 12 * XMV_MAX_AUDIO];
    uint32_t this_size, header_size;
    int i;

    memset(d, 0, sizeof(*d));
    d->f = f;
    if (!d->f)
        return -1;
    if (fread(h, 1, 36, d->f) != 36 || memcmp(h + 12, "xobX", 4) != 0)
        goto bad;
    this_size = rd32(h + 4);
    d->version = rd32(h + 16);
    d->width = rd32(h + 20);
    d->height = rd32(h + 24);
    d->duration_ms = rd32(h + 28);
    d->audio_count = rd16(h + 32);
    if (d->audio_count > XMV_MAX_AUDIO || !d->width || !d->height)
        goto bad;
    if (d->audio_count &&
        fread(h + 36, 1, 12u * (size_t)d->audio_count, d->f) != 12u * (size_t)d->audio_count)
        goto bad;
    for (i = 0; i < d->audio_count; i++) {
        const uint8_t *t = h + 36 + 12 * i;
        d->audio[i].codec = rd16(t);
        d->audio[i].channels = rd16(t + 2);
        d->audio[i].rate = rd32(t + 4);
        d->audio[i].bits = rd16(t + 8);
        d->audio[i].flags = rd16(t + 10);
    }
    /* The first packet is the rest of the first `this_size` bytes. */
    header_size = 36u + 12u * (uint32_t)d->audio_count;
    if (this_size <= header_size || load_packet(d, this_size - header_size) != 0)
        goto bad;
    return 0;
bad:
    xmv_close(d);
    return -1;
}

void xmv_close(xmv_demux *d)
{
    int i;

    if (d->f)
        fclose(d->f);
    free(d->packet);
    for (i = 0; i < XMV_MAX_AUDIO; i++) {
        free(d->aq[i]);
        free(d->aq_out[i]);
    }
    memset(d, 0, sizeof(*d));
}

int xmv_next_video(xmv_demux *d, const uint8_t **data, uint32_t *size,
                   uint32_t *pts_ms, int *keyframe)
{
    uint32_t fh, fsize, i;
    uint8_t *p;

    if (d->eof || !d->packet)
        return 0;
    while (!d->frames_left || d->video_left < 4u) {
        if (!d->next_packet_size) {
            d->eof = 1;
            return 0;
        }
        /* A frame count of 0 is a packet with audio and no video (FFmpeg
         * starts such a packet at its audio): its video bytes are padding. */
        if (load_packet(d, d->next_packet_size) != 0) {
            d->eof = 1;
            return feof(d->f) ? 0 : -1;
        }
    }
    fh = rd32(d->packet + d->video_off);
    fsize = (fh & 0x1FFFFu) * 4u + 4u;
    if (fsize + 4u > d->video_left)
        return -1;
    p = d->packet + d->video_off + 4u;
    /* The bitstream is stored as little-endian words: swap each in place so a
     * standard WMV2 decoder reads it. */
    for (i = 0; i + 4u <= fsize; i += 4u) {
        uint8_t t0 = p[i], t1 = p[i + 1];
        p[i] = p[i + 3];
        p[i + 1] = p[i + 2];
        p[i + 2] = t1;
        p[i + 3] = t0;
    }
    d->pts_ms += fh >> 17;
    *data = p;
    *size = fsize;
    *pts_ms = d->pts_ms;
    *keyframe = (p[0] & 0x80u) == 0u;
    d->video_off += fsize + 4u;
    d->video_left -= fsize + 4u;
    d->frames_left--;
    return 1;
}

int xmv_next_audio(xmv_demux *d, int track, const uint8_t **data, uint32_t *size)
{
    if (track < 0 || track >= d->audio_count || !d->aq_len[track])
        return 0;
    /* Hand the queue over and start a new one: the caller's pointer stays
     * valid until its next call for this track. */
    free(d->aq_out[track]);
    d->aq_out[track] = d->aq[track];
    *data = d->aq[track];
    *size = d->aq_len[track];
    d->aq[track] = NULL;
    d->aq_len[track] = d->aq_cap[track] = 0;
    return 1;
}
