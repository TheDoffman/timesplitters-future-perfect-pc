/* Generic world removal does not own the actor roster or held models.
 * Guest bot replicas bypass the stock AI retirement that releases them. */
void pc_network_bots_remove_body(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    extern void sub_0015BCC0(void),sub_000A9070(void);
    const uint32_t frame=g_esp,pool=WORD_AT(0x7356d8u);
    const unsigned humans=*(const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
    const unsigned bots=*(const uint8_t *)((uintptr_t)0x7de402u+g_xbox_mem_offset);
    const uint32_t body=guest_writable(frame,8u)?WORD_AT(frame+4u):0u;
    uint32_t actor=0;unsigned slot=16u;
    if(pc_online_mode()==PC_ONLINE_ARCADE && WORD_AT(0x76b140u)==2u &&
       WORD_AT(0x76b108u) && WORD_AT(0x76b1a4u)==15u && humans && bots &&
       bots<=10u && humans+bots<=16u && pool &&
       guest_writable(pool,(humans+bots)*0xea0u) && guest_writable(body,0x25cu) &&
       WORD_AT(body+0x20u)==8u && WORD_AT(0x74c37cu)<=32u) {
        const uint32_t owner=WORD_AT(body+0x7cu);
        for(unsigned i=humans;i<humans+bots;i++)
            if(owner==pool+i*0xea0u && WORD_AT(owner)==body) {actor=owner;slot=i;break;}
    }
    if(actor) {
        /* Remove before destruction, while +7C still identifies the owner.
           Original removal is harmless when stock retirement did it first. */
        guest_call(sub_0015BCC0,1,body);
        for(unsigned hand=0;hand<2u;hand++) {
            const uint32_t record=actor+0xa4u+hand*0x130u;
            const uint32_t attachment=WORD_AT(record+0xe8u);
            if(attachment==body || !guest_writable(attachment,0x154u) ||
               WORD_AT(attachment+0x20u)!=0x20u ||
               WORD_AT(attachment+0x7cu)!=record || WORD_AT(record+0xecu)!=actor)continue;
            if(!(WORD_AT(attachment+0x28u)&0x80000000u))guest_call(sub_000A9070,1,attachment);
            if(WORD_AT(record+0xe8u)==attachment)WORD_AT(record+0xe8u)=0;
        }
    }
    original();
    /* A destructor callback can replace storage. Never clear its new owner. */
    if(actor && WORD_AT(0x7356d8u)==pool && WORD_AT(actor)==body) {
        WORD_AT(actor)=0;
        if(WORD_AT(0x7a2d00u+(slot-humans)*4u)==body)
            WORD_AT(0x7a2d00u+(slot-humans)*4u)=0;
    }
#else
    original();
#endif
}

static void pc_network_bots_register_body(uint32_t actor,uint32_t body)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    extern void sub_0015BCA0(void);
    const unsigned count=WORD_AT(0x74c37cu);
    if(count>=32u)return;
    for(unsigned i=0;i<count;i++)if(WORD_AT(0x7a3580u+i*4u)==actor)return;
    guest_call(sub_0015BCA0,1,body);
#else
    (void)actor;(void)body;
#endif
}
