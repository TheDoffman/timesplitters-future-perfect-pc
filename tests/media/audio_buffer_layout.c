/* Real HLE initialization followed by the original XDK's region-field contract. */
#define TSFP_AUDIO_BUFFER_DATA_ADDRESS 0x30000u
#define TSFP_AUDIO_BUFFER_LOOP_ADDRESS 0x31000u
#define main existing_buffer_main
#include "audio_buffer_timing.c"
#undef main
static const char data_bytes[]=
    "558bec515356578bf133dbbf00000080857e08895dfc75108b86bc0000003bc3740c50e8bd0f0000"
    "899ebc0000008b450c3bc38986c000000074398b4d083bcb740b097e08898ebc000000eb2780660b"
    "7f53506844536461e8200f00008986bc000000f7d81bc025f2fff87f050e0007808945fcffb6c000"
    "00008bce53e866f7ffff8b45fc5f5e5bc9c20800";
static const char loop_bytes[]=
    "558bec57e879f8ffff833da8244100000fb6f8741685ff740b68b4244100ff1518584700b8054000"
    "80eb558b4d1085c98b55088b421c568b750c7414538d1c0e3b98c80000005b760fbe32007888eb1e"
    "8b88c80000002bce89b0cc0000008988d00000008b4a20e8297500008bf085ff740b68b4244100ff"
    "15185847008bc65e5f5dc20c00";
static const char play_bytes[]=
    "8b44240483a1cc000000008981c40000008b4424088981c80000008981d0000000c20800";
static void install(uint32_t address,const char *hex) {
    for(unsigned i=0;hex[i*2];i++)
        ((uint8_t*)HLE_PTR(address))[i]=(uint8_t)(hex_digit(hex[i*2])*16+hex_digit(hex[i*2+1]));
}
/* Same validation/stores as original40717C; separately executed against the
   actual x86 body in the private ABI probe. No host-model internals are used. */
