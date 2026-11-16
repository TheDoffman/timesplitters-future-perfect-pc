/* Actual host player + original shared clocks; only bootstrap/device are faked. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hle/audio_output.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4245) /* Existing model's high-bit HRESULT enum returns. */
#endif
#include "hle/dsound_buffer_model.c"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "mods/mod_wave.h"
#include "gameplay/pc_audio_mix.h"
static uint8_t mono[44100],stereo[192000];
static TsfpModWave waves[2];
static uint64_t clock_ms=1000;
static int locked,fixture_ready=1,refuse,discontinuity;
static unsigned submits,resets,finishes;
static uint32_t last_slot,last_bytes,last_rate,last_channels;
static uint8_t last_pcm[80000];
static int pending[RECOMP_AUDIO_SLOT_COUNT];
static int32_t last_volume;
static int32_t effects_offset=-100;
static void lock(void){assert(!locked);locked=1;}
static void unlock(void){assert(locked);locked=0;}
static uint64_t now_ms(void){return clock_ms;}
const TsfpModWave *pc_mod_bootstrap_audio(uint32_t key){return fixture_ready?(key==0?&waves[0]:key==70000?&waves[1]:NULL):NULL;}
const char *pc_mod_bootstrap_source(size_t *n){if(n)*n=fixture_ready?1:0;return fixture_ready?"x":NULL;}
static int32_t pc_audio_mix_level(int32_t level,int bus){assert(bus==PC_AUDIO_EFFECTS);return level+effects_offset;}
void recomp_audio_output_reset_voice(uint32_t slot){assert(locked && slot>=RECOMP_AUDIO_SLOT_MOD_FIRST && slot<RECOMP_AUDIO_SLOT_COUNT);resets++;pending[slot]=0;}
int recomp_audio_output_set_volume(uint32_t slot,int32_t volume){assert(locked && slot>=RECOMP_AUDIO_SLOT_MOD_FIRST && slot<RECOMP_AUDIO_SLOT_COUNT);last_volume=volume;return pending[slot];}
int recomp_audio_output_take_discontinuity(uint32_t slot){(void)slot;int value=discontinuity;discontinuity=0;return value;}
int recomp_audio_output_tail_pending(uint32_t slot,int finish){assert(locked && finish==1);finishes++;return pending[slot];}
int recomp_audio_output_submit(uint32_t slot,const uint8_t *pcm,uint32_t bytes,uint32_t rate,uint32_t channels,uint32_t bits,int32_t level) {
    assert(locked && slot>=RECOMP_AUDIO_SLOT_MOD_FIRST && slot<RECOMP_AUDIO_SLOT_COUNT && bits==16 && bytes<=sizeof last_pcm && bytes%(channels*2u)==0);
    if(refuse)return 0;
    submits++;last_slot=slot;last_rate=rate;last_channels=channels;last_bytes=bytes;last_volume=level;
    memcpy(last_pcm,pcm,bytes);pending[slot]=1;return 1;
}
#include "mods/pc_mod_audio_host.inc"
static void tick(uint64_t now){clock_ms=now;lock();pc_mod_audio_tick_locked(now);unlock();}
static void setup(void) {
    for(size_t i=0;i<sizeof mono;i++)mono[i]=(uint8_t)(i*17u);
    for(size_t i=0;i<sizeof stereo;i++)stereo[i]=(uint8_t)(i*31u);
    waves[0]=(TsfpModWave){mono,sizeof mono,sizeof mono/2,22050,1};
    waves[1]=(TsfpModWave){stereo,sizeof stereo,sizeof stereo/4,48000,2};
}
int main(int argc,char **argv) {
    assert(argc==2);setup();const char *mode=argv[1];
    if(!strcmp(mode,"basic")) {
        assert(pc_mod_audio_play(0,0,0,-600,0));
        assert(submits==1 && last_slot==273 && last_rate==22050 && last_channels==1 && last_bytes==1764 && last_volume== -700);
        assert(!memcmp(last_pcm,mono,last_bytes));
        tick(1010);assert(submits==2 && !memcmp(last_pcm,mono+1764,last_bytes));
        pc_mod_audio_stop(0);assert(!pc_mod_audio_voices[0].wave && !pending[273]);
    } else if(!strcmp(mode,"bounds")) {
        assert(pc_mod_audio_play(31,70000,262144,0,1));
        assert(last_slot==304 && last_rate==192000 && last_channels==2 && last_bytes==30720);
        unsigned count=submits,before=resets;PcModAudioVoice old=pc_mod_audio_voices[31];
        assert(!pc_mod_audio_play(32,0,0,0,0) && !pc_mod_audio_play(31,9,0,0,0));
        assert(!pc_mod_audio_play(31,0,16383,0,0) && !pc_mod_audio_play(31,0,262145,0,0));
        assert(!pc_mod_audio_play(31,0,0,1,0) && !pc_mod_audio_play(31,0,0,-10001,0) && !pc_mod_audio_play(31,0,0,0,2));
        assert(!pc_mod_audio_pause(31,2) && !pc_mod_audio_pitch(31,1) && !pc_mod_audio_volume(31,1));
        assert(!memcmp(&old,&pc_mod_audio_voices[31],sizeof old) && submits==count && resets==before);
        pc_mod_audio_stop(32);assert(resets==before);
    } else if(!strcmp(mode,"refusal")) {
        refuse=1;assert(pc_mod_audio_play(0,0,0,0,0));
        assert(!submits && pc_mod_audio_voices[0].output.cursor_bytes==0 && pc_mod_audio_voices[0].output.last_ms==1000);
        tick(1010);assert(!submits && pc_mod_audio_voices[0].output.cursor_bytes==0);
        refuse=0;tick(1020);assert(submits==1 && last_bytes==2646 && !memcmp(last_pcm,mono,last_bytes));
    } else if(!strcmp(mode,"loop")) {
        waves[0].bytes=16;waves[0].frames=8;assert(pc_mod_audio_play(0,0,0,0,1));
        for(uint32_t i=0;i<last_bytes;i++)assert(last_pcm[i]==mono[i%16]);
        for(unsigned t=1010;t<1200;t+=10) {
            uint32_t offset=pc_mod_audio_voices[0].output.cursor_bytes;tick(t);
            for(uint32_t i=0;i<last_bytes;i++)assert(last_pcm[i]==mono[(offset+i)%16]);
        }
        assert(pc_mod_audio_voices[0].model.playing && !finishes);
    } else if(!strcmp(mode,"pause")) {
        assert(pc_mod_audio_play(0,0,0,0,1));clock_ms=1100;assert(pc_mod_audio_pause(0,1));
        uint32_t cursor=pc_mod_audio_voices[0].model.cursor_bytes;assert(cursor==4410 && !pending[273]);
        unsigned count=submits;tick(2100);assert(submits==count && pc_mod_audio_voices[0].model.cursor_bytes==cursor);
        assert(pc_mod_audio_pitch(0,131072));assert(pc_mod_audio_voices[0].model.cursor_bytes==cursor);
        clock_ms=2200;assert(pc_mod_audio_pause(0,0));assert(last_rate==44100 && !memcmp(last_pcm,mono+cursor,last_bytes));
        assert(pc_mod_audio_pitch(0,0) && last_rate==22050);
        assert(pc_mod_audio_volume(0,-10000) && last_volume== -10100);
    } else if(!strcmp(mode,"one-shot")) {
        waves[0].bytes=16;waves[0].frames=8;assert(pc_mod_audio_play(0,0,0,0,0));
        assert(last_bytes==16 && !memcmp(last_pcm,mono,16) && finishes==1);
        effects_offset=-600;tick(1010);assert(pc_mod_audio_voices[0].wave && submits==1 && last_volume==-600);
        pending[273]=0;tick(1020);assert(!pc_mod_audio_voices[0].wave && submits==1);
    } else if(!strcmp(mode,"stale")) {
        assert(pc_mod_audio_play(0,0,0,0,1));tick(2000);
        assert(last_bytes==1764 && !memcmp(last_pcm,mono,last_bytes));
        discontinuity=1;tick(2010);assert(last_bytes==1764 && !memcmp(last_pcm,mono+440,last_bytes));
    } else if(!strcmp(mode,"invalidate")) {
        for(unsigned i=0;i<32;i++)assert(pc_mod_audio_play(i,0,16384,0,1));
        fixture_ready=0;unsigned count=submits;assert(!pc_mod_audio_play(0,0,0,0,0));tick(1010);
        for(unsigned i=0;i<32;i++)assert(!pending[273+i] && !pc_mod_audio_voices[i].wave);
        assert(submits==count);pc_mod_audio_stop_all();
    } else if(!strcmp(mode,"budget")) {
        waves[1].bytes=4;waves[1].frames=1; // Worst-case tiny loop at maximum pitch.
        for(unsigned i=0;i<32;i++)assert(pc_mod_audio_play(i,70000,262144,0,1));
        for(uint32_t i=0;i<last_bytes;i++)assert(last_pcm[i]==stereo[i%4]);
        assert(submits==32);tick(1010);assert(submits==64);
        for(unsigned t=1010;t<1020;t++)tick(t);
        assert(submits==64);tick(1020);assert(submits==96);pc_mod_audio_stop_all();
    } else return 2;
    assert(!locked);puts("PASS actual mod voice controls and PCM submission");return 0;
}
