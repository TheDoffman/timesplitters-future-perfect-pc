/* A promoted replica keeps its actor pool, but stock AI reinitializes its hand
 * records. Retire the replica's owned cosmetic guns before those handles are
 * overwritten. Never run inventory/death routines or create dropped pickups. */
void pc_network_bots_promote(void)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    extern void sub_000A9070(void);
    if(pc_online_mode()!=PC_ONLINE_ARCADE || WORD_AT(0x76b140u)!=1u ||
       !WORD_AT(0x76b108u) || WORD_AT(0x76b1a4u)!=15u) return;
    const unsigned humans=*(const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
    const unsigned bots=*(const uint8_t *)((uintptr_t)0x7de402u+g_xbox_mem_offset);
    const uint32_t pool=WORD_AT(0x7356d8u);
    if(!humans || humans>16u || !bots || bots>10u || humans+bots>16u ||
       !pool || !guest_writable(pool,16u*0xea0u)) return;
    for(unsigned slot=humans;slot<humans+bots;slot++) {
        const uint32_t actor=pool+slot*0xea0u,body=WORD_AT(actor);
        for(unsigned hand=0;hand<2u;hand++) {
            const uint32_t record=actor+0xa4u+hand*0x130u;
            const uint32_t attachment=WORD_AT(record+0xe8u);
            if(attachment==body || !guest_writable(attachment,0x154u) ||
               WORD_AT(attachment+0x20u)!=0x20u ||
               WORD_AT(attachment+0x7cu)!=record || WORD_AT(record+0xecu)!=actor) continue;
            if(!(WORD_AT(attachment+0x28u)&0x80000000u)) guest_call(sub_000A9070,1,attachment);
            if(WORD_AT(record+0xe8u)==attachment) WORD_AT(record+0xe8u)=0;
        }
    }
#endif
}
