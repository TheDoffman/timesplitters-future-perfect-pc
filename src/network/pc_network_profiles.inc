#ifndef PC_NETWORK_PROFILES_INC
#define PC_NETWORK_PROFILES_INC
/* Persistent identities survive the title moving profile bodies to peer slots. */
static inline void network_profiles_capture(uint32_t identities[16],unsigned *captured)
{
    *captured=0;
    const unsigned count=WORD_AT(0x74e934u);
    if(!count || count>16u || !guest_writable(0x7bedc0u,16u*0x1f54u)) return;
    for(unsigned i=0;i<count;i++) {
        const uint32_t metadata=WORD_AT(0x7bedc0u+i*0x1f54u);
        if(metadata<0x7bc9c0u || metadata>=0x7bedc0u || (metadata-0x7bc9c0u)%0x40u) return;
        for(unsigned previous=0;previous<i;previous++) if(identities[previous]==metadata) return;
        identities[i]=metadata;
    }
    *captured=count;
}
static inline void network_profiles_save(void (*original)(void),const uint32_t identities[16],unsigned captured_count)
{
        enum { PROFILE_BYTES=0x1f54u,PROFILE_SLOTS=16u };
        const uint32_t table=0x7bedc0u;
        const unsigned count=WORD_AT(0x74e934u);
        unsigned source[PROFILE_SLOTS],changed=0;
        if(!count || count>PROFILE_SLOTS || count!=captured_count ||
           !guest_writable(table,PROFILE_BYTES*PROFILE_SLOTS)) {
            original();return;
        }
        for(unsigned i=0;i<count;i++) {
            unsigned matches=0;source[i]=0;
            for(unsigned slot=0;slot<PROFILE_SLOTS;slot++) {
                if(WORD_AT(table+slot*PROFILE_BYTES)==identities[i]) {source[i]=slot;++matches;}
            }
            if(matches!=1u) {original();return;}
            changed|=source[i]!=i;
        }
        if(!changed) {original();return;}
        uint8_t *saved=(uint8_t *)malloc((PROFILE_SLOTS+count)*PROFILE_BYTES);
        if(!saved) {original();return;}
        uint8_t *updated=saved+PROFILE_SLOTS*PROFILE_BYTES;
        uint8_t *profiles=(uint8_t *)((uintptr_t)table+g_xbox_mem_offset);
        memcpy(saved,profiles,PROFILE_SLOTS*PROFILE_BYTES);
        for(unsigned i=0;i<count;i++) memcpy(profiles+i*PROFILE_BYTES,saved+source[i]*PROFILE_BYTES,PROFILE_BYTES);
        const uint32_t aggregate=WORD_AT(0x6f1e0cu);
        for(unsigned i=0;i<count;i++) {
            const uint32_t start=table+source[i]*PROFILE_BYTES;
            if(aggregate>=start && aggregate-start<PROFILE_BYTES)
                WORD_AT(0x6f1e0cu)=table+i*PROFILE_BYTES+(aggregate-start);
        }
        original();
        WORD_AT(0x6f1e0cu)=aggregate;
        memcpy(updated,profiles,count*PROFILE_BYTES);
        memcpy(profiles,saved,PROFILE_SLOTS*PROFILE_BYTES);
        for(unsigned i=0;i<count;i++) memcpy(profiles+source[i]*PROFILE_BYTES,updated+i*PROFILE_BYTES,PROFILE_BYTES);
        free(saved);return;
}
#endif
