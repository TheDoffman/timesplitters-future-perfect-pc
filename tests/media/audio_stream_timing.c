/* Execute the real stream adapter with deterministic time and a fake device.
 * No game files, audio hardware, wall-clock sleeps or personal settings. */
#include "platform/xbox_winnt.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t test_now;
static uint64_t test_clock(void) { return test_now; }
#define GetTickCount64 test_clock
#include "hle/hle_dsound_stream.c"
#include "hle/xbox_adpcm.c"
#undef GetTickCount64

RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
static uint8_t memory[1024 * 1024];
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_total_ram = sizeof memory;
static struct { int available; uint64_t played; uint32_t queued; } host;
static uint8_t captured[160000];
static int refuse_output;
static uint32_t captured_bytes;
static uint32_t release_result;
static void original_release(void) { g_eax = release_result; }
void (*const hle_original_CDirectSoundStream_Release)(void) = original_release;
void (*const hle_original_CDirectSoundStream_FlushEx)(void) = NULL;

static int capture;

recomp_func_t recomp_lookup(uint32_t address) { (void)address; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t address) { (void)address; return NULL; }
long __stdcall xbox_NtSetEvent(void *handle, long *previous) { return 0; }
void *xbox_bridge_resolve_handle(uint32_t token) { return NULL; }
int xbox_EnvSwitch(const char *name, int default_on) { return default_on; }
int32_t hle_dsound_stream_adjust(uint32_t object, int *is3d) { return 0; }
static int host_discontinuity;
int recomp_audio_output_take_discontinuity(uint32_t slot) { int v=host_discontinuity; host_discontinuity=0; return v; }
void recomp_audio_output_reset_voice(uint32_t slot) { memset(&host, 0, sizeof host); }
int recomp_audio_output_position(uint32_t slot, uint64_t *played, uint32_t *queued)
{
    if (!host.available) return 0;
    if (played) *played = host.played;
    if (queued) *queued = host.queued;
    return 1;
}
int recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t rate, uint32_t channels, uint32_t bits, int32_t level)
{
    if (refuse_output) return 0;
    host.available = 1;
    host.queued++;
    if (capture && captured_bytes + bytes <= sizeof captured) {
        memcpy(captured + captured_bytes, pcm, bytes);
        captured_bytes += bytes;
    }
    return 1;
}
int recomp_audio_output_set_volume(uint32_t slot, int32_t level) { return 1; }
void (*const hle_original_DirectSoundCreateStream)(void) = NULL;
void (*const hle_original_CDirectSound_CreateSoundStream)(void) = NULL;
void (*const hle_original_CDirectSoundStream_SetVolume)(void) = NULL;
void (*const hle_original_CDirectSoundStream_SetFormat)(void) = NULL;
void (*const hle_original_CDirectSoundStream_SetFrequency)(void) = NULL;

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static Stream *setup(void)
{
    Stream *s = &g_streams[0];
    memset(g_streams, 0, sizeof g_streams);
    memset(memory, 0, sizeof memory);
    memset(&host, 0, sizeof host);
    captured_bytes = 0; capture = 0;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_esp = 0x10000;
    test_now = 1000;
    g_wallclock = 0;
    s->iface = 0x11020; s->object = 0x11000;
    s->tag = TAG_PCM; s->rate = 48000; s->channels = 1; s->bits = 16;
    s->align = 2; s->max_packets = MAX_PACKETS; s->output = 1;
    return s;
}

static void packet(Stream *s, uint32_t packet_va, uint32_t status_va)
{
    HLE_MEM32(g_esp + 4) = s->object;
    HLE_MEM32(g_esp + 8) = packet_va;
    HLE_MEM32(packet_va) = 0x20000;
    HLE_MEM32(packet_va + 4) = 96000; /* one second */
    HLE_MEM32(packet_va + 12) = status_va;
    hle_CDirectSoundStream_Process();
}

static int underrun(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    CHECK(s->sent == 38400); /* only 400 ms has reached the device */
    host.played = s->sent; host.queued = 0;
    test_now = 6000; /* loading stalled the game for five seconds */
    tick(s, test_now);
    CHECK(s->count == 1);
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
    CHECK(s->sent > 38400); /* resume feeding the unsent part */
    CHECK(consumed(s, test_now) == 38400);
    return 0;
}

