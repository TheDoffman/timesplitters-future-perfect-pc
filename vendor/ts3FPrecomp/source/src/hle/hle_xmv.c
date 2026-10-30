/*
 * hle_xmv.c -- replacing the Xbox video library at its API boundary.
 *
 * A title that opens with a logo movie decodes it with XMV, which is linked
 * into the XBE and so is lifted and run like the title's own code. That
 * decoder is some of the least forgiving code in any image: hand-written MMX
 * inner loops fed by pointers that come from the graphics layer. Black faults
 * inside one of them within seconds of starting, having reached nothing else.
 *
 * Refusing to open the file does not help. Black treats a missing movie as
 * fatal -- it calls its own error callback, which asserts and halts -- and it
 * is right to, because on a real disc the file is always there.
 *
 * So the movie is replaced rather than decoded. The title asks the library to
 * create a playback, start it, and then poll it once a frame; this answers
 * those questions without decoding anything, reports the movie as finished
 * after a moment, and lets the title get on with loading. Not one instruction
 * of the title's decoder runs.
 *
 * Finding the boundary
 * --------------------
 * XbSymbolDatabase covers Direct3D, DirectSound and the application library,
 * not this one, so there were addresses and no names. scripts/section_calls.py
 * finds them a different way: each SDK library gets its own named section, so
 * the address range already says which functions belong to it, and the calls
 * that cross into that range from outside are its entry points. For Black
 * that is five functions out of a 163 KB section. The names below are the
 * XDK's own, deduced from what the callers do with each result, and they are
 * bound per title through config/extra_symbols/<title id>.json.
 *
 * The protocol, as the title uses it
 * ----------------------------------
 *   Create(flags, source, &handle)  non-zero return means failure, and the
 *                                   title then calls its error callback
 *   GetStreamInfo(handle, out)      copies the playback object's +0x40, +0x44
 *                                   and +0x48 into out +0, +4 and +0xC
 *   CreateAudioStream(handle, 0, 0, 0, &stream)
 *                                   Future Perfect only; answered with a NULL
 *                                   stream, which the title checks for
 *   Start(handle)                   the title sets its own playing flag after
 *   Update(handle, surface, &status, user)
 *                                   status 0 is no new frame, 1 is a frame
 *                                   ready to present (Future Perfect hands it
 *                                   to D3DDevice_UpdateOverlay), 2 is end of
 *                                   file and 3 failure; both titles leave
 *                                   their loop on 2 or 3
 *   Destroy(handle)                 the title zeroes its handle field after
 *
 * What this does not do
 * ---------------------
 * It draws nothing. The title gets a black screen for the length of the
 * movie, unless RECOMP_FMV_HOST is set, in which case the host's own decoder
 * plays the file when the title opens it and this simply keeps the title's
 * state machine happy alongside it.
 *
 * A title that reads decoded pixels back, or syncs audio to the video clock,
 * would notice. None seen so far does; if one does, the symptom will be a
 * stall rather than a fault, because every call here still answers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hle.h"
#include "hle_xmv_play.h"
#include "../kernel/xbox_memory_layout.h"

/* Playback status, as the titles' switches read it. Two titles agree, and
 * they were read separately:
 *
 *   Black's Update caller switches on the status through a four-entry jump
 *   table at 0x000C4720: 0 retries the poll, 1 rotates its frame-buffer
 *   indices and marks a frame ready, 2 and 3 both set its done flag.
 *
 *   Future Perfect's loop (0x00030542..0x00030604) calls
 *   D3DDevice_UpdateOverlay when the status is 1, presents, and leaves the
 *   loop only when the status is 2 or 3.
 *
 * So 1 is "a new frame is ready", not "finished". Until 22 Sep 2026 this
 * file reported 1 to mean finished, and Future Perfect sat in its movie loop
 * presenting black frames at 60 fps for as long as it was left running,
 * waiting for a 2 that never came. */
