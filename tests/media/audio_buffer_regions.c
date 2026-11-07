/* Production buffer adapter, clocks and decoder; mock only host submission. */
#define TSFP_REAL_ADPCM
#define main existing_buffer_main
#include "audio_buffer_timing.c"
#undef main
#include "hle/xbox_adpcm.c"

static uint8_t captured[200000];
static unsigned captured_bytes;
static void capture(const uint8_t *pcm,uint32_t bytes,uint32_t channels,uint32_t bits) {
    (void)channels;(void)bits;
    if(captured_bytes+bytes>sizeof captured)abort();
    memcpy(captured+captured_bytes,pcm,bytes);captured_bytes+=bytes;
}
static int16_t sample_value(unsigned frame,unsigned channel,int adpcm) {
    int16_t value=(int16_t)(adpcm?1000+frame/64*200:frame+100);
    return channel?(int16_t)-value:value;
}
static int compare(unsigned first,unsigned count,unsigned channels,unsigned bits,int adpcm,
                   unsigned start,unsigned end,int looping) {
    CHECK(captured_bytes==count*channels*(bits/8));
    for(unsigned frame=0;frame<count;frame++) {
        unsigned source=first+frame;
        if(looping && source>=end)source=start+(source-end)%(end-start);
        for(unsigned channel=0;channel<channels;channel++) {
            unsigned index=(frame*channels+channel)*(bits/8);
            if(bits==8)CHECK(captured[index]==(uint8_t)(source+48+channel));
            else {int16_t value;memcpy(&value,captured+index,2);
                CHECK(value==sample_value(source,channel,adpcm));}
        }
    }
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==2);const char *mode=argv[1];
    int adpcm=strstr(mode,"adpcm")!=NULL,bits=strstr(mode,"pcm8")?8:16;
    unsigned channels=strstr(mode,"stereo")?2:1,frames=bits==8?144:512;
    unsigned start=bits==8?32:128,end=bits==8?96:320;
    if(!strcmp(mode,"tiny")){start=16;end=20;}
    if(!strcmp(mode,"tiny-one")){start=16;end=17;}
    if(!strcmp(mode,"whole")){start=0;end=frames;}
    if(!strcmp(mode,"suffix"))end=frames;
    int looping=strcmp(mode,"oneshot")!=0;
    unsigned alignment=adpcm?36*channels:channels*(bits/8);
    unsigned size=adpcm?frames/64*alignment:frames*alignment;
    unsigned raw_size=size+(!strcmp(mode,"partial")?1:0);
    unsigned loop_start=adpcm?start/64*alignment:start*alignment;
    unsigned loop_end=adpcm?end/64*alignment:end*alignment;
    g_xbox_mem_offset=(ptrdiff_t)memory;g_esp=0x18000;g_layout=0;
    g_lead_ms=40;g_adaptive_lead=0;accept=1;accepted_pcm_callback=capture;
    PcAudioMixOptions mix=PC_AUDIO_MIX_DEFAULTS;pc_audio_mix_live=pc_audio_mix_pack(mix);
    uint32_t iface=0x11020,settings=0x12000,voice=0x13000,data=0x40000;
    HLE_MEM32(iface)=settings;HLE_MEM32(iface-12)=voice;
    HLE_MEM32(settings+SET_DATA)=data;HLE_MEM32(settings+SET_SIZE)=raw_size;
    HLE_MEM32(settings+SET_LOOP_START)=loop_start;
    HLE_MEM32(settings+SET_LOOP_LEN)=(!strcmp(mode,"whole")||!strcmp(mode,"suffix"))?0:loop_end-loop_start;
    HLE_MEM32(voice+SET_FORMAT)=(adpcm?TAG_ADPCM:TAG_PCM)|(channels<<16)|((adpcm?4:bits)<<24);
    HLE_MEM32(voice+SET_RATE)=1000;HLE_MEM32(voice+SET_ALIGN)=alignment;
    if(adpcm) {
        for(unsigned block=0;block<frames/64;block++)for(unsigned c=0;c<channels;c++) {
            int16_t predictor=sample_value(block*64,c,1);
            memcpy(memory+data+block*alignment+c*4,&predictor,2);
        }
    } else for(unsigned f=0;f<frames;f++)for(unsigned c=0;c<channels;c++) {
        unsigned at=data+(f*channels+c)*(bits/8);
        if(bits==8)memory[at]=(uint8_t)(f+48+c);
        else {int16_t value=sample_value(f,c,0);memcpy(memory+at,&value,2);}
    }
    if(!strcmp(mode,"invalid-length")) {
        HLE_MEM32(settings+SET_LOOP_LEN)=UINT32_MAX;end=frames;
    }
    if(!strcmp(mode,"unaligned-length")) {
        HLE_MEM32(settings+SET_LOOP_LEN)=3;end=frames;
    }
    if(!strcmp(mode,"invalid-start")) {
        HLE_MEM32(settings+SET_LOOP_START)=UINT32_MAX;start=0;end=frames;
    }
    uint8_t source_before[4096];CHECK(raw_size<=sizeof source_before);
    memcpy(source_before,memory+data,raw_size);
    Buffer *b=model_for(iface,1000);CHECK(b);
    CHECK(recomp_dsound_buffer_play(&b->model,looping?RECOMP_DSOUND_PLAY_LOOPING:0,1000)==0);
    b->output_model=b->model;
    if(!strncmp(mode,"fractional",10)) {
        CHECK(recomp_dsound_buffer_set_frequency(&b->model,1297,1000)==0);
        b->output_model=b->model;pump(b,1000);
        unsigned step=strstr(mode,"17")?17:10;
        for(unsigned now=1000+step;now<2000;now+=step) {
            if(!strcmp(mode,"fractional-refused") && now==1020) {
                RecompDsoundBufferModel before=b->output_model;unsigned held=captured_bytes;
                accept=0;pump(b,now);
                CHECK(!memcmp(&before,&b->output_model,sizeof before) && captured_bytes==held);
                accept=1;
            }
            pump(b,now);
        }
        pump(b,2000);
        CHECK(compare(0,1348,channels,bits,adpcm,start,end,1)==0);
        CHECK(!memcmp(source_before,memory+data,raw_size));
        puts("PASS: fractional loop clock and PCM");return 0;
    }
    if(!strcmp(mode,"clock")) {
        CHECK(recomp_dsound_buffer_cursor(&b->model,1400)==416); /* source208, not tail400 */
        puts("PASS: clock");return 0;
    }
    if(!strcmp(mode,"huge-clock")) {
        uint64_t elapsed=UINT64_MAX-1000;
        unsigned expected=start+(unsigned)((elapsed-end)%(end-start));
        CHECK(recomp_dsound_buffer_cursor(&b->model,UINT64_MAX)==expected*2);
        puts("PASS: huge-clock");return 0;
    }
    if(!strcmp(mode,"rate")) {
        CHECK(recomp_dsound_buffer_set_frequency(&b->model,2000,1050)==0);
        CHECK(recomp_dsound_buffer_cursor(&b->model,1200)==316); /* source158 after350 frames */
        CHECK(recomp_dsound_buffer_set_frequency(&b->model,0,1200)==0 && b->model.sample_rate==1000);
        puts("PASS: rate");return 0;
    }
    if(!strcmp(mode,"pause")) {
        recomp_dsound_buffer_stop(&b->model,1050);
        CHECK(recomp_dsound_buffer_cursor(&b->model,9000)==100);
        CHECK(recomp_dsound_buffer_play(&b->model,RECOMP_DSOUND_PLAY_LOOPING,9000)==0);
        b->output_model=b->model;pump(b,9000);
        CHECK(compare(50,40,channels,bits,adpcm,start,end,1)==0);
        for(unsigned now=9020;now<=9600;now+=20)pump(b,now);
        CHECK(compare(50,640,channels,bits,adpcm,start,end,1)==0);
        puts("PASS: pause");return 0;
    }
    if(!strcmp(mode,"seek")) {
        CHECK(recomp_dsound_buffer_set_position(&b->model,(end+8)*2,1000)==0);
        b->output_model=b->model;pump(b,1000);
        CHECK(compare(start,40,channels,bits,adpcm,start,end,1)==0);
        puts("PASS: seek");return 0;
    }
    pump(b,1000);
    CHECK(compare(0,40,channels,bits,adpcm,start,end,looping)==0);
    if(!strcmp(mode,"hitch")) {
        captured_bytes=0;pump(b,1400);
        CHECK(compare(start+(400-end)%(end-start),40,channels,bits,adpcm,start,end,1)==0);
        puts("PASS: hitch");return 0;
    }
    if(!strcmp(mode,"live")) {
        HLE_MEM32(settings+SET_LOOP_START)=32;HLE_MEM32(settings+SET_LOOP_LEN)=8;
        b=model_for(iface,1100);CHECK(b);captured_bytes=0;
        pump(b,1100);
        CHECK(compare(16,40,channels,bits,adpcm,16,20,1)==0);
        puts("PASS: live");return 0;
    }
    if(!strcmp(mode,"refused")) {
        RecompDsoundBufferModel before=b->output_model;
        PcAudioDynamics history=b->dynamics;unsigned bytes=captured_bytes;
        accept=0;pump(b,1020);
        CHECK(!memcmp(&before,&b->output_model,sizeof before));
        CHECK(!memcmp(&history,&b->dynamics,sizeof history) && captured_bytes==bytes);
        accept=1;
    }
    for(unsigned now=1020;now<=1600;now+=20)pump(b,now);
    unsigned total=looping?640:frames;
    CHECK(compare(0,total,channels,bits,adpcm,start,end,looping)==0);
    CHECK(!memcmp(source_before,memory+data,raw_size));
    CHECK(b->model.playing); /* output completion cannot advance the independent guest clock */
    puts("PASS: region PCM");return 0;
}
