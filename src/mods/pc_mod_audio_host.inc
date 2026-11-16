/* Included once by the shared HLE audio unit after its lock/clock definitions.
 * No guest data or callbacks enter this player. All state uses the HLE lock.
 * Original buffers, streams and movie slots remain separate and unchanged. */
#include "mods/mod_audio_player.h"
#include "mods/mod_bootstrap.h"
#if TSFP_MOD_AUDIO_VOICES != RECOMP_AUDIO_SLOT_MOD_COUNT
#error Mod voice and backend slot budgets must match
#endif
typedef struct PcModAudioVoice {
    const TsfpModWave *wave;
    RecompDsoundBufferModel model,output;
    uint32_t paused_flags;
    int paused;
    int32_t volume,last_level;
} PcModAudioVoice;
static PcModAudioVoice pc_mod_audio_voices[TSFP_MOD_AUDIO_VOICES];
static uint64_t pc_mod_audio_last_tick;
static uint8_t pc_mod_audio_pcm[80000]; /* serialized scratch; no per-tick alloc */

static uint32_t pc_mod_audio_rate(const TsfpModWave *wave,uint32_t pitch) {
    if(!wave || (pitch && (pitch<16384u || pitch>262144u)))return 0;
    return pitch?(uint32_t)((uint64_t)wave->rate*pitch/65536u):wave->rate;
}
static void pc_mod_audio_clear_locked(uint32_t voice) {
    if(pc_mod_audio_voices[voice].wave)recomp_audio_output_reset_voice(RECOMP_AUDIO_SLOT_MOD_FIRST+voice);
    memset(&pc_mod_audio_voices[voice],0,sizeof pc_mod_audio_voices[voice]);
}
static void pc_mod_audio_feed_locked(uint32_t index,uint64_t now) {
    PcModAudioVoice *voice=&pc_mod_audio_voices[index];
    if(!voice->wave || voice->paused)return;
    uint32_t slot=RECOMP_AUDIO_SLOT_MOD_FIRST+index;
    (void)recomp_dsound_buffer_cursor(&voice->model,now);
    int32_t level=pc_audio_mix_level(voice->volume,PC_AUDIO_EFFECTS);
    if(level!=voice->last_level && recomp_audio_output_set_volume(slot,level))voice->last_level=level;
    if(!voice->output.playing) {
        if(!recomp_audio_output_tail_pending(slot,1))pc_mod_audio_clear_locked(index);
        return;
    }
    if(recomp_audio_output_take_discontinuity(slot) || (now>voice->output.last_ms && now-voice->output.last_ms>100u)) {
        // Reanchor only stale past audio, as the ordinary buffer pump does.
        recomp_audio_output_reset_voice(slot);voice->output=voice->model;
    }
    if(!voice->output.playing)return;
    uint64_t until=now>UINT64_MAX-40u?UINT64_MAX:now+40u;
    if(until<=voice->output.last_ms || until-voice->output.last_ms<10u)return;
    if(until-voice->output.last_ms>100u)until=voice->output.last_ms+100u;
    RecompDsoundBufferModel next=voice->output;uint32_t offset=0;
    uint32_t bytes=recomp_dsound_buffer_consume(&next,until,&offset);
    if(!bytes){voice->output=next;return;}
    if(bytes>sizeof pc_mod_audio_pcm || offset>=voice->wave->bytes)return;
    uint32_t first=voice->wave->bytes-offset;if(first>bytes)first=bytes;
    memcpy(pc_mod_audio_pcm,voice->wave->pcm+offset,first);
    if(first<bytes) {
        uint32_t seed=voice->wave->bytes;if(seed>bytes-first)seed=bytes-first;
        memcpy(pc_mod_audio_pcm+first,voice->wave->pcm,seed);
        uint32_t copied=first+seed;
        // Repeat a captured full period by doubling. A one-frame loop must not
        // turn every service tick into thousands of tiny copies per voice.
        while(copied<bytes) {
            uint32_t count=copied-first;if(count>bytes-copied)count=bytes-copied;
            memcpy(pc_mod_audio_pcm+copied,pc_mod_audio_pcm+first,count);copied+=count;
        }
    }
    if(recomp_audio_output_submit(slot,pc_mod_audio_pcm,bytes,next.sample_rate,voice->wave->channels,16u,level)) {
        voice->output=next;voice->last_level=level;
        if(!next.playing)(void)recomp_audio_output_tail_pending(slot,1);
    }
}
int pc_mod_audio_play(uint32_t index,uint32_t resource,uint32_t pitch,int32_t volume,int looping) {
    if(index>=TSFP_MOD_AUDIO_VOICES || volume< -10000 || volume>0 || (looping!=0 && looping!=1))return 0;
    lock();
    const TsfpModWave *wave=pc_mod_bootstrap_audio(resource);uint32_t rate=pc_mod_audio_rate(wave,pitch);
    if(!rate){unlock();return 0;}
    PcModAudioVoice next;memset(&next,0,sizeof next);uint64_t now=now_ms();
    if(recomp_dsound_buffer_configure(&next.model,wave->bytes,wave->rate,wave->channels*2u,now)!=RECOMP_DSOUND_OK ||
       recomp_dsound_buffer_set_frequency(&next.model,rate,now)!=RECOMP_DSOUND_OK ||
       recomp_dsound_buffer_play(&next.model,RECOMP_DSOUND_PLAY_FROMSTART|(looping?RECOMP_DSOUND_PLAY_LOOPING:0u),now)!=RECOMP_DSOUND_OK) {
        unlock();return 0;
    }
    next.wave=wave;next.volume=volume;next.last_level=INT32_MAX;next.output=next.model;
    pc_mod_audio_clear_locked(index);pc_mod_audio_voices[index]=next;pc_mod_audio_feed_locked(index,now);
    unlock();return 1;
}
int pc_mod_audio_pause(uint32_t index,int paused) {
    if(index>=TSFP_MOD_AUDIO_VOICES || (paused!=0 && paused!=1))return 0;
    lock();PcModAudioVoice *voice=&pc_mod_audio_voices[index];
    if(!voice->wave){unlock();return 0;}
    if(paused!=voice->paused) {
        uint64_t now=now_ms();
        if(paused) {
            (void)recomp_dsound_buffer_cursor(&voice->model,now);
            voice->paused_flags=voice->model.playing?voice->model.play_flags:0;
            // FROMSTART is only a start command, never a resume instruction.
            voice->paused_flags&=~RECOMP_DSOUND_PLAY_FROMSTART;
            if(voice->model.playing)voice->paused_flags|=0x80000000u;
            recomp_dsound_buffer_stop(&voice->model,now);
        } else if(voice->paused_flags&0x80000000u) {
            (void)recomp_dsound_buffer_play(&voice->model,voice->paused_flags&~0x80000000u,now);
        }
        voice->paused=paused;voice->output=voice->model;
        recomp_audio_output_reset_voice(RECOMP_AUDIO_SLOT_MOD_FIRST+index);
        if(!paused)pc_mod_audio_feed_locked(index,now);
    }
    unlock();return 1;
}
int pc_mod_audio_pitch(uint32_t index,uint32_t pitch) {
    if(index>=TSFP_MOD_AUDIO_VOICES)return 0;
    lock();PcModAudioVoice *voice=&pc_mod_audio_voices[index];uint32_t rate=pc_mod_audio_rate(voice->wave,pitch);
    if(!rate){unlock();return 0;}
    uint64_t now=now_ms();(void)recomp_dsound_buffer_set_frequency(&voice->model,rate,now);
    voice->output=voice->model;recomp_audio_output_reset_voice(RECOMP_AUDIO_SLOT_MOD_FIRST+index);
    if(!voice->paused)pc_mod_audio_feed_locked(index,now);
    unlock();return 1;
}
int pc_mod_audio_volume(uint32_t index,int32_t volume) {
    if(index>=TSFP_MOD_AUDIO_VOICES || volume< -10000 || volume>0)return 0;
    lock();PcModAudioVoice *voice=&pc_mod_audio_voices[index];
    if(!voice->wave){unlock();return 0;}
    voice->volume=volume;
    int32_t level=pc_audio_mix_level(volume,PC_AUDIO_EFFECTS);
    if(recomp_audio_output_set_volume(RECOMP_AUDIO_SLOT_MOD_FIRST+index,level))voice->last_level=level;
    unlock();return 1;
}
void pc_mod_audio_stop(uint32_t index) {
    if(index>=TSFP_MOD_AUDIO_VOICES)return;
    lock();pc_mod_audio_clear_locked(index);unlock();
}
void pc_mod_audio_stop_all(void) {
    lock();for(uint32_t i=0;i<TSFP_MOD_AUDIO_VOICES;++i)pc_mod_audio_clear_locked(i);unlock();
}
static void pc_mod_audio_tick_locked(uint64_t now) {
    if(now>=pc_mod_audio_last_tick && now-pc_mod_audio_last_tick<10u)return;
    pc_mod_audio_last_tick=now;
    int ready=pc_mod_bootstrap_source(NULL)!=NULL;
    for(uint32_t i=0;i<TSFP_MOD_AUDIO_VOICES;++i) {
        if(!ready)pc_mod_audio_clear_locked(i);else pc_mod_audio_feed_locked(i,now);
    }
}
