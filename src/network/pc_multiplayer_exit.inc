#include "pc_network_profiles.inc"
#ifdef TSFP_ARCADE_MIGRATION
#include "pc_arcade_api.h"
#endif
/* Shared Arcade Online/System Link exit routing. Keep directory membership
 * separate from the original guest match transport. Story retains its adapter. */
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
extern void sub_00065420(void),sub_00065470_gen(void),sub_002EC650_gen(void);
extern void sub_002EAF60_gen(void),sub_002EB030_gen(void),sub_002EBE60(void);
extern void sub_000668F0(void);
extern void sub_00079B70(void),sub_001CC360(void),sub_001A4350(void);
extern void sub_001A8D30(void),sub_001A8F30(void),sub_003544B0(void),sub_003535C0(void);
static unsigned multiplayer_destination; /* 1 party lobby, 3 Online/System Link menu */
static unsigned multiplayer_exit_depth;
static unsigned multiplayer_factory_observations;
static int multiplayer_lobby_pending;
static uint32_t multiplayer_profiles[16];
static unsigned multiplayer_profile_count;
static int multiplayer_arcade(void)
{
    return pc_network_enabled() && pc_online_mode()!=PC_ONLINE_STORY;
}
static int multiplayer_match(void)
{
    return multiplayer_arcade() && WORD_AT(0x79094cu)<=100u &&
        WORD_AT(0x76b108u) && (WORD_AT(0x76b140u)==1u || WORD_AT(0x76b140u)==2u);
}
#ifdef TSFP_ARCADE_MIGRATION
static int multiplayer_migrating_match(void)
{
    return multiplayer_match() && pc_online_mode()==PC_ONLINE_ARCADE &&
        WORD_AT(0x76b140u)==1u && WORD_AT(0x76b1a4u)==15u && WORD_AT(0x7de30cu)==7u &&
        pc_network_arcade_enabled();
}
uint32_t pc_multiplayer_migration_prompt(const char *text)
{
    if(!text || !(strstr(text,"LEAVING THE MATCH") || strstr(text,"Leaving the match")) ||
       !multiplayer_migrating_match())return 0;
    static uint32_t prompt;
    if(!prompt) {
        const char message[]="Leave this match?\nHosting will transfer to an available player.";
        prompt=xbox_HeapAlloc(sizeof message,4u);
        if(prompt)memcpy((void *)((uintptr_t)prompt+g_xbox_mem_offset),message,sizeof message);
    }
    return prompt;
}
#endif
static void multiplayer_leave(unsigned destination,int close_menu)
{
    multiplayer_destination=destination;multiplayer_factory_observations=0;
    fprintf(stderr,"[PC-MULTIPLAYER-EXIT] begin destination=%u role=%u phase=%u level=%u\n",
        destination,WORD_AT(0x76b140u),WORD_AT(0x76b1a4u),WORD_AT(0x79094cu));
    multiplayer_lobby_pending=destination==1u;
    WORD_AT(0x78ad20u)=13u; /* Retained network frontend. */
    WORD_AT(0x78ad24u)|=0x10000u;
    if(destination==1u && WORD_AT(0x76b140u)==1u) WORD_AT(0x774178u)=0;
    GuestRegisters saved;save_registers(&saved);
    if(WORD_AT(0x7de30cu)==7u) guest_call(sub_000668F0,0);
    const uint32_t menu=WORD_AT(0x761bbcu);
    if(close_menu && guest_writable(menu,8u)) guest_call(sub_00079B70,2,menu,0u);
    ++multiplayer_exit_depth;
    guest_call(sub_002EC650_gen,0);
    --multiplayer_exit_depth;
    restore_registers(&saved);
}
static void multiplayer_leave_confirm(void)
{
    const uint32_t menu=WORD_AT(g_esp+4u);
    if(multiplayer_match() && WORD_AT(0x76b140u)==1u && guest_writable(menu,0x190u) && WORD_AT(menu+0x18cu)==0x40u) {
#ifdef TSFP_ARCADE_MIGRATION
        if(WORD_AT(0x7de30cu)==7u && WORD_AT(0x76b1a4u)==15u &&
           pc_network_arcade_request_leave()) {WORD_AT(0x789640u)=0;g_eax=0;g_esp+=4u;return;}
#endif
        multiplayer_leave(1u,1);
        WORD_AT(0x789640u)=0;g_eax=0;g_esp+=4u;return;
    }
    sub_002EBE60();
}
static void multiplayer_lobby_tick(void (*original)(void))
{
    if(multiplayer_lobby_pending && WORD_AT(0x79094cu)==101u) {
        GuestRegisters saved;save_registers(&saved);
        for(unsigned i=0;i<2u && (WORD_AT(0x76b1a4u)==16u || WORD_AT(0x76b1a4u)==17u);++i)
            guest_call(sub_003544B0,0);
        if(WORD_AT(0x76b1a4u)==11u || WORD_AT(0x76b1a4u)==12u) {
            guest_call(sub_003535C0,0);
            multiplayer_lobby_pending=0;
        }
        restore_registers(&saved);
    }
    original();
}
static void multiplayer_host_tick(void){multiplayer_lobby_tick(sub_001A8D30);}
static void multiplayer_guest_tick(void){multiplayer_lobby_tick(sub_001A8F30);}
#endif
static void (*multiplayer_exit_lookup(uint32_t address))(void)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    /* This resolver sees every indirect game call. Unrelated addresses must
     * not inspect networking or guest lobby state. */
    if(address!=0x2ebe60u && address!=0x1a8d30u && address!=0x1a8f30u)return NULL;
    if(multiplayer_match() && WORD_AT(0x76b140u)==1u && address==0x2ebe60u) return multiplayer_leave_confirm;
    if(multiplayer_arcade() && WORD_AT(0x79094cu)==101u) {
        if(multiplayer_lobby_pending && address==0x1a8d30u) return multiplayer_host_tick;
        if(multiplayer_lobby_pending && address==0x1a8f30u) return multiplayer_guest_tick;
    }
