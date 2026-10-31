/* Real buffer lookup/DoWork with invalidated guest settings and counted resets. */
#define main existing_buffer_timing_main
#include "audio_buffer_timing.c"
#undef main

static uint32_t make_buffer(unsigned i)
{
    uint32_t base = 0x10000 + i * 0x300, iface = base + 0x20;
    HLE_MEM32(iface) = base + 0x100;
    HLE_MEM32(iface - 0xC) = base + 0x200;
    HLE_MEM32(base + 0x100 + SET_DATA) = 0x50000;
    HLE_MEM32(base + 0x100 + SET_SIZE) = 9600;
    HLE_MEM32(base + 0x200 + SET_FORMAT) = 1u | (1u << 16) | (16u << 24);
    HLE_MEM32(base + 0x200 + SET_RATE) = 48000;
    HLE_MEM32(base + 0x200 + SET_ALIGN) = 2;
    return iface;
}

static void invalidate(uint32_t iface, const char *mode)
{
    if (!strcmp(mode, "null-buffer")) HLE_MEM32(iface) = 0;
    else if (!strcmp(mode, "null-voice")) HLE_MEM32(iface - 0xC) = 0;
    else if (!strcmp(mode, "bad-pointer")) HLE_MEM32(iface) = 0x70000000;
    else if (!strcmp(mode, "bad-rate")) HLE_MEM32(HLE_MEM32(iface - 0xC) + SET_RATE) = 0;
    else if (!strcmp(mode, "high-rate")) HLE_MEM32(HLE_MEM32(iface - 0xC) + SET_RATE) = 200001;
    else if (!strcmp(mode, "bad-align")) HLE_MEM32(HLE_MEM32(iface - 0xC) + SET_ALIGN) = 0;
    else if (!strcmp(mode, "empty-size")) HLE_MEM32(HLE_MEM32(iface) + SET_SIZE) = 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_esp = 0x48000;
    g_lead_ms = 40;
    accept = 1;
    if (!strcmp(argv[1], "reuse")) {
        /* More distinct guest objects than the fixed host slot capacity. */
        for (unsigned cycle = 0; cycle < 4; ++cycle) {
            for (unsigned i = 0; i < SLOTS; ++i) {
                uint32_t iface = make_buffer(i);
                Buffer *b = model_for(iface, 1000);
                CHECK(b && b->iface == iface);
                HLE_MEM32(iface) = 0;
            }
            hle_DirectSoundDoWork();
            for (unsigned i = 0; i < SLOTS; ++i) {
                CHECK(g_buffers[i].iface == 0);
                CHECK(reset_count[i] == cycle + 1);
            }
        }
        uint32_t iface = make_buffer(SLOTS);
        Buffer *b = model_for(iface, 2000);
        CHECK(b);
        CHECK(recomp_dsound_buffer_play(&b->model, 0, 2000) == 0);
        b->output_model = b->model;
        pump(b, 2000);
        CHECK(accepted_bytes == 3840); /* recovered slot actually emits PCM */
    } else if (!strcmp(argv[1], "retain-valid")) {
        for (unsigned i = 0; i < SLOTS; ++i) {
            Buffer *b = model_for(make_buffer(i), now_ms());
            CHECK(b);
            if (i == 0) {
                recomp_dsound_buffer_play(&b->model, RECOMP_DSOUND_PLAY_LOOPING, now_ms());
                b->output_model = b->model;
            } else if (i == 1) {
                b->paused = 1;
                b->paused_flags = RECOMP_DSOUND_PLAY_LOOPING;
            }
        }
        hle_DirectSoundDoWork();
        for (unsigned i = 0; i < SLOTS; ++i) {
            CHECK(g_buffers[i].iface == 0x10020 + i * 0x300);
            CHECK(reset_count[i] == 0);
        }
        CHECK(g_buffers[0].model.playing && g_buffers[1].paused);
        CHECK(model_for(make_buffer(SLOTS), now_ms()) == NULL); /* no valid eviction */
        CHECK(reset_count[0] == 0 && reset_count[1] == 0);
    } else if (!strcmp(argv[1], "active-invalid")) {
        uint32_t iface = make_buffer(0);
        Buffer *b = model_for(iface, now_ms());
        CHECK(b);
        recomp_dsound_buffer_play(&b->model, RECOMP_DSOUND_PLAY_LOOPING, now_ms());
        b->output_model = b->model;
        pump(b, now_ms());
        CHECK(accepted_bytes > 0 && b->fed);
        unsigned before = accepted_bytes;
        HLE_MEM32(iface) = 0;
        hle_DirectSoundDoWork();
        CHECK(b->iface == 0 && !b->model.playing && !b->fed);
        CHECK(reset_count[0] == 1 && accepted_bytes == before);
        hle_DirectSoundDoWork();
        CHECK(reset_count[0] == 1 && accepted_bytes == before);
    } else {
        uint32_t iface = make_buffer(0);
        Buffer *b = model_for(iface, 1000);
        CHECK(b);
        /* A stopped buffer still owns a host voice until retirement. */
        b->fed = 1;
        b->paused = 1;
        invalidate(iface, argv[1]);
        CHECK(model_for(iface, 1100) == NULL);
        CHECK(b->iface == 0 && !b->fed && !b->paused);
        CHECK(reset_count[0] == 1);
        CHECK(model_for(iface, 1200) == NULL);
        hle_DirectSoundDoWork();
        CHECK(reset_count[0] == 1); /* already cleared: no double release */
        CHECK(model_for(make_buffer(1), 1300) == b);
    }
    printf("PASS: %s\n", argv[1]);
    return 0;
}
