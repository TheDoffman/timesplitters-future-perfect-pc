/* The original dead-state path copies a posed NPC into B3630's bounded
   cosmetic corpse ring before retiring its actor. Guests do not run that AI
   path. Mirror only the local corpse copy when a dead replica is retired. */
extern void sub_000B3630(void);
static int pc_character_preserve_corpse(uint32_t actor,uint32_t entity)
{
    const uint32_t frame=g_esp,capacity=WORD_AT(0x00732a10u);
    if(!capacity || capacity>8u || WORD_AT(0x00732d18u)>=capacity || frame<1024u ||
       !guest_writable(frame-1024u,1024u) || !guest_writable(entity,0x2e8u) ||
       !guest_writable(actor,0xea0u) || WORD_AT(entity+0x20u)!=8u ||
       WORD_AT(entity+0x7cu)!=actor || WORD_AT(actor)!=entity ||
       (WORD_AT(entity+0x28u)&0x80000000u)) return 0;
    float health;memcpy(&health,(const void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),4u);
    const uint32_t model=WORD_AT(entity+4u);
    if(!isfinite(health) || health>0.f || !guest_writable(model,0x200u)) return 0;
    PcCharacterPose pose;pc_character_pose_read(&pose,(const void *)((uintptr_t)model+g_xbox_mem_offset),(const void *)((uintptr_t)actor+g_xbox_mem_offset));
    if(!pc_character_pose_valid(&pose)) return 0;

    /* B3630 reads the local authored descriptor's corpse flag at+58. A
       network-created actor can lack that descriptor. Supply a zeroed local
       descriptor above the callee's scratch stack, without copying host pointers.
       Its final3EF callback must not run a second Story AI/script transition:
       27CBD0 handles only event5, and zero life/callback bypasses27CEF0/27CB10. */
    const uint32_t descriptor=WORD_AT(actor+0xbf0u),callback=WORD_AT(actor+0x74u);
    const uint32_t life0=WORD_AT(actor+0x10u),life1=WORD_AT(actor+0x14u);
    if(!guest_writable(descriptor,0x60u)) {
        memset((void *)((uintptr_t)(frame-256u)+g_xbox_mem_offset),0,0xa0u);
        WORD_AT(actor+0xbf0u)=frame-256u;
    }
    WORD_AT(actor+0x74u)=0;WORD_AT(actor+0x10u)=0;WORD_AT(actor+0x14u)=0;
    g_esp=frame-512u;
    const uint32_t corpse=guest_call(sub_000B3630,1,entity);
    g_esp=frame;
    WORD_AT(actor+0xbf0u)=descriptor;WORD_AT(actor+0x74u)=callback;
    WORD_AT(actor+0x10u)=life0;WORD_AT(actor+0x14u)=life1;
    if(getenv("TSFP_STORY_TRACE")) {
        static unsigned observations;
        if(observations++<32u) fprintf(stderr,"[PC-STORY-CORPSE] slot=%u body=%08x corpse=%08x\n",
            WORD_AT(actor+8u),entity,corpse);
    }
    return corpse!=0u;
}