#define XMV_STATUS_NOFRAME   0u   /* nothing new this poll */
#define XMV_STATUS_NEWFRAME  1u   /* a decoded frame is in the surface */
#define XMV_STATUS_ENDOFFILE 2u   /* the movie is over */
#define XMV_STATUS_FAILED    3u

/* The playback object the title is handed. Only three fields are ever read by
 * it, through GetStreamInfo, and they sit where the real library puts them so
 * that a title reading them directly finds them too. The rest is ours.
 *
 * The size is set by the library entry points that are *not* replaced and
 * still run against this object. TimeSplitters: Future Perfect calls a
 * two-argument setter straight after Create that stores its argument at
 * +0x130, and the audio-stream creator (replaced below) reads +0x4C and
 * +0x130..+0x138. At 0x100 bytes that setter wrote 0x30 bytes past the end of
 * the allocation on every run. 0x200 covers the real object's extent with
 * room to spare; the fields it lands on are zero, which is what an unstarted
 * playback holds. */
#define XMV_OBJ_SIZE     0x200u
#define XMV_OFF_WIDTH    0x40u
#define XMV_OFF_HEIGHT   0x44u
/* +0x48 is the audio stream count: the XDK's GetStreamInfo copies it to the
 * caller's +0xC (XMVVIDEODESC: Width, Height, FramesPerSecond,
 * AudioStreamCount), and Breakdown loops over it creating one DirectSound
 * stream per track. It was a float 30.0 here, which Breakdown read as
 * 0x41F00000 tracks and capped at 6. Zero: the movie's sound plays on a host
 * voice (hle_xmv_play.c), so the title needs no stream of its own. */
#define XMV_OFF_AUDIO_COUNT 0x48u
/* Our own bookkeeping, past anything the library exposes. */
#define XMV_OFF_MAGIC    0xE0u
#define XMV_OFF_START_MS 0xE4u
#define XMV_OFF_FRAMES   0xE8u
#define XMV_OFF_DONE     0xECu
#define XMV_OFF_PLAY     0xF0u   /* hle_xmv_play handle, 0 when not playing */

#define XMV_MAGIC 0x584D5648u   /* 'XMVH' */

/* How long the movie is reported to last, in milliseconds.
 *
 * Zero by default, because nothing is being decoded and the title draws
 * nothing while it waits. The wait is not free either: Black polls the
 * playback in a tight loop rather than once a frame, and a one-second movie
 * cost it 17.8 million polls of a burning core to sit through a black screen.
 *
 * A few polls still report the movie playing before it ends, so a title that
 * wants to observe playback in progress sees it. RECOMP_XMV_SECONDS gives the
 * movie a real length back, for a title that needs the time to pass.
 */
static uint32_t xmv_ms(void)
{
    static long ms = -1;
    if (ms < 0) {
        const char *env = getenv("RECOMP_XMV_SECONDS");
        double secs = env ? atof(env) : 0.0;
        if (secs < 0.0) secs = 0.0;
        ms = (long)(secs * 1000.0);
    }
    return (uint32_t)ms;
}

/* Polls that report the movie playing before it is allowed to end, so the
 * title sees a beginning even when the movie has no duration. */
#define XMV_MIN_POLLS 3u

static int xmv_enabled(void)
{
    /* On unless turned off. A title whose decoder works is better served by
     * its own, so this is the switch that gives it back. */
    return xbox_EnvSwitch("RECOMP_HLE_XMV", 1);
}

/* Milliseconds since the process started. `clock()` rather than a host timer
 * so this file stays free of platform headers -- it is the only time source
 * here and a movie's length does not need better than that. */
static uint32_t xmv_now_ms(void)
{
    return (uint32_t)((clock() * 1000ull) / (unsigned long long)CLOCKS_PER_SEC);
}

static int xmv_is_ours(uint32_t handle)
{
    return handle && HLE_MEM32(handle + XMV_OFF_MAGIC) == XMV_MAGIC;
}