static int process_clock(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 9600; host.queued = 3;
    test_now = 2200; /* host heard 100 ms, game thread stalled 1200 ms */
    packet(s, 0x12040, 0x13004);
    CHECK(s->count == 2);
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
    CHECK(consumed(s, test_now) == 9600);
    return 0;
}

static int flush_origin(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    HLE_MEM32(g_esp + 4) = s->object;
    hle_CDirectSoundStream_Flush();
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_FLUSHED);
    test_now = 1100;
    packet(s, 0x12040, 0x13004);
    host.played = 9600; host.queued = 3;
    test_now = 1200;
    tick(s, test_now);
    CHECK(consumed(s, test_now) == 105600);
    CHECK(s->count == 1);
    return 0;
}

static int fallback(void)
{
    Stream *s = setup();
    s->output = 0; /* unsupported format or disabled backend */
    packet(s, 0x12000, 0x13000);
    test_now = 2000;
    tick(s, test_now);
    CHECK(s->count == 0);
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_SUCCESS);
    return 0;
}

static int no_extrapolation(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 9600; host.queued = 3;
    test_now = 1200;
    tick(s, test_now);
    CHECK(consumed(s, 9000) == 9600);
    return 0;
}

static int pause_resume(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 9600; host.queued = 3;
    test_now = 2300;
    HLE_MEM32(g_esp + 4) = s->object;
    HLE_MEM32(g_esp + 8) = DSSTREAMPAUSE_PAUSE;
    hle_CDirectSoundStream_Pause();
    CHECK(s->paused && s->consumed_base == 9600);
    test_now = 4000;
    CHECK(consumed(s, test_now) == 9600);
    HLE_MEM32(g_esp + 8) = DSSTREAMPAUSE_RESUME;
    hle_CDirectSoundStream_Pause();
    CHECK(!s->paused && s->play_origin == 9600);
    host.played = 9600; host.queued = 3;
    tick(s, 4100);
    CHECK(consumed(s, 4100) == 19200);
    return 0;
}

static int completion(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 95998; host.queued = 1;
    tick(s, 3000);
    CHECK(s->count == 1);
    host.played = 96000; host.queued = 0;
    tick(s, 3010);
    CHECK(s->count == 0 && HLE_MEM32(0x13000) == XMP_STATUS_SUCCESS);
    return 0;
}

static int lost_device(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 9600; host.queued = 3;
    tick(s, 1200);
    host.available = 0;
    tick(s, 3000);
    CHECK(s->count == 0);
    return 0;
}

static int bad_data_fallback(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 38400; host.queued = 1;
    s->packets[0].data = sizeof memory + 100;
    tick(s, 1200);
    CHECK(!s->output);
    tick(s, 3000);
    CHECK(s->count == 0);
    return 0;
}

static int diagnostics(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    host.played = 9600; host.queued = 3;
    tick(s, 1100);
    host.played = 19200;
    tick(s, 1200);
    CHECK(s->drift_ms == 0); /* device and wall clock both advanced 100 ms */
    return 0;
}

static int adpcm_resume(void)
{
    Stream *s = setup();
    int16_t expected[192];
    s->tag = TAG_ADPCM; s->bits = 4; s->align = 36;
    s->resumed_ms = 1000; s->queued = 384; s->sent = 2; s->consumed_base = 2;
    s->count = 1;
    s->packets[0].data = 0x20000; s->packets[0].size = 108;
    s->packets[0].start = 0; s->packets[0].end = 384;
    for (int i = 0; i < 3; i++) {
        uint8_t *block = memory + 0x20000 + i * 36;
        memset(block, 0x11, 36);
        block[0] = (uint8_t)(i * 64); block[1] = block[2] = block[3] = 0;
        CHECK(xbox_adpcm_decode_block(block, 36, 1, expected + i * 64, 64));
    }
    capture = 1;
    pump(s, 1000); /* resume one sample into an ADPCM block */
    CHECK(s->sent == s->queued);
    CHECK(captured_bytes == 382);
    CHECK(!memcmp(captured, expected + 1, 382));
    return 0;
}


/* The release wrapper must preserve shared references, then cancel the old
 * host queue before the allocator can reuse its packet storage and interface. */
