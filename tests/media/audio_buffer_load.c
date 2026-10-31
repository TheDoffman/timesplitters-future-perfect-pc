/* Actual shared effects pump with 64 voices and a deterministic device clock. */
#define main buffer_backpressure_main
#include "audio_buffer_timing.c"
#undef main

static int load_case(const char *scenario)
{
    const unsigned voices = 64;
    const int tail = !strcmp(scenario, "tail");
    const int expired = !strcmp(scenario, "expired");
    const int legacy = !strcmp(scenario, "legacy");
    const int fixed = !strcmp(scenario, "fixed");
    const int stalled = !strcmp(scenario, "stall") || !strcmp(scenario, "stall-refused") || tail || expired;
    const int refused = !strcmp(scenario, "stall-refused");
    const int steady = !strcmp(scenario, "steady");
    unsigned i, tick;
    uint64_t now = 1000;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_lead_ms = 40;
    if (fixed || legacy) {
        g_lead_ms = UINT32_MAX;
        CHECK(lead_ms() == (legacy ? 0 : 40));
    }
    g_layout = 0;
    fake_device = 1;
    fake_now = now;
    accept = 1;
    for (i = 0; i < 192000; ++i) memory[0x40000 + i] = (uint8_t)(i / 96);
    for (i = 0; i < voices; ++i) {
        Buffer *b = &g_buffers[i];
        uint32_t base = 0x10000 + i * 0x300;
        b->iface = base + 0x20;
        b->data = 0x40000;
        b->size = tail ? 28800 : expired ? 9600 : 192000;
        b->format = 1u | (1u << 16) | (16u << 24);
        b->output = 1;
        HLE_MEM32(b->iface) = base + 0x100;
        HLE_MEM32(b->iface - 0xC) = base + 0x200;
        HLE_MEM32(base + 0x100 + SET_DATA) = b->data;
        HLE_MEM32(base + 0x100 + SET_SIZE) = b->size;
        HLE_MEM32(base + 0x200 + SET_FORMAT) = b->format;
        CHECK(recomp_dsound_buffer_configure(&b->model, b->size, 48000, 2, now) == 0);
        CHECK(recomp_dsound_buffer_play(&b->model, (tail || expired) ? 0 : RECOMP_DSOUND_PLAY_LOOPING, now) == 0);
        b->output_model = b->model;
        pump(b, now);
        CHECK(fake_until[i] == (legacy ? 0 : 1040)); /* new sounds still start immediately */
    }
    if (legacy) {
        fake_now = 1050;
        pump(&g_buffers[0], fake_now);
        CHECK(accepted_bytes == 4800);
        fake_now = 1250;
        pump(&g_buffers[0], fake_now);
        CHECK(accepted_bytes == 4800); /* explicit zero retains old discard behavior */
        CHECK(g_buffers[0].output_model.last_ms == 1250);
        puts("PASS: legacy");
        return 0;
    }
    if (stalled) {
        RecompDsoundBufferModel before = g_buffers[0].output_model;
        now += 150;
        fake_now = now;
        if (refused) {
            accept = 0;
            pump(&g_buffers[0], now);
            CHECK(!memcmp(&before, &g_buffers[0].output_model, sizeof before));
            accept = 1;
            now += 10;
            fake_now = now;
        }
        for (i = 0; i < voices; ++i) {
            pump(&g_buffers[i], now);
            if (expired) {
                CHECK(fake_until[i] <= now);
                CHECK(!g_buffers[i].output_model.playing);
                continue;
            }
            CHECK(fake_until[i] > now); /* refill during this very update */
            CHECK(first_byte == (uint8_t)(now - 1000)); /* skip stale past only */
        }
    }
    if (tail || expired) {
        fake_now = 1230;
        for (i = 0; i < voices; ++i) {
            pump(&g_buffers[i], fake_now);
            CHECK(!g_buffers[i].output_model.playing);
            CHECK(fake_until[i] == (tail ? 1300 : 1040));
        }
        printf("PASS: %s\n", scenario);
        return 0;
    }
    for (tick = 0; tick < 120; ++tick) {
        now += steady ? 17 : 60;
        fake_now = now;
        for (i = 0; i < voices; ++i) {
            pump(&g_buffers[i], now);
            CHECK(fake_until[i] > now);
            CHECK(fake_until[i] - now <= 180); /* no ever-growing audio backlog */
        }
    }
    if (fixed) CHECK(g_stat.underruns > voices); /* explicit 40ms does not adapt */
    else CHECK(g_stat.underruns <= (steady ? 0 : voices + refused));
    printf("PASS: %s (%llu empty-queue observations)\n", scenario,
           (unsigned long long)g_stat.underruns);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    return load_case(argv[1]);
}