/* A playback this file did not make goes to the XDK's own body. XGRA creates
 * some through an entry point that is not replaced (0x000D975A, fed packets
 * by the title through two callbacks) and then polls and destroys them through
 * the same Update and Destroy as its file movies; answering those "not ours,
 * E_FAIL" would stop them dead. A title whose symbols do not name the original
 * gets E_FAIL as before. */
HLE_ORIGINAL(XMVPlaybackStart);
HLE_ORIGINAL(XMVPlaybackGetStreamInfo);
HLE_ORIGINAL(XMVPlaybackCreateAudioStream);
HLE_ORIGINAL(XMVPlaybackGetAudioStreamInfo);
HLE_ORIGINAL(XMVPlaybackGetCurrentTime);
HLE_ORIGINAL(XMVPlaybackUpdate);
HLE_ORIGINAL(XMVPlaybackDestroy);
HLE_ORIGINAL(XMVPlaybackReleaseAudioStream);
HLE_ORIGINAL(XMVPlaybackStopAudioStreams);
HLE_ORIGINAL(XMVPlaybackGetAudioStream);

#define XMV_OTHERS_TO_XDK(name)                                               \
    do {                                                                       \
        if (!xmv_is_ours(HLE_ARG(0))) {                                        \
            if (hle_original_##name) {                                         \
                HLE_CALL_ORIGINAL(name);                                       \
                return;                                                        \
            }                                                                  \
            HLE_RETURN(0x80004005u);                                           \
            return;                                                            \
        }                                                                      \
    } while (0)

/* HRESULT XMVPlaybackCreate(DWORD flags, void *source, XMVPlayback **out) */
HLE_EXPORT(XMVPlaybackCreate)
{
    uint32_t out_va = HLE_ARG(2);
    uint32_t obj;

    if (!xmv_enabled() || !out_va) {
        HLE_RETURN(0x80004005u);           /* E_FAIL: let the title decide */
        return;
    }

    obj = xbox_HeapAlloc(XMV_OBJ_SIZE, 16);
    if (!obj) {
        fprintf(stderr, "[XMV] no guest memory for a playback object\n");
        HLE_RETURN(0x8007000Eu);           /* E_OUTOFMEMORY */
        return;
    }
    memset(HLE_PTR(obj), 0, XMV_OBJ_SIZE);

    /* What the title will read back through GetStreamInfo: the console's
     * standard frame until the file says otherwise, and no audio streams. */
    HLE_MEM32(obj + XMV_OFF_WIDTH)  = 640u;
    HLE_MEM32(obj + XMV_OFF_HEIGHT) = 480u;
    HLE_MEM32(obj + XMV_OFF_AUDIO_COUNT) = 0u;

    HLE_MEM32(obj + XMV_OFF_MAGIC)    = XMV_MAGIC;
    HLE_MEM32(obj + XMV_OFF_START_MS) = 0;    /* not started yet */
    HLE_MEM32(obj + XMV_OFF_FRAMES)   = 0;
    HLE_MEM32(obj + XMV_OFF_DONE)     = 0;

    /* Play the file when it can be (hle_xmv_play.c: FFmpeg present, the file
     * found); otherwise the movie is reported over, as before. */
    {
        uint32_t w = 0, h = 0;
        int play = xmv_play_open(HLE_ARG(1), &w, &h);
        HLE_MEM32(obj + XMV_OFF_PLAY) = (uint32_t)play;
        if (play) {
            HLE_MEM32(obj + XMV_OFF_WIDTH)  = w;
            HLE_MEM32(obj + XMV_OFF_HEIGHT) = h;
        }
    }

    HLE_MEM32(out_va) = obj;

    fprintf(stderr, "[XMV] playback created at 0x%08X, %s; the title's decoder "
                    "will not run (RECOMP_HLE_XMV=0 gives it back)\n", obj,
            HLE_MEM32(obj + XMV_OFF_PLAY) ? "playing the file on the host"
                                          : "reported over at once");
    fflush(stderr);
    HLE_RETURN(0);
}

