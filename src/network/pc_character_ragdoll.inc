/* Dynamic death clips E5..E7 contain locally generated ragdoll rotations.
   Replicate numeric rigid poses, then regenerate a separately allocated local
   clip. Never use the host's pool slot, collision handles or bone pointers. */
typedef struct PcLocalRagdoll {uint32_t entity,spawn,group;} PcLocalRagdoll;
static PcLocalRagdoll pc_local_ragdolls[32];
static uint32_t pc_ragdoll_actor(uint32_t entity)
{
    if(!guest_writable(entity,0x2e8u) || WORD_AT(entity+0x20u)!=8u) return 0;
    const uint32_t actor=WORD_AT(entity+0x7cu);
    return guest_writable(actor,0xea0u) && WORD_AT(actor)==entity && WORD_AT(actor+8u)<32u?actor:0;
}
static int pc_ragdoll_group_valid(uint32_t group)
{
    if(!guest_writable(group,0x48u)) return 0;
    const uint32_t kind=WORD_AT(group+4u);
    return kind<7u && WORD_AT(group)>=229u && WORD_AT(group)<232u;
}
static uint32_t pc_ragdoll_owned(uint32_t entity,uint32_t actor)
{
    PcLocalRagdoll *local=&pc_local_ragdolls[WORD_AT(actor+8u)];
    return local->entity==entity && local->spawn==WORD_AT(entity+0x258u) &&
        local->group==WORD_AT(actor+0x4cu) && pc_ragdoll_group_valid(local->group)?local->group:0;
}
void pc_character_ragdoll_capture(PcRagdollState *state,uint32_t entity)
{
    memset(state,0,sizeof *state);
    const uint32_t actor=pc_ragdoll_actor(entity),group=actor?WORD_AT(actor+0x4cu):0;
    if(!pc_ragdoll_group_valid(group)) return;
    state->kind=WORD_AT(group+4u);
    state->count=WORD_AT(0x0053fa58u+state->kind*0x4dcu);
    if(state->count>PC_RAGDOLL_PARTS) {memset(state,0,sizeof *state);return;}
    for(unsigned i=0;i<state->count;i++) pc_rigid_body_capture(&state->parts[i],WORD_AT(group+8u+i*4u));
    if(!pc_ragdoll_valid(state)) memset(state,0,sizeof *state);
}
void pc_character_ragdoll_present(uint32_t entity,const PcRagdollState *state)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    extern void sub_00351580(void),sub_003515A0(void),sub_00352200(void),sub_00351F60(void);
    const uint32_t actor=pc_ragdoll_actor(entity);
    if(!actor || !pc_ragdoll_valid(state)) return;
    uint32_t group=pc_ragdoll_owned(entity,actor);
    PcLocalRagdoll *local=&pc_local_ragdolls[WORD_AT(actor+8u)];
    if(group && (!state->count || WORD_AT(group+4u)!=state->kind)) {
        guest_call(sub_00351F60,1,actor+0x4cu);memset(local,0,sizeof *local);group=0;
    }
    if(!state->count) return;
    if(!group) {
        /* An existing unowned group belongs to the original engine. */
        if(WORD_AT(actor+0x4cu)) return;
        const uint32_t clip=guest_call(sub_00351580,0);
        if(clip<229u || clip>=232u) return;
        group=guest_call(sub_00352200,3,entity,clip,state->kind);
        if(!pc_ragdoll_group_valid(group)) {guest_call(sub_003515A0,1,clip);return;}
        WORD_AT(actor+0x4cu)=group;
        *local=(PcLocalRagdoll){entity,WORD_AT(entity+0x258u),group};
    }
    for(unsigned i=0;i<state->count;i++) {
        const uint32_t part=WORD_AT(group+8u+i*4u);
        pc_rigid_body_present(part,&state->parts[i]);
        /* Host collision/settling owns these poses. Another guest solve adds
           gravity after the host has settled or stopped publishing the actor. */
        pc_rigid_body_pause(part);
    }
    pc_character_ragdoll_evaluate(entity);
#else
    (void)entity;(void)state;
#endif
}
void pc_character_ragdoll_release(uint32_t entity)
{
    const PcRagdollState absent={0};pc_character_ragdoll_present(entity,&absent);
}
void pc_character_ragdoll_evaluate(uint32_t entity)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    extern void sub_003513C0(void);
    const uint32_t actor=pc_ragdoll_actor(entity),group=actor?pc_ragdoll_owned(entity,actor):0;
    /* 351F30 skips sleeping roots; build the generated clip directly from
       the received physical pose even though guest integration is paused. */
    if(group) guest_call(sub_003513C0,2,entity,group);
#else
    (void)entity;
#endif
}
void pc_character_ragdoll_preserve(uint32_t entity,uint32_t corpse)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    extern void sub_000B2E50(void),sub_00351F60(void);
    const uint32_t actor=pc_ragdoll_actor(entity),group=actor?pc_ragdoll_owned(entity,actor):0;
    if(!group || !guest_writable(corpse,0x2e8u) || WORD_AT(corpse+0x20u)!=0x1000u ||
       !guest_writable(WORD_AT(corpse+0x7cu),0x60u)) return;
    const uint32_t context=WORD_AT(corpse+0x7cu),capacity=WORD_AT(0x00732a10u);
    if(context<0x00732a18u || (context-0x00732a18u)%96u || !capacity || capacity>8u) return;
    const uint32_t index=(context-0x00732a18u)/96u;
    if(index>=capacity || WORD_AT(0x007b2e20u+index*4u)!=corpse) return;
    /* Mirror original3521C0's final-pose path. B2E50(...,0) bakes into the
       corpse-ring clip232+slot without transferring a live physical group.
       It returns the temporary NPC clip before351F60 frees the local parts. */
    guest_call(sub_000B2E50,3,corpse,group,0u);
    guest_call(sub_00351F60,1,actor+0x4cu);
    memset(&pc_local_ragdolls[WORD_AT(actor+8u)],0,sizeof pc_local_ragdolls[0]);
#else
    (void)entity;(void)corpse;
#endif
}
int32_t pc_character_ragdoll_clip(uint32_t entity,int32_t clip)
{
    if(clip<229 || clip>=232) return clip;
    const uint32_t actor=pc_ragdoll_actor(entity),group=actor?pc_ragdoll_owned(entity,actor):0;
    return group?(int32_t)WORD_AT(group):-1;
}