static int release_reuse(void)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    HLE_MEM32(g_esp + 4) = s->object;
    release_result = 1;
    hle_CDirectSoundStream_Release();
    CHECK(g_eax == 1 && s->count == 1 && host.available);
    release_result = 0;
    hle_CDirectSoundStream_Release();
    CHECK(g_eax == 0 && s->iface == 0 && !host.available);
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_FLUSHED);
    /* Reused memory is no longer audio. No tick may read or submit it. */
    memset(memory + 0x20000, 0xCD, 96000);
    hle_dsound_stream_tick(2000);
    CHECK(!host.available);
    /* Reusing the same interface must register a new format and empty queue. */
    HLE_MEM32(0x14000 + 4) = 3;
    HLE_MEM32(0x14000 + 8) = 0x15000;
    HLE_MEM32(0x15000) = TAG_ADPCM | (2u << 16);
    HLE_MEM32(0x15004) = 32000;
    HLE_MEM32(0x1500C) = 72 | (4u << 16);
    record_stream(0x11020, 0x14000);
    CHECK(s->iface == 0x11020 && s->rate == 32000 && s->channels == 2);
    CHECK(s->count == 0 && s->queued == 0 && s->sent == 0);
    return 0;
}

static int flush_ex(int async)
{
    Stream *s = setup();
    packet(s, 0x12000, 0x13000);
    HLE_MEM32(g_esp + 4) = s->object;
    HLE_MEM32(g_esp + 8) = HLE_MEM32(g_esp + 12) = 0;
    HLE_MEM32(g_esp + 16) = async;
    hle_CDirectSoundStream_FlushEx();
    CHECK(g_eax == 0 && !host.available);
    if (async) {
        CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
        /* Status/process calls must not complete cancelled audio using a
         * wall-clock fallback while the asynchronous flush awaits DoWork. */
        test_now = 5000;
        HLE_MEM32(g_esp + 8) = 0x13010;
        hle_CDirectSoundStream_GetStatus__r2();
        CHECK(!(HLE_MEM32(0x13010) & DSSTREAMSTATUS_READY));
        CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
        packet(s, 0x12040, 0x13004);
        CHECK(g_eax == HR_E_FAIL && s->count == 1);
        CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
        hle_dsound_stream_tick(5100);
    }
    CHECK(s->count == 0 && HLE_MEM32(0x13000) == XMP_STATUS_FLUSHED);
    CHECK(!host.available);
    test_now = 1200;
    packet(s, 0x12040, 0x13004);
    CHECK(s->count == 1 && host.available);
    host.played = 9600;
    tick(s, 1300);
    CHECK(consumed(s, 1300) == 105600);
    return 0;
}

/* A format-rate change must preserve the exact unheard PCM/ADPCM suffix. */
static int format_rate(int adpcm, int paused, int frequency)
{
    Stream *s = setup();
    uint32_t p = 0x12000, wfx = 0x14000;
    int16_t expected_samples[512];
    uint8_t *expected = (uint8_t *)expected_samples;
    uint32_t total = 1024, heard = 256;
    if (adpcm) {
        s->tag = TAG_ADPCM; s->bits = 4; s->align = 36;
        total = 384; heard = 2;
        for (int i = 0; i < 3; i++) {
            uint8_t *block = memory + 0x20000 + i * 36;
            memset(block, 0x11, 36);
            block[0] = (uint8_t)(i * 64);
            block[1] = block[2] = block[3] = 0;
            CHECK(xbox_adpcm_decode_block(block, 36, 1, expected_samples + i * 64, 64));
        }
    } else {
        memset(memory + 0x20000, 0x11, heard);
        memset(memory + 0x20000 + heard, 0x77, total - heard);
        memcpy(expected, memory + 0x20000, total);
    }
    HLE_MEM32(g_esp + 4) = s->object; HLE_MEM32(g_esp + 8) = p;
    HLE_MEM32(p) = 0x20000; HLE_MEM32(p + 4) = adpcm ? 108 : total;
    HLE_MEM32(p + 8) = 0x13004; HLE_MEM32(p + 12) = 0x13000;
    hle_CDirectSoundStream_Process();
    host.played = heard; test_now = 1003;
    if (paused) {
        HLE_MEM32(g_esp + 8) = DSSTREAMPAUSE_PAUSE;
        hle_CDirectSoundStream_Pause();
    }
    HLE_MEM32(wfx) = s->tag | (s->channels << 16);
    HLE_MEM32(wfx + 4) = 24000;
    HLE_MEM32(wfx + 12) = s->align | (s->bits << 16);
    capture = 1; captured_bytes = 0;
    HLE_MEM32(g_esp + 8) = frequency ? 24000 : wfx;
    if (frequency) hle_CDirectSoundStream_SetFrequency();
    else hle_CDirectSoundStream_SetFormat();
    CHECK(g_eax == 0 && s->queued == total);
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
    if (paused) {
        CHECK(captured_bytes == 0);
        HLE_MEM32(g_esp + 8) = DSSTREAMPAUSE_RESUME;
        hle_CDirectSoundStream_Pause();
    }
    CHECK(captured_bytes == total - heard);
    CHECK(!memcmp(captured, expected + heard, total - heard));
    host.played = total - heard; host.queued = 0;
    tick(s, 1100);
    CHECK(s->count == 0 && HLE_MEM32(0x13000) == XMP_STATUS_SUCCESS);
    CHECK(HLE_MEM32(0x13004) == (adpcm ? 108 : total));
    return 0;
}