/* HRESULT XMVPlaybackStart(XMVPlayback *p) */
HLE_EXPORT(XMVPlaybackStart)
{
    uint32_t obj = HLE_ARG(0);

    XMV_OTHERS_TO_XDK(XMVPlaybackStart);

    if (xmv_is_ours(obj)) {
        HLE_MEM32(obj + XMV_OFF_START_MS) = xmv_now_ms();
        xmv_play_start((int)HLE_MEM32(obj + XMV_OFF_PLAY));
        fprintf(stderr, "[XMV] playback started; the movie is reported as "
                        "%u ms long (RECOMP_XMV_SECONDS)\n", xmv_ms());
        fflush(stderr);
    }
    HLE_RETURN(0);
}

/* void XMVPlaybackGetStreamInfo(XMVPlayback *p, XMVSTREAMINFO *out)
 *
 * The real one copies three fields; so does this. Laid out the same way, so a
 * title that reads the object directly rather than through here agrees. */
HLE_EXPORT(XMVPlaybackGetStreamInfo)
{
    uint32_t obj = HLE_ARG(0);
    uint32_t out = HLE_ARG(1);

    XMV_OTHERS_TO_XDK(XMVPlaybackGetStreamInfo);

    if (!obj || !out)
        return;
    HLE_MEM32(out + 0x0u) = HLE_MEM32(obj + XMV_OFF_WIDTH);
    HLE_MEM32(out + 0x4u) = HLE_MEM32(obj + XMV_OFF_HEIGHT);
    HLE_MEM32(out + 0xCu) = HLE_MEM32(obj + XMV_OFF_AUDIO_COUNT);
}

/* HRESULT XMVPlaybackCreateAudioStream(XMVPlayback *p, DWORD a, DWORD b,
 *                                      DWORD c, IDirectSoundStream **out)
 *
 * The XDK's own name for this entry is not known; the name describes what it
 * does. TimeSplitters: Future Perfect calls it straight after Create, with the
 * handle, three zeros and an output slot, and the library body builds a
 * DirectSound stream for the movie's audio: it reads the playback object at
 * +0x4C and +0x130..+0x138, divides by one of those fields, and calls
 * DirectSoundCreateStream with a format derived from them. Against this
 * object those fields are zero, so with the body left lifted the run ended in
 * a divide-by-zero inside DirectSound (0xC0000094 at 0x0040CA37+0x508,
 * `div edi` with edi loaded from a byte at +0x64 of the half-built stream)
 * the moment the movie was opened.
 *
 * Nothing here decodes audio, so there is no stream to hand back. The title
 * allows for that: it tests the output slot for zero before calling
 * IDirectSoundStream_SetVolume on it, and again before Flush and Release when
 * the movie ends. So the answer is "no audio stream", written as NULL, and
 * S_OK, which the title does not look at. */
HLE_EXPORT(XMVPlaybackCreateAudioStream)
{
    uint32_t obj = HLE_ARG(0);
    uint32_t out = HLE_ARG(4);

    XMV_OTHERS_TO_XDK(XMVPlaybackCreateAudioStream);

    if (out)
        HLE_MEM32(out) = 0;
    if (xmv_is_ours(obj)) {
        fprintf(stderr, "[XMV] audio stream requested for playback 0x%08X; "
                        "none is made: %s\n", obj,
                HLE_MEM32(obj + XMV_OFF_PLAY) ? "the movie's sound plays on a host voice"
                                              : "the movie is not being played");
        fflush(stderr);
    }
    HLE_RETURN(0);
}

