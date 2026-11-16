#ifndef TSFP_MOD_AUDIO_COMMAND_H
#define TSFP_MOD_AUDIO_COMMAND_H
#include "mod_script.h"
static inline int tsfp_script_is_audio(uint32_t kind) {
    return kind>=TSFP_SCRIPT_AUDIO_PLAY && kind<=TSFP_SCRIPT_AUDIO_STOP_ALL;
}
static inline int tsfp_script_audio_data(const tsfp_script_command *c) {
    return c->resource || c->pitch || c->flags || c->volume;
}
static inline int tsfp_script_audio_shape(const tsfp_script_command *c,int stopping) {
    if(!tsfp_script_is_audio(c->kind) || c->value || c->text_size)return 0;
    if(c->kind==TSFP_SCRIPT_AUDIO_STOP_ALL)return !c->player && !tsfp_script_audio_data(c);
    if(c->player>=32)return 0;
    if(c->kind==TSFP_SCRIPT_AUDIO_STOP)return !tsfp_script_audio_data(c);
    if(stopping)return 0;
    if(c->kind==TSFP_SCRIPT_AUDIO_PLAY)
        return c->resource!=UINT32_MAX && (!c->pitch || (c->pitch>=16384u && c->pitch<=262144u)) &&
            c->volume>=-10000 && c->volume<=0 && c->flags<=1;
    if(c->kind==TSFP_SCRIPT_AUDIO_PAUSE)return !c->resource && !c->pitch && !c->volume && c->flags<=1;
    if(c->kind==TSFP_SCRIPT_AUDIO_PITCH)return !c->resource && !c->volume && !c->flags &&
        (!c->pitch || (c->pitch>=16384u && c->pitch<=262144u));
    return c->kind==TSFP_SCRIPT_AUDIO_VOLUME && !c->resource && !c->pitch && !c->flags && c->volume>=-10000 && c->volume<=0;
}
#endif
