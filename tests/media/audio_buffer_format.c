/* Actual buffer adapter: format identity and partial source tails. */
#define main existing_buffer_main
#include "audio_buffer_timing.c"
#undef main
static uint32_t setup_buffer(uint32_t size, int adpcm) {
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