/* HRESULT XMVPlaybackGetAudioStreamInfo(XMVPlayback *p, DWORD index, void *out)
 *
 * Breakdown asks this once per audio stream GetStreamInfo reported. That count
 * is 0 now, so it should not be asked; if a title asks anyway, there is no
 * such stream. */
HLE_EXPORT(XMVPlaybackGetAudioStreamInfo)
{

    XMV_OTHERS_TO_XDK(XMVPlaybackGetAudioStreamInfo);
    if (xmv_is_ours(HLE_ARG(0))) {
        static int said;
        if (!said++)
            fprintf(stderr, "[XMV] audio stream %u info requested: there are none "
                            "(the movie's sound plays on a host voice)\n", HLE_ARG(1));
    }
    HLE_RETURN(0x80004005u);
}

/* DWORD XMVPlaybackGetCurrentTime(XMVPlayback *p)
 *
 * Breakdown calls this after every Update. The XDK's body subtracts a start
 * time kept in the real playback object from QueryPerformanceCounter, and in
 * this object that field is zero, so it would report the time since boot.
 *
 * For a movie played here, the host player's own clock -- the one its
 * pictures' times (Update's fourth argument) are on. Otogi waits after each
 * picture until this reaches that picture's time, and never calls Start, so
 * a clock kept from Start stood at zero and Otogi waited for ever. */
HLE_EXPORT(XMVPlaybackGetCurrentTime)
{
    uint32_t obj = HLE_ARG(0), started;

    XMV_OTHERS_TO_XDK(XMVPlaybackGetCurrentTime);

    if (!xmv_is_ours(obj)) {
        HLE_RETURN(0);
        return;
    }
    if (HLE_MEM32(obj + XMV_OFF_PLAY)) {
        HLE_RETURN(xmv_play_time((int)HLE_MEM32(obj + XMV_OFF_PLAY)));
        return;
    }
    started = HLE_MEM32(obj + XMV_OFF_START_MS);
    HLE_RETURN(started ? xmv_now_ms() - started : 0u);
}

/* HRESULT XMVPlaybackUpdate(XMVPlayback *p, void *surface, DWORD *status,
 *                           void *user)
 *
 * Polled once a frame. Reports the movie playing until its time is up and
 * finished after that, which is the transition the title waits for. */
HLE_EXPORT(XMVPlaybackUpdate)
{
    uint32_t obj       = HLE_ARG(0);
    uint32_t status_va = HLE_ARG(2);
    uint32_t started, frames, elapsed;

    XMV_OTHERS_TO_XDK(XMVPlaybackUpdate);

    started = HLE_MEM32(obj + XMV_OFF_START_MS);
    frames  = HLE_MEM32(obj + XMV_OFF_FRAMES) + 1u;
    HLE_MEM32(obj + XMV_OFF_FRAMES) = frames;

    if (HLE_MEM32(obj + XMV_OFF_PLAY)) {
        uint32_t pts = 0, time_va = HLE_ARG(3);
        uint32_t status = xmv_play_update((int)HLE_MEM32(obj + XMV_OFF_PLAY), HLE_ARG(1), &pts);
        if (status_va)
            HLE_MEM32(status_va) = status;
        /* The fourth argument, when the title passes one, receives the new
         * picture's time: the XDK writes [playback +0xE4] + [+0xC8] through
         * it whenever it is non-NULL and a picture is new. Future Perfect and
         * Breakdown pass 0; XGRA passes a local and draws only when it comes
         * back non-zero, so the first picture (at 0 ms) is reported as 1. */
        if (status == XMV_STATUS_NEWFRAME && time_va >= 0x00010000u &&
            (uint64_t)time_va + 4u <= g_xbox_total_ram)
            HLE_MEM32(time_va) = pts + 1u;
        if (status == XMV_STATUS_ENDOFFILE && !HLE_MEM32(obj + XMV_OFF_DONE)) {
            HLE_MEM32(obj + XMV_OFF_DONE) = 1u;
            fprintf(stderr, "[XMV] update #%u: the movie is over\n", frames);
            fflush(stderr);
        }
        HLE_RETURN(0);
        return;
    }

    if (!started) {
        /* Polled before Start. Treat the first poll as the start so a title
         * that never calls Start still finishes. */
        started = xmv_now_ms();
        HLE_MEM32(obj + XMV_OFF_START_MS) = started;
    }
    elapsed = xmv_now_ms() - started;

    if (status_va) {
        /* No frame is ever decoded, so no poll reports NEWFRAME: a title
         * given 1 would present whatever its surface holds. */
        uint32_t status = (frames > XMV_MIN_POLLS && elapsed >= xmv_ms())
                        ? XMV_STATUS_ENDOFFILE : XMV_STATUS_NOFRAME;
        HLE_MEM32(status_va) = status;

        if (frames <= 3u || (status == XMV_STATUS_ENDOFFILE
                             && !HLE_MEM32(obj + XMV_OFF_DONE))) {
            fprintf(stderr, "[XMV] update #%u: status %u -> 0x%08X "
                            "(elapsed %u ms, surface 0x%08X)\n",
                    frames, status, status_va, elapsed, HLE_ARG(1));
            fflush(stderr);
        }
        if (status == XMV_STATUS_ENDOFFILE)
            HLE_MEM32(obj + XMV_OFF_DONE) = 1u;
    }
    HLE_RETURN(0);
}

