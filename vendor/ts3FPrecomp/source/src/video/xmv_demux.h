/*
 * xmv_demux.h -- reading the Xbox's XMV movie container.
 *
 * The layout is FFmpeg's (libavformat/xmv.c); docs/technical/video-playback.md
 * has it in prose. Portable C: the file is read with stdio and nothing here
 * touches guest memory or a host graphics API.
 */
#ifndef XBOXRECOMP_XMV_DEMUX_H
#define XBOXRECOMP_XMV_DEMUX_H

#include <stdint.h>
#include <stdio.h>

#define XMV_MAX_AUDIO 4

typedef struct xmv_audio_track {
    uint16_t codec;          /* 0x0001 PCM, 0x0069 Xbox ADPCM */
    uint16_t channels;
    uint32_t rate;
    uint16_t bits;
    uint16_t flags;
} xmv_audio_track;

typedef struct xmv_demux {
    FILE    *f;
    uint32_t version;
    uint32_t width, height, duration_ms;
    int      audio_count;
    xmv_audio_track audio[XMV_MAX_AUDIO];
    uint8_t  extradata[4];   /* WMV2 codec data, big-endian, as decoders want it */
    int      have_extradata;

    /* The packet being read. */
    uint8_t *packet;         /* whole packet, read at once */
    uint32_t packet_size, next_packet_size;
    uint32_t video_off, video_left;     /* next frame in `packet` */
    uint32_t frames_left;
    uint32_t audio_size[XMV_MAX_AUDIO];            /* this packet's, per track */
    /* Audio read so far and not yet handed out, per track. A packet can hold
     * audio and no video, so audio is queued as packets load rather than
     * handed out a packet at a time. */
    uint8_t *aq[XMV_MAX_AUDIO];
    uint32_t aq_len[XMV_MAX_AUDIO], aq_cap[XMV_MAX_AUDIO];
    uint8_t *aq_out[XMV_MAX_AUDIO];                /* what the last call returned */
    uint32_t pts_ms;         /* running video timestamp */
    int      eof;
} xmv_demux;

/* 0 on success. The file stays open until xmv_close. xmv_open_file takes a
 * file the caller opened (a wide path on Windows) and owns it from then on,
 * closing it on failure too. */
int  xmv_open(xmv_demux *d, const char *path);
int  xmv_open_file(xmv_demux *d, FILE *f);
void xmv_close(xmv_demux *d);

/* The next video frame, byte-swapped into a normal WMV2 bitstream. *data
 * points into a buffer the demuxer owns, valid until the next call. Returns 1
 * with a frame, 0 at the end of the file, -1 on a malformed file. Every
 * packet read on the way queues its audio for xmv_next_audio. */
int  xmv_next_video(xmv_demux *d, const uint8_t **data, uint32_t *size,
                    uint32_t *pts_ms, int *keyframe);

/* All audio for `track` queued since the last call: 1 with data (valid until
 * the next call for that track), 0 when there is none. */
int  xmv_next_audio(xmv_demux *d, int track, const uint8_t **data, uint32_t *size);

#endif
