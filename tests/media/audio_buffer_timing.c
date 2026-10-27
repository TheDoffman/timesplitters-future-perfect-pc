/* Run the actual buffer pump against a refusing backend, without hardware. */
#include "hle/hle_dsound.c"
#include "hle/dsound_buffer_model.c"
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
static uint8_t memory[1024 * 1024];
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_total_ram = sizeof memory;
uint32_t g_xbox_code_lo, g_xbox_code_hi;
const uint32_t hle_var_CMcpxVoiceClient_Commit3dSettings_0 = 0;
const uint32_t hle_var_CMcpxVoiceClient_SetVolume = 0;
const uint32_t hle_var_CDirectSoundVoice_SetVolume = 0;
const uint32_t hle_var_CDirectSoundStream_SetVolume = 0;
const uint32_t hle_var_CDirectSound3DCalculator_Calculate3D = 0;
const uint32_t hle_var_CDirectSound3DCalculator_GetVoiceData = 0;
void (*const hle_original_IDirectSoundBuffer_SetFormat)(void) = NULL;
void (*const hle_original_IDirectSoundBuffer_SetBufferData)(void) = NULL;
void (*const hle_original_CMcpxVoiceClient_Commit3dSettings_0)(void) = NULL;
recomp_func_t recomp_lookup(uint32_t address) { return NULL; }
int xbox_EnvSwitch(const char *name, int value) { return value; }
void hle_dsound_stream_tick(uint64_t now) { }
void recomp_audio_output_reset_voice(uint32_t slot) { }
int recomp_audio_output_position(uint32_t slot, uint64_t *played, uint32_t *queued) { return 0; }
int recomp_audio_output_engine_stats(RecompAudioEngineStats *stats) { memset(stats, 0, sizeof *stats); return 0; }
static int accept;
static uint32_t accepted_bytes;
static uint8_t first_byte;
int recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t rate, uint32_t channels, uint32_t bits, int32_t level)
{
    if (!accept) return 0;
    accepted_bytes += bytes;
    first_byte = pcm[0];
    return 1;
}
int xbox_adpcm_decode_block(const uint8_t *block, size_t bytes, unsigned channels,
                            int16_t *pcm, size_t samples)
{ memset(pcm, 0, samples * sizeof *pcm); return 1; }
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "headroom")) {
        uint32_t stream = 0x14000, settings = 0x15000;
        g_xbox_mem_offset = (ptrdiff_t)memory;
        g_layout = g_stream_layout = 1; /* checked XDK 5849 layout */
        HLE_MEM32(stream + 0x14) = settings;
        HLE_MEM32(settings + 0x20) = 600;
        /* Raw stream SetVolume is zero; the Xbox voice plays at -6 dB. */
        CHECK(hle_dsound_stream_adjust(stream, NULL) == -600);
        HLE_MEM32(settings + 0x20) = 300;
        CHECK(hle_dsound_stream_adjust(stream, NULL) == -300);
        HLE_MEM32(settings + 0x24) = 1;
        HLE_MEM32(settings + 0x28) = 0;
        HLE_MEM32(settings + 0x30) = (uint32_t)-1200;
        CHECK(hle_dsound_stream_adjust(stream, NULL) == -1500);
        g_stream_layout = 0;
        CHECK(hle_dsound_stream_adjust(stream, NULL) == 0);
        puts("PASS: stream headroom combines with mix-bin attenuation");
        return 0;
    }
    Buffer *b = &g_buffers[0];
    uint32_t flags = argc == 2 ? RECOMP_DSOUND_PLAY_LOOPING : 0;
    g_xbox_mem_offset = (ptrdiff_t)memory;
    g_lead_ms = 40;
    g_layout = 0;
    b->iface = 0x11020; b->data = 0x20000; b->size = 3840;
    b->format = 1u | (1u << 16) | (16u << 24); b->output = 1;
    HLE_MEM32(b->iface) = 0x12000;
    HLE_MEM32(b->iface - 0xCu) = 0x13000;
    HLE_MEM32(0x12000 + SET_DATA) = b->data;
    HLE_MEM32(0x12000 + SET_SIZE) = b->size;
    HLE_MEM32(0x13000 + SET_FORMAT) = b->format;
    memset(memory + b->data, 0x5A, b->size);
    CHECK(recomp_dsound_buffer_configure(&b->model, b->size, 48000, 2, 1000) == 0);
    CHECK(recomp_dsound_buffer_play(&b->model, flags, 1000) == 0);
    b->output_model = b->model;
    RecompDsoundBufferModel before = b->output_model;
    pump(b, 1000); /* full host queue */
    CHECK(!memcmp(&before, &b->output_model, sizeof before));
    accept = 1;
    pump(b, 1000); /* same audio retried without advancing past it */
    CHECK(accepted_bytes == 3840 && first_byte == 0x5A);
    CHECK(b->output_model.last_ms == 1040);
    CHECK(b->output_model.playing == (flags != 0));
    puts("PASS: refused buffer PCM is retried, including a non-looping tail");
    return 0;
}