static int device_recovery(int early)
{
    Stream *s = setup();
    memset(memory + 0x20000, 0x11, 48000);
    memset(memory + 0x20000 + 48000, 0x77, 48000);
    packet(s, 0x12000, 0x13000);
    if (!early) {
        host.played = 9600;
        tick(s, 1100);
        CHECK(s->consumed_base == 9600);
    }
    host.available = 0; host_discontinuity = 1; refuse_output = 1;
    tick(s, 1200);
    CHECK(s->output_lost && s->sent == 19200);
    CHECK(HLE_MEM32(0x13000) == XMP_STATUS_PENDING);
    capture = 1; captured_bytes = 0; refuse_output = 0;
    tick(s, 1500);
    CHECK(!s->output_lost && s->play_origin == 48000);
    CHECK(captured_bytes == 38400 && captured[0] == 0x77);
    CHECK(s->sent == 86400);
    host.played = 9600;
    tick(s, 1600);
    CHECK(consumed(s, 1600) == 57600);
    host.played = 48000; host.queued = 0;
    tick(s, 2000);
    CHECK(s->count == 0 && HLE_MEM32(0x13000) == XMP_STATUS_SUCCESS);
    return 0;
}

int main(int argc, char **argv)
{
    int result;
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "underrun")) result = underrun();
    else if (!strcmp(argv[1], "process")) result = process_clock();
    else if (!strcmp(argv[1], "flush")) result = flush_origin();
    else if (!strcmp(argv[1], "fallback")) result = fallback();
    else if (!strcmp(argv[1], "snapshot")) result = no_extrapolation();
    else if (!strcmp(argv[1], "pause")) result = pause_resume();
    else if (!strcmp(argv[1], "completion")) result = completion();
    else if (!strcmp(argv[1], "lost-device")) result = lost_device();
    else if (!strcmp(argv[1], "bad-data")) result = bad_data_fallback();
    else if (!strcmp(argv[1], "diagnostics")) result = diagnostics();
    else if (!strcmp(argv[1], "adpcm")) result = adpcm_resume();
    else if (!strcmp(argv[1], "release-reuse")) result = release_reuse();
    else if (!strcmp(argv[1], "flush-ex")) result = flush_ex(0);
    else if (!strcmp(argv[1], "flush-ex-async")) result = flush_ex(1);
    else if (!strcmp(argv[1], "format-pcm")) result = format_rate(0, 0, 0);
    else if (!strcmp(argv[1], "format-adpcm")) result = format_rate(1, 0, 0);
    else if (!strcmp(argv[1], "format-paused")) result = format_rate(0, 1, 0);
    else if (!strcmp(argv[1], "frequency")) result = format_rate(0, 0, 1);
    else if (!strcmp(argv[1], "recover-device")) result = device_recovery(0);
    else if (!strcmp(argv[1], "recover-early")) result = device_recovery(1);
    else return 2;
    if (!result) printf("PASS: %s\n", argv[1]);
    return result;
}