#else
    (void)address;
#endif
    return NULL;
}
static int multiplayer_exit_button(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(multiplayer_match() && WORD_AT(0x76b140u)==1u && guest_writable(g_esp,28u)) {
        const uint32_t callback=WORD_AT(g_esp+20u);
        if(callback==0x2ec060u || callback==0x2ec030u || callback==0x2eaf60u || callback==0x2eb030u) {
            static uint32_t captions[2];unsigned migration=0;
#ifdef TSFP_ARCADE_MIGRATION
            migration=multiplayer_migrating_match();
#endif
            if(!captions[migration]) {
                const char *text=migration?"Leave Match":"Return to Lobby";
                captions[migration]=xbox_HeapAlloc((uint32_t)strlen(text)+1u,4u);
                if(captions[migration]) memcpy((void *)((uintptr_t)captions[migration]+g_xbox_mem_offset),text,strlen(text)+1u);
            }
            const uint32_t caption=captions[migration];
            const uint32_t frame=g_esp,old=WORD_AT(frame+4u);
            if(caption) WORD_AT(frame+4u)=caption;
            original();WORD_AT(frame+4u)=old;return 1;
        }
    }
#else
    (void)original;
#endif
    return 0;
}
void pc_multiplayer_exit_action(void (*original)(void),unsigned action)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(!action && multiplayer_match() && WORD_AT(0x76b140u)==1u &&
       (original==sub_002EAF60_gen || original==sub_002EB030_gen)) {
#ifdef TSFP_ARCADE_MIGRATION
        if(WORD_AT(0x7de30cu)==7u && WORD_AT(0x76b1a4u)==15u &&
           pc_network_arcade_request_leave()) {g_eax=0;g_esp+=4u;return;}
#endif
        multiplayer_leave(1u,1);
        g_eax=0;g_esp+=4u;return;
    }
#endif
    pc_story_network_result_action(original,action);
}
#ifdef TSFP_ARCADE_MIGRATION
void pc_multiplayer_migration_leave(void)
{
    if(!multiplayer_match())return;
    multiplayer_leave(3u,1);
    GuestRegisters saved;save_registers(&saved);
    guest_call(sub_00065420,0);restore_registers(&saved);
}
#endif
void pc_multiplayer_return_lobby(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(multiplayer_match() && WORD_AT(0x76b140u)==2u && !WORD_AT(0x76b178u)) {
        multiplayer_leave(1u,0);g_eax=0;g_esp+=4u;return;
    }
#endif
    pc_story_network_return_lobby(original);
}
void pc_multiplayer_disconnect_exit(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    if(multiplayer_match()) fprintf(stderr,"[PC-MULTIPLAYER-EXIT] disconnect caller=%08X depth=%u pending=%u role=%u phase=%u\n",
        guest_writable(g_esp,4u)?WORD_AT(g_esp):0u,multiplayer_exit_depth,multiplayer_destination,WORD_AT(0x76b140u),WORD_AT(0x76b1a4u));
    if(multiplayer_match() && !multiplayer_exit_depth && !multiplayer_destination &&
       (WORD_AT(0x76b1a4u)==15u || WORD_AT(0x76b1a4u)==16u) &&
       guest_writable(g_esp,4u) && WORD_AT(g_esp)==0x35c360u) {
        multiplayer_destination=3u;multiplayer_lobby_pending=0;
        WORD_AT(0x78ad20u)=13u;
    }
#endif
    pc_story_network_disconnect_exit(original);
}
void pc_multiplayer_exit_menu(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    const uint32_t frame=g_esp;
    if(multiplayer_destination && WORD_AT(0x79094cu)==101u && guest_writable(frame,16u) && multiplayer_factory_observations++<16u)
        fprintf(stderr,"[PC-MULTIPLAYER-EXIT] factory destination=%u parent=%08X root=%08X descriptor=%08X role=%u phase=%u\n",
            multiplayer_destination,WORD_AT(frame+4u),WORD_AT(0x7613e4u),WORD_AT(frame+8u),WORD_AT(0x76b140u),WORD_AT(0x76b1a4u));
    if(multiplayer_destination && WORD_AT(0x79094cu)==101u && guest_writable(frame,16u) &&
       WORD_AT(frame+4u)==WORD_AT(0x7613e4u) && WORD_AT(0x7613e4u) &&
       (WORD_AT(frame+8u)==0x4e99c4u || WORD_AT(frame+8u)==0x4e99d4u ||
        WORD_AT(frame+8u)==0x4e9404u || WORD_AT(frame+8u)==0x4e93f4u ||
        WORD_AT(frame+8u)==0x4e9584u || WORD_AT(frame+8u)==0x4e9974u ||
        WORD_AT(frame+8u)==0x4e92c4u || WORD_AT(frame+8u)==0x4e9574u)) {
        const unsigned destination=multiplayer_destination;
        const uint32_t old=WORD_AT(frame+8u);
        multiplayer_destination=0;
        if(destination!=1u) {
            GuestRegisters saved;save_registers(&saved);
            guest_call(sub_001CC360,0);restore_registers(&saved);
            if(destination==3u) pc_online_disconnect();
        }
        WORD_AT(frame+8u)=destination==3u?0x4e92c4u:
            (WORD_AT(0x76b140u)==1u?0x4e99c4u:0x4e99d4u);
        original();WORD_AT(frame+8u)=old;return;
    }
#endif
    pc_story_network_rewards_menu(original);
}