static uint32_t native_loop(uint32_t settings,uint32_t start,uint32_t length) {
    uint32_t play=HLE_MEM32(settings+0xc8);
    if(length && (uint64_t)start+length>play)return 0x88780032u;
    if(!length)length=play-start;
    HLE_MEM32(settings+0xcc)=start;HLE_MEM32(settings+0xd0)=length;return 0;
}
static unsigned native_calls,native_allocations,native_frees;
static int native_allocation_failure;
static void native_data(void) {
    uint32_t object=g_ecx,data=HLE_MEM32(g_esp+4),bytes=HLE_MEM32(g_esp+8);
    native_calls++;
    if(!(HLE_MEM32(object+8)&0x80000000u) && HLE_MEM32(object+0xbc))native_frees++;
    HLE_MEM32(object+0xbc)=0;HLE_MEM32(object+0xc0)=bytes;
    if(bytes) {
        if(data)HLE_MEM32(object+8)|=0x80000000u;
        else {HLE_MEM32(object+8)&=~0x80000000u;data=native_allocation_failure?0:0x60000;native_allocations++;}
        HLE_MEM32(object+0xbc)=data;
    }
    HLE_MEM32(object+0xc4)=HLE_MEM32(object+0xcc)=0;
    HLE_MEM32(object+0xc8)=HLE_MEM32(object+0xd0)=bytes;
    g_ecx=0xdead;g_edx=0xbeef;g_eax=native_allocation_failure?0x8007000eu:0;g_esp+=12;
}
int main(int argc,char **argv) {
    CHECK(argc==2);const char *mode=argv[1];
    g_xbox_mem_offset=(ptrdiff_t)memory;g_esp=0x18000;g_layout=0;g_lead_ms=40;accept=1;
    PcAudioMixOptions mix=PC_AUDIO_MIX_DEFAULTS;pc_audio_mix_live=pc_audio_mix_pack(mix);
    native_buffer_data_callback=native_data;
    int legacy=!strcmp(mode,"legacy") || !strncmp(mode,"guard-",6);
    if(strcmp(mode,"legacy")) {
        install(0x30000,data_bytes);install(0x31000,loop_bytes);install(0x32000,play_bytes);
        HLE_MEM32(0x30000+0x7e)=0x32000-(0x30000+0x7d+5);
        if(!strcmp(mode,"guard-data"))HLE_MEM32(0x30000+0x39)=0;
        if(!strcmp(mode,"guard-loop"))HLE_MEM32(0x31000+0x59)=0;
        if(!strcmp(mode,"guard-play"))HLE_MEM32(0x32000+0x0d)=0;
    }
    uint32_t iface=0x11020,settings=0x12000,voice=0x13000,data=0x40000,size=144;
    HLE_MEM32(iface)=settings;HLE_MEM32(iface-12)=voice;
    HLE_MEM32(voice+SET_FORMAT)=TAG_PCM|(1u<<16)|(16u<<24);
    HLE_MEM32(voice+SET_RATE)=1000;HLE_MEM32(voice+SET_ALIGN)=2;
    HLE_MEM32(settings+8)=0x110;
    HLE_MEM32(settings+0xb8)=0x5a5a5a5a; /* native reserved word must survive */
    HLE_MEM32(g_esp+4)=iface;HLE_MEM32(g_esp+8)=data;HLE_MEM32(g_esp+12)=size;
    g_ecx=0x1234;g_edx=0x5678;
    hle_IDirectSoundBuffer_SetBufferData();CHECK(g_eax==0);
    CHECK(g_esp==0x18000 && g_ecx==0x1234 && g_edx==0x5678);
    uint32_t bias=legacy?0:4;
    CHECK(HLE_MEM32(settings+SET_DATA+bias)==data);
    CHECK(HLE_MEM32(settings+SET_SIZE+bias)==size);
    CHECK(HLE_MEM32(settings+SET_PLAY_START+bias)==0);
    CHECK(HLE_MEM32(settings+SET_PLAY_LEN+bias)==(legacy?0:size));
    CHECK(HLE_MEM32(settings+SET_LOOP_START+bias)==0);
    CHECK(HLE_MEM32(settings+SET_LOOP_LEN+bias)==(legacy?0:size));
    CHECK(HLE_MEM32(settings+8)==(legacy?0x110u:0x80000110u));
    CHECK(native_calls==(legacy?0u:1u));
    if(!legacy)CHECK(HLE_MEM32(settings+0xb8)==0x5a5a5a5a);
    Buffer *b=find(iface);CHECK(b && b->data==data && b->size==size);
    b->model.last_ms=b->output_model.last_ms=1000;
    if(!strcmp(mode,"native-clear")) {
        HLE_MEM32(g_esp+12)=0; /* Native zero-length clears even a nonnull argument. */
        hle_IDirectSoundBuffer_SetBufferData();CHECK(g_eax==0);
        CHECK(HLE_MEM32(settings+0xbc)==0 && HLE_MEM32(settings+0xc0)==0);
        CHECK(!find(iface));
        CHECK(!native_frees); /* caller-owned PCM is never freed */
    }
    if(!strcmp(mode,"native-allocate")) {
        HLE_MEM32(g_esp+8)=0;
        hle_IDirectSoundBuffer_SetBufferData();CHECK(g_eax==0);
        CHECK(HLE_MEM32(settings+0xbc)==0x60000 && native_allocations==1 && !native_frees);
        CHECK(!(HLE_MEM32(settings+8)&0x80000000u));
        HLE_MEM32(g_esp+8)=data;
        hle_IDirectSoundBuffer_SetBufferData();CHECK(g_eax==0);
        CHECK(HLE_MEM32(settings+0xbc)==data && native_frees==1 && native_allocations==1);
        CHECK(HLE_MEM32(settings+8)&0x80000000u);
    }
    if(!strcmp(mode,"native-failure")) {
        native_allocation_failure=1;HLE_MEM32(g_esp+8)=0;
        hle_IDirectSoundBuffer_SetBufferData();CHECK(g_eax==0x8007000eu);
        CHECK(HLE_MEM32(settings+0xbc)==0 && native_calls==2 && !native_frees);
        CHECK(!find(iface) || find(iface)->data==0);
    }
    if(!strcmp(mode,"native-missing")) {
        native_buffer_data_callback=NULL;HLE_MEM32(g_esp+12)=size*2;
        hle_IDirectSoundBuffer_SetBufferData();CHECK(g_eax==0x80004005u);
        CHECK(HLE_MEM32(settings+0xbc)==data && HLE_MEM32(settings+0xc0)==size && native_calls==1);
    }
    if(!strcmp(mode,"native-start")) {
        CHECK(native_loop(settings,36,108)==0);
        b=model_for(iface,1000);CHECK(b && b->model.loop_start_bytes==36);
        CHECK(recomp_dsound_buffer_play(&b->model,RECOMP_DSOUND_PLAY_LOOPING,1000)==0);
        CHECK(recomp_dsound_buffer_cursor(&b->model,1080)==52); /* intro once; repeat18..71 */
        CHECK(native_loop(settings,0,0)==0);
        b=model_for(iface,1100);CHECK(b && b->model.loop_start_bytes==0);
    }
    if(!strcmp(mode,"native-end")) {
        CHECK(native_loop(settings,36,72)==0);
        b=model_for(iface,1000);CHECK(b);
        CHECK(recomp_dsound_buffer_play(&b->model,RECOMP_DSOUND_PLAY_LOOPING,1000)==0);
        CHECK(recomp_dsound_buffer_cursor(&b->model,1060)==48); /* loop18..53; source24 */
    }
    printf("PASS: %s\n",mode);return 0;
}
