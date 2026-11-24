/* A7870 advances HUD components only for bodies in its update list with +78=0.
   Online guests can own a human body outside that list. Damage still starts
   the HUD's two-second display, but its timer/segment flashes then never age.
   Supplement only the missing native update; never run damage or AI here. */
#ifdef _WIN32
extern void sub_0013A2C0(void);
static void story_human_hud_frame(void)
{
    static uint32_t last_frame,last_match,last_epoch,last_scene;
    static int updated;
    if(!story_campaign_gameplay() || WORD_AT(0x76b140u)!=2u ||
       WORD_AT(0x7de30cu)!=7u || story_cine_state.active ||
       (WORD_AT(0x7de45cu)&0x02000000u) || (int32_t)WORD_AT(0x7497b4u)>0) {
        updated=0;return;
    }
    const unsigned count=WORD_AT(0x790950u);
    const uint32_t contexts=WORD_AT(0x7b0c48u),actors=WORD_AT(0x7356d8u);
    if(count<2u || count>4u || count!=story_npc_humans() || !contexts || !actors ||
       !guest_writable(contexts,count*0x1584u))return;
    /* Guest frames remain distinct when native beta rates share a 60-Hz tick. */
    const uint32_t frame=WORD_AT(0x7497b8u),match=story_npc_match(),scene=WORD_AT(0x79094cu);
    if(updated && last_frame==frame && last_match==match && last_epoch==story_reload_epoch && last_scene==scene)return;
    uint32_t bodies[4]={0};unsigned native=0;
    for(unsigned slot=0;slot<count;slot++)bodies[slot]=WORD_AT(contexts+slot*0x1584u+0x14u);
    uint32_t node=WORD_AT(0x7b4cc0u);unsigned visited=0;
    for(;node && visited<4096u;visited++) {
        if(!guest_writable(node,0x7cu))return;
        if(!WORD_AT(node+0x78u))for(unsigned slot=0;slot<count;slot++)if(node==bodies[slot])native|=1u<<slot;
        node=WORD_AT(node);
    }
    if(node)return; /* Incomplete/cyclic list: do not risk a second update. */
    updated=1;last_frame=frame;last_match=match;last_epoch=story_reload_epoch;last_scene=scene;
    GuestRegisters saved;save_registers(&saved);
    for(unsigned slot=0;slot<count;slot++) {
        const uint32_t context=contexts+slot*0x1584u,actor=actors+slot*0xea0u,body=bodies[slot];
        if((native&(1u<<slot)) || WORD_AT(context+4u)>=4u || WORD_AT(context)!=slot ||
           WORD_AT(context+0x114u)!=2u || (WORD_AT(context+0xcu)&0x8000u) ||
           !guest_writable(actor,0xea0u) || WORD_AT(actor)!=body || WORD_AT(actor+8u)!=slot ||
           WORD_AT(actor+0x10u)!=2u || (WORD_AT(actor+0x2cu)&4u) ||
           !guest_writable(body,0x26cu) || WORD_AT(body+0x20u)!=8u ||
           WORD_AT(body+0x7cu)!=actor || WORD_AT(body+0x150u)!=context ||
           (WORD_AT(body+0x28u)&0x80000000u))continue;
        const uint32_t hud=WORD_AT(body+0x268u);
        if(!guest_writable(hud,0x144u))continue;
        guest_call(sub_0013A2C0,2,hud,body);
        if(getenv("TSFP_STORY_TRACE")) {
            static unsigned observations[4];
            if(observations[slot]++<4u)fprintf(stderr,"[PC-STORY-HUD] slot=%u missing_native=1 owner=%u hud=%08X\n",slot,WORD_AT(body+0x78u),hud);
        }
    }
    restore_registers(&saved);
}
#endif
