/* Run the actual buffer pump against a refusing backend, without hardware. */
#include "hle/hle_dsound.c"
#include "hle/dsound_buffer_model.c"
RECOMP_TLS uint32_t g_eax,g_ebx,g_ecx,g_edx,g_esi,g_edi,g_esp,g_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];RECOMP_TLS int g_fp_top,g_fp_cmp;
RECOMP_TLS uint16_t g_fp_cc,g_fp_control_word;
static uint8_t memory[8 * 1024 * 1024];
ptrdiff_t g_xbox_mem_offset;
size_t g_xbox_total_ram = sizeof memory;
uint32_t g_xbox_code_lo, g_xbox_code_hi;
const uint32_t hle_var_CMcpxVoiceClient_Commit3dSettings_0 = 0;
const uint32_t hle_var_CMcpxVoiceClient_SetVolume = 0;
const uint32_t hle_var_CDirectSoundVoice_SetVolume = 0;
const uint32_t hle_var_CDirectSoundStream_SetVolume = 0;
const uint32_t hle_var_CDirectSound3DCalculator_Calculate3D = 0;
const uint32_t hle_var_CDirectSound3DCalculator_GetVoiceData = 0;
static void (*release_callback)(void);
static void release_original(void) {
    if (release_callback) release_callback();
    else g_eax = 0x80004005u;
}
void (*const hle_original_CDirectSoundBuffer_Release)(void) = release_original;
void (*const hle_original_IDirectSoundBuffer_SetFormat)(void) = NULL;
void (*const hle_original_IDirectSoundBuffer_SetBufferData)(void) = NULL;
void (*const hle_original_CMcpxVoiceClient_Commit3dSettings_0)(void) = NULL;
static int acoustic_mock;
static void acoustic_ray_mock(void) {
    if(g_eax!=2 || HLE_MEM32(g_esp+4)!=0 || HLE_MEM32(g_esp+8)!=0)abort();
    float start[3],delta[3];memcpy(start,HLE_PTR(g_edi),12);memcpy(delta,HLE_PTR(g_esi),12);
    if(fabsf(start[0]-.015f)>.001f || fabsf(delta[0]-9.97f)>.001f)abort();
    g_ebx=123;g_esi=456;g_edi=789;g_xmm7.f[0]=123;g_fp_top=4;g_fp_stack[0]=123;
    g_eax=0;g_esp+=4;
}
recomp_func_t recomp_lookup(uint32_t address) { return acoustic_mock && address==0x88950 ? acoustic_ray_mock : NULL; }
int xbox_EnvSwitch(const char *name, int value) { return value; }
void hle_dsound_stream_tick(uint64_t now) { }
void hle_dsound_stream_acoustics(uint32_t settings,float obstruction,float room,int ambience) {}
void hle_dsound_stream_spatial(uint32_t settings,float pan,int enabled) { }
#ifndef TSFP_INTEGRATED_AUDIO
static unsigned reset_count[256];
void recomp_audio_output_reset_voice(uint32_t slot) { reset_count[slot]++; }
static int fake_device;
static uint64_t fake_now, fake_until[256];
int recomp_audio_output_position(uint32_t slot, uint64_t *played, uint32_t *queued)
{
    if (!fake_device) return 0;
    if (queued) *queued = fake_until[slot] > fake_now;
    return 1;
}
void recomp_audio_output_set_ambience(uint32_t slot,int enabled) {}
void recomp_audio_output_set_acoustics(uint32_t slot,float obstruction,float room) {}
void recomp_audio_output_set_spatial(uint32_t slot,float pan,int enabled) { }
int recomp_audio_output_engine_stats(RecompAudioEngineStats *stats) { memset(stats, 0, sizeof *stats); return 0; }
static int accept;
static uint32_t accepted_bytes;
static uint8_t first_byte;
int recomp_audio_output_submit(uint32_t slot, const uint8_t *pcm, uint32_t bytes,
    uint32_t rate, uint32_t channels, uint32_t bits, int32_t level)
{
    if (!accept) return 0;
    if (fake_device) {
        if (fake_until[slot] < fake_now) fake_until[slot] = fake_now;
        fake_until[slot] += (uint64_t)bytes * 1000 / (rate * channels * (bits / 8));
    }
    accepted_bytes += bytes;
    first_byte = pcm[0];
    return 1;
}
#endif
int xbox_adpcm_decode_block(const uint8_t *block, size_t bytes, unsigned channels,
                            int16_t *pcm, size_t samples)
{ memset(pcm, 0, samples * sizeof *pcm); return 1; }
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

