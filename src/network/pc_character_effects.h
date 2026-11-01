#ifndef TSFP_PC_CHARACTER_EFFECTS_H
#define TSFP_PC_CHARACTER_EFFECTS_H
#include <stdint.h>
#include <string.h>
#include <math.h>
/* Splitter body fade at actor+C68: only numeric flags and offsets20..50.
   The actor pointer, sound handles and other effect ownership remain local. */
typedef struct PcCharacterCamo {
    uint32_t flags,fade_phase;
    float fade_values[7];
    uint32_t cycle_phase;
    float cycle_values[4];
} PcCharacterCamo;
#define PC_CHARACTER_CAMO_FLAGS 0x01000010u
static inline int pc_character_camo_valid(const PcCharacterCamo *s)
{
    if((s->flags&~PC_CHARACTER_CAMO_FLAGS) || s->fade_phase>2u || s->cycle_phase>2u) return 0;
    for(unsigned i=0;i<7u;i++) if(!isfinite(s->fade_values[i]) || fabsf(s->fade_values[i])>1000000.f) return 0;
    for(unsigned i=0;i<4u;i++) if(!isfinite(s->cycle_values[i]) || fabsf(s->cycle_values[i])>1000000.f) return 0;
    /* Original11F780 divides by effect+2C in fading phases0/2. */
    return !(s->flags&0x10u) || s->fade_phase==1u || s->fade_values[2]>0.f;
}
static inline void pc_character_camo_capture(PcCharacterCamo *s,const uint8_t *actor)
{
    memcpy(&s->flags,actor+0xc6cu,4u);s->flags&=PC_CHARACTER_CAMO_FLAGS;
    if(!s->flags) {memset(s,0,sizeof *s);return;}
    memcpy(&s->fade_phase,actor+0xc88u,52u);
}
static inline void pc_character_camo_apply(uint8_t *actor,const PcCharacterCamo *s)
{
    uint32_t flags;memcpy(&flags,actor+0xc6cu,4u);
    flags=(flags&~PC_CHARACTER_CAMO_FLAGS)|s->flags;
    memcpy(actor+0xc6cu,&flags,4u);memcpy(actor+0xc88u,&s->fade_phase,52u);
}
#endif
