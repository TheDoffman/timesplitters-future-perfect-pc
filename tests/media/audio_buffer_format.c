/* Actual buffer adapter: format identity and partial source tails. */
#define main existing_buffer_main
#include "audio_buffer_timing.c"
#undef main
static uint32_t setup_buffer(uint32_t size, int adpcm) {
    PcAudioMixOptions mix=PC_AUDIO_MIX_DEFAULTS;pc_audio_mix_live=pc_audio_mix_pack(mix);
    uint32_t iface = 0x11020;
    g_xbox_mem_offset=(ptrdiff_t)memory; g_esp=0x48000;
    g_lead_ms=40; g_layout=0; accept=1;
    HLE_MEM32(iface)=0x12000; HLE_MEM32(iface-12)=0x13000;
    HLE_MEM32(0x12000+SET_DATA)=0x20000;
    HLE_MEM32(0x12000+SET_SIZE)=size;
    HLE_MEM32(0x13000+SET_FORMAT)=(adpcm?TAG_ADPCM:TAG_PCM)|(1u<<16)|((adpcm?4u:16u)<<24);
    HLE_MEM32(0x13000+SET_RATE)=48000;
    HLE_MEM32(0x13000+SET_ALIGN)=adpcm?36:2;
    return iface;
}
int main(int argc, char **argv) {
    CHECK(argc==2);
    int tail=!strncmp(argv[1],"tail",4), adpcm=strstr(argv[1],"adpcm")!=NULL;
    uint32_t raw=tail?(adpcm?361:9601):9600;
    uint32_t iface = setup_buffer(raw,adpcm);
    Buffer *b = model_for(iface, 1000);
    CHECK(b);
    if(!strcmp(argv[1],"mix-gain")) {
        PcAudioMixOptions mix=PC_AUDIO_MIX_DEFAULTS;mix.volume[PC_AUDIO_DIALOGUE]=50;
        pc_audio_mix_live=pc_audio_mix_pack(mix);
        HLE_MEM32(g_esp+4)=iface;HLE_MEM32(g_esp+16)=RECOMP_DSOUND_PLAY_FROMSTART;
        unsigned token=pc_audio_scope_enter(PC_AUDIO_DIALOGUE,g_esp+256);
        hle_IDirectSoundBuffer_Play();pc_audio_scope_leave(token);
        CHECK(first_level==-602 && b->mix_bus==PC_AUDIO_DIALOGUE && b->fed);
        /* Volume changes apply even after the last PCM chunk was queued. */
        b->output_model.playing=0;uint32_t bytes=accepted_bytes;volume_updates=0;
        mix.master=50;pc_audio_mix_live=pc_audio_mix_pack(mix);pump(b,1000);
        CHECK(updated_level==-1204 && volume_updates==1 && accepted_bytes==bytes);
        puts("PASS: mix-gain");return 0;
    }
    if(!strcmp(argv[1],"bus")) {
        HLE_MEM32(g_esp+4)=iface;HLE_MEM32(g_esp+16)=RECOMP_DSOUND_PLAY_FROMSTART;
        unsigned token=pc_audio_scope_enter(PC_AUDIO_DIALOGUE,g_esp+256);
        hle_IDirectSoundBuffer_Play();pc_audio_scope_leave(token);
        CHECK(g_eax==0 && b->mix_bus==PC_AUDIO_DIALOGUE && accepted_bytes>0 && first_bus==PC_AUDIO_DIALOGUE);
        HLE_MEM32(g_esp+16)=0;hle_IDirectSoundBuffer_Play();
        CHECK(b->mix_bus==PC_AUDIO_DIALOGUE); /* ordinary continuation keeps the source */
        accepted_bytes=0;HLE_MEM32(g_esp+16)=RECOMP_DSOUND_PLAY_FROMSTART;
        hle_IDirectSoundBuffer_Play();CHECK(b->mix_bus==PC_AUDIO_EFFECTS && first_bus==PC_AUDIO_EFFECTS);
        HLE_MEM32(0x4ca9b4)=0x50000;HLE_MEM32(0x50008)=0x210;
        token=pc_audio_stream_scope(0x5836d0,0,g_esp);
        CHECK(pc_audio_scope_bus(g_esp)==PC_AUDIO_BACKGROUND);pc_audio_scope_leave(token);
        unsigned outer=pc_audio_scope_enter(PC_AUDIO_DIALOGUE,g_esp+256);
        token=pc_audio_stream_scope(0x5836d0,0,g_esp);
        CHECK(pc_audio_scope_bus(g_esp)==PC_AUDIO_DIALOGUE);pc_audio_scope_leave(token);
        token=pc_audio_stream_scope(0x64bd98,UINT32_MAX,g_esp);
        CHECK(pc_audio_scope_bus(g_esp)==PC_AUDIO_MUSIC);pc_audio_scope_leave(token);pc_audio_scope_leave(outer);
        HLE_MEM32(0x50000+5703*20+8)=0x210;
        token=pc_audio_stream_scope(0x5836d0,5703,g_esp);
        CHECK(pc_audio_scope_bus(g_esp)==PC_AUDIO_EFFECTS);pc_audio_scope_leave(token);
        HLE_MEM32(0x4ca9b4)=0xfffffffcu;
        token=pc_audio_stream_scope(0x5836d0,0,g_esp);
        CHECK(pc_audio_scope_bus(g_esp)==PC_AUDIO_EFFECTS);pc_audio_scope_leave(token);
        token=pc_audio_stream_scope(0x123456,0,g_esp);
        CHECK(pc_audio_scope_bus(g_esp)==PC_AUDIO_EFFECTS);pc_audio_scope_leave(token);
        printf("PASS: bus\n");return 0;
    }
    if (tail) {
        CHECK(b->size==raw);
        CHECK(b->model.size_bytes==(adpcm?1280:9600));
        CHECK(recomp_dsound_buffer_play(&b->model,0,1000)==0);
        b->output_model=b->model; pump(b,1000);
        CHECK(b->iface==iface && accepted_bytes==(adpcm?1280:3840));
        CHECK(model_for(iface,1000)==b && b->model.playing);
        pump(b,1040); pump(b,1080);
        CHECK(accepted_bytes==(adpcm?1280:9600));
    } else if (!strcmp(argv[1],"frequency")) {
        CHECK(recomp_dsound_buffer_set_frequency(&b->model,32000,1000)==0);
        CHECK(model_for(iface,1000)==b && b->model.sample_rate==32000);
    } else {
        uint32_t wfx=0x49000;
        HLE_MEM32(wfx)=TAG_PCM|(1u<<16);
        HLE_MEM32(wfx+4)=!strcmp(argv[1],"rate")?24000:48000;
        HLE_MEM32(wfx+12)=(!strcmp(argv[1],"align")?4u:2u)|(16u<<16);
        HLE_MEM32(g_esp+4)=iface; HLE_MEM32(g_esp+8)=wfx;
        hle_IDirectSoundBuffer_SetFormat(); CHECK(g_eax==0);
        b=find(iface); CHECK(b);
        CHECK(b->model.original_sample_rate==HLE_MEM32(wfx+4));
        CHECK(b->output_model.sample_rate==HLE_MEM32(wfx+4));
        CHECK(b->model.block_align==(HLE_MEM32(wfx+12)&0xffff));
        CHECK(reset_count[0]==1);
    }
    printf("PASS: %s\n",argv[1]); return 0;
}
