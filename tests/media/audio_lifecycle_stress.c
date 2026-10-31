/* Real shared HLE linked to each complete adapter; only host APIs are doubled. */
#define TSFP_INTEGRATED_AUDIO
#include "audio_buffer_timing.c"
void test_output_fault(void);
void test_output_recover(void);
int test_output_active(unsigned slot);
int test_output_clean(unsigned slot);

static void final_release(void)
{
    uint32_t object = HLE_ARG(0);
    g_eax = --HLE_MEM32(object + 4);
    g_esp += 8;
}
static uint32_t prepare_buffer(unsigned i)
{
    uint32_t base = 0x10000 + i * 0x300, iface = base + 0x20;
    HLE_MEM32(iface - 0x18) = 1;
    HLE_MEM32(iface) = base + 0x100;
    HLE_MEM32(iface - 0xC) = base + 0x200;
    HLE_MEM32(base + 0x100 + SET_DATA) = 0x50000;
    HLE_MEM32(base + 0x100 + SET_SIZE) = 19200;
    HLE_MEM32(base + 0x200 + SET_FORMAT) = 1u | (1u << 16) | (16u << 24);
    HLE_MEM32(base + 0x200 + SET_RATE) = 48000;
    HLE_MEM32(base + 0x200 + SET_ALIGN) = 2;
    return iface;
}
int audio_lifecycle_stress(int enhanced)
{
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_esp = 0x48000; g_lead_ms = 40; release_callback = final_release;
    for (unsigned scene = 0; scene < 32; ++scene) {
        memset(memory + 0x50000, (int)scene + 1, 19200);
        uint32_t iface[8];
        for (unsigned i = 0; i < 8; ++i) {
            iface[i] = prepare_buffer(i);
            Buffer *b = model_for(iface[i], now_ms()); CHECK(b);
            CHECK(!b->fed && !b->paused && !b->spatial && !b->room);
            CHECK(test_output_clean(i));
            b->spatial = enhanced; b->pan = -.5f;
            b->obstruction = enhanced ? .5f : 0;
            HLE_ARG(0) = iface[i]; HLE_ARG(3) = RECOMP_DSOUND_PLAY_LOOPING;
            hle_IDirectSoundBuffer_Play(); CHECK(!g_eax && test_output_active(i));
            if (i % 3 == 0) {
                HLE_ARG(1) = DSBPAUSE_PAUSE; hle_IDirectSoundBuffer_Pause();
                CHECK(!g_eax && b->paused && !test_output_active(i));
            }
        }
        test_output_fault();
        uint64_t now = now_ms();
        for (unsigned i = 0; i < 8; ++i) if (!g_buffers[i].paused) pump(&g_buffers[i], now + 150);
        for (unsigned i = 0; i < 4; ++i) {
            HLE_ARG(0) = iface[i] - 0x1C; hle_CDirectSoundBuffer_Release();
            CHECK(!g_eax && !g_buffers[i].iface && !test_output_active(i) && test_output_clean(i));
        }
        test_output_recover();
        for (unsigned i = 4; i < 8; ++i) {
            Buffer *b = &g_buffers[i];
            if (b->paused) {
                HLE_ARG(0) = iface[i]; HLE_ARG(1) = DSBPAUSE_RESUME;
                hle_IDirectSoundBuffer_Pause(); CHECK(!g_eax && !b->paused);
            } else pump(b, now + 160);
            CHECK(test_output_active(i));
            HLE_ARG(0) = iface[i] - 0x1C; hle_CDirectSoundBuffer_Release();
            CHECK(!g_eax && !b->iface && test_output_clean(i));
        }
        /* Scene is gone before another tick; recovery must not resurrect it. */
        hle_DirectSoundDoWork();
        for (unsigned i = 0; i < 8; ++i) CHECK(!g_buffers[i].iface && !test_output_active(i));
    }
    CHECK(g_stat.released == 256 && !g_stat.pool_full && g_stat.refused > 0);
    puts("PASS: 32 scene teardowns with pause, loss, final release and recovery");
    return 0; /* atexit must emit the short session's summary without a manual report */
}