/* HRESULT XMVPlaybackGetAudioStream(XMVPlayback *p, DWORD index,
 *                                   IDirectSoundStream **out)
 *
 * Nightfire's (XDK 4831) way of fetching the DirectSound stream for one audio
 * track after creating it. A playback made here has none -- its sound is on a
 * host voice -- so the answer is NULL, which is also what CreateAudioStream
 * hands back. */
HLE_EXPORT(XMVPlaybackGetAudioStream)
{
    uint32_t out = HLE_ARG(2);

    XMV_OTHERS_TO_XDK(XMVPlaybackGetAudioStream);
    if (out)
        HLE_MEM32(out) = 0;
    HLE_RETURN(0);
}

/* void XMVPlaybackStopAudioStreams(XMVPlayback *p)
 * void XMVPlaybackReleaseAudioStream(XMVPlayback *p, DWORD index)
 *
 * XGRA's names for two calls its movie code makes before Destroy: the first
 * walks the playback's DirectSound streams and calls a method on each, the
 * second clears one slot of the stream array the playback keeps at +0x138.
 * A playback made here has no DirectSound streams -- its sound is on a host
 * voice, closed with the movie -- and no such array, so both are done. */
HLE_EXPORT(XMVPlaybackStopAudioStreams)
{
    XMV_OTHERS_TO_XDK(XMVPlaybackStopAudioStreams);
    HLE_RETURN(0);
}

HLE_EXPORT(XMVPlaybackReleaseAudioStream)
{
    XMV_OTHERS_TO_XDK(XMVPlaybackReleaseAudioStream);
    HLE_RETURN(0);
}

/* HRESULT XMVPlaybackDestroy(XMVPlayback *p) */
HLE_EXPORT(XMVPlaybackDestroy)
{
    uint32_t obj = HLE_ARG(0);

    XMV_OTHERS_TO_XDK(XMVPlaybackDestroy);

    if (xmv_is_ours(obj)) {
        /* The guest heap here has no free, so the object is neutered rather
         * than returned: a title that keeps a stale handle then gets the same
         * answer as one that passes a handle we never made. */
        xmv_play_close((int)HLE_MEM32(obj + XMV_OFF_PLAY));
        HLE_MEM32(obj + XMV_OFF_PLAY) = 0;
        HLE_MEM32(obj + XMV_OFF_MAGIC) = 0;
        fprintf(stderr, "[XMV] playback destroyed\n");
        fflush(stderr);
    }
    HLE_RETURN(0);
}