#ifndef TSFP_INTEGRATED_AUDIO
int main(int argc, char **argv)
{
    if(argc==2 && !strcmp(argv[1],"acoustics")) {
        g_xbox_mem_offset=(ptrdiff_t)memory;g_esp=0x60000;
        g_ebx=11;g_esi=22;g_edi=33;g_xmm7.f[0]=44;g_fp_top=5;g_fp_stack[0]=66;
        float from[3]={0,0,0},to[3]={10,0,0};
        acoustic_mock=1;
        CHECK(pc_acoustic_ray(2,from,to,NULL)==1);
        CHECK(g_esp==0x60000 && g_ebx==11 && g_esi==22 && g_edi==33);
        CHECK(g_xmm7.f[0]==44 && g_fp_top==5 && g_fp_stack[0]==66);
        acoustic_mock=0;CHECK(pc_acoustic_ray(2,from,to,NULL)==-1);
        /* Real compact-tree layout: root leaf with one bounded room. */
        uint32_t tree[8]={0x10000,0x10100};
        float bounds[6]={-1,-1,-1,20,20,20};
        memcpy(tree+2,bounds,24);memcpy(HLE_PTR(0x702e00),tree,32);
        uint16_t node=0xc000,count=1,room=2;
        memcpy(HLE_PTR(0x10002),&node,2);memcpy(HLE_PTR(0x10100),&count,2);
        memcpy(HLE_PTR(0x10102),&room,2);
        memcpy(HLE_PTR(0x12000+2*0xb4+0x10),bounds,24);
        CHECK(pc_acoustic_room(to,3,0x12000)==2);
        float outside[3]={21,0,0};CHECK(pc_acoustic_room(outside,3,0x12000)==0);
        HLE_MEM32(0x790950)=1;HLE_MEM32(0x7b0cc0)=0x20000;HLE_MEM32(0x20208)=0x20400;
        float camera[3]={10,2,5},source[3]={20,8,10},view[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        memcpy(HLE_PTR(0x2027c),camera,12);memcpy(HLE_PTR(0x20400),view,64);
        HLE_MEM32(0x5836c0)=0x30000;HLE_MEM32(0x30000)=0x31000;
        HLE_MEM32(0x31000-12)=0x32000;HLE_MEM32(0x30030)=0x583690;
        memcpy(HLE_PTR(0x30008),source,12);float wl[3],ws[3],wv[16];
        CHECK(pc_acoustic_world(0x32000,wl,ws,wv));
        CHECK(wl[0]==10 && ws[0]==20 && wv[10]==1);
        PcAcousticResult spatial={{1,2,3},.5f,0},neutral={{9,9,9},0,0};
        spatial=pc_acoustic_view(spatial,wv,neutral);
        CHECK(spatial.direction[0]==1 && spatial.direction[1]==2 && spatial.direction[2]==-3);
        /* Positioned streams use their original world vector too. */
        HLE_MEM32(0x5836d0+4)=0x40000;HLE_MEM32(0x40000+0x14)=0x42000;
        HLE_MEM32(0x5836d0+0x28)=2;HLE_MEM32(0x5836d0+0x50)=1;
        HLE_MEM32(0x5836d0+0x64)=1;memcpy(HLE_PTR(0x5836d0+0x6c),source,12);
        CHECK(pc_acoustic_world(0x42000,wl,ws,wv) && ws[0]==20);
        HLE_MEM32(0x5836d0+0x64)=0;CHECK(!pc_acoustic_world(0x42000,wl,ws,wv));
        HLE_MEM32(0x790950)=2;CHECK(!pc_acoustic_world(0x32000,wl,ws,wv));
        puts("PASS: acoustic guest ABI/state preservation and bounded spatial-tree lookup");return 0;
    }
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

#endif