void pc_multiplayer_lobby_lost(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    const uint32_t frame=g_esp;
    if(multiplayer_arcade() && WORD_AT(0x79094cu)==101u && guest_writable(frame,12u) &&
       (WORD_AT(frame)==0x1cc4d9u || WORD_AT(frame)==0x1cc552u) &&
       !WORD_AT(frame+4u) && WORD_AT(frame+8u)==0x8du) {
        multiplayer_destination=3u;multiplayer_lobby_pending=0;
    }
#endif
    pc_story_network_lobby_lost(original);
}

static void multiplayer_profiles_capture(void)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    network_profiles_capture(multiplayer_profiles,&multiplayer_profile_count);
#endif
}
static int multiplayer_profile_save(void (*original)(void))
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    /* The lobby factory consumes the exit destination before subsequent stock
       frontend saves. Profiles still occupy network slots while the party is
       connected, so their persistent save view must outlive that route. */
    if(multiplayer_arcade() && (multiplayer_destination ||
       (WORD_AT(0x76b108u) && (WORD_AT(0x76b140u)==1u || WORD_AT(0x76b140u)==2u)))) {
        const uint32_t frame=g_esp;
        static unsigned observations;
        const unsigned dirty=WORD_AT(0x78ad24u)&0x10000u;
        if(dirty && observations++<16u) {
            fprintf(stderr,"[PC-PROFILE-SAVE] begin role=%u phase=%u scene=%u profiles=%u captured=%u destination=%u\n",
                WORD_AT(0x76b140u),WORD_AT(0x76b1a4u),WORD_AT(0x79094cu),WORD_AT(0x74e934u),multiplayer_profile_count,multiplayer_destination);
            for(unsigned i=0;i<multiplayer_profile_count && i<16u;i++) {
                const uint32_t metadata=multiplayer_profiles[i];
                if(guest_writable(metadata,0x40u)) fprintf(stderr,
                    "[PC-PROFILE-SAVE] local=%u metadata=%08X device=%u crc=%08X temporary=%u body=%08X\n",
                    i,metadata,WORD_AT(metadata+0x24u),WORD_AT(metadata+0x28u),WORD_AT(metadata+0x34u),WORD_AT(metadata+0x3cu));
            }
        }
        network_profiles_save(original,multiplayer_profiles,multiplayer_profile_count);
        if(dirty && g_eax) {
            const unsigned slot=WORD_AT(0x74f9dcu);
            uint32_t crc=UINT32_MAX;
            if(frame>=0x1f54u && guest_writable(frame-0x1f54u,0x1f54u)) {
                const uint8_t *body=(const uint8_t *)(g_xbox_mem_offset+(uintptr_t)frame-0x1f54u);
                for(unsigned i=0;i<0x1f54u;i++) {
                    crc^=body[i];
                    for(unsigned bit=0;bit<8u;bit++) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
                }
                crc=~crc;
            }
            fprintf(stderr,"[PC-PROFILE-SAVE] error=%u kind=%u slot=%u scratch_crc=%08X frame=%08X\n",
                WORD_AT(0x74f9d8u),WORD_AT(0x74e948u),slot,crc,frame);
        }
        return 1;
    }
#else
    (void)original;
#endif
    return 0;
}
