/* Reserve storage at round construction; construct remote players on demand.
 * All engine pointers remain local and never enter checkpoints or the wire. */
#ifdef TSFP_ARCADE_MIGRATION
#include "pc_arcade_api.h"
#include "pc_arcade_join.h"
extern void sub_0015BCA0(void),sub_000D2FD0_gen(void);
extern void sub_0015DDA0(void);
static uint32_t arcade_join_context_pool,arcade_join_actor_pool;
static uint32_t arcade_join_owners[16],arcade_join_team_mask;
static uint8_t arcade_join_teams[16];
static unsigned arcade_join_initial_humans,arcade_join_initial_bots;
static int arcade_join_allocating,arcade_join_construct_slot=-1,arcade_join_construct_active;

static int arcade_join_experimental(void)
{
    const char *value=getenv("TSFP_ARCADE_JOIN_IN_PROGRESS");
    return value && !strcmp(value,"1") && pc_online_mode()==PC_ONLINE_ARCADE;
}
int pc_arcade_join_initial_construct(void (*original)(void))
{
    if(!arcade_join_allocating)return 0;
    uint8_t *config=(uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
    WORD_AT(0x790950u)=arcade_join_initial_humans;
    WORD_AT(0x7a2958u)=arcade_join_initial_bots;
    config[0]=(uint8_t)arcade_join_initial_humans;config[2]=(uint8_t)arcade_join_initial_bots;
    original();
    return 1;
}
void pc_arcade_join_allocate(void (*original)(void))
{
    const unsigned humans=WORD_AT(0x790950u),bots=WORD_AT(0x7a2958u);
    uint8_t *config=(uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
    const int scope=arcade_join_experimental() && WORD_AT(0x76b108u) &&
        humans && humans+bots<=16u && WORD_AT(0x79094cu)<=100u;
    arcade_join_context_pool=arcade_join_actor_pool=0;
    arcade_join_team_mask=0;memset(arcade_join_owners,0,sizeof arcade_join_owners);
    arcade_join_initial_humans=humans;arcade_join_initial_bots=bots;
    arcade_join_allocating=scope;
    if(scope)WORD_AT(0x790950u)=16u; /* Only D3B00's three storage allocations. */
    original();
    arcade_join_allocating=0;
    if(!scope)return;
    WORD_AT(0x790950u)=humans;WORD_AT(0x7a2958u)=bots;
    config[0]=(uint8_t)humans;config[2]=(uint8_t)bots;
    uint32_t pool=WORD_AT(0x7356d8u),contexts=WORD_AT(0x7b0c48u),aux=WORD_AT(0x7b0c78u);
    if(!pool || !contexts || !aux || !guest_writable(pool,16u*0xea0u) ||
       !guest_writable(contexts,16u*0x1584u) || !guest_writable(aux,16u*0x3e0u))return;
    for(unsigned i=humans;i<16u;i++) {
        memset((void *)((uintptr_t)(contexts+i*0x1584u)+g_xbox_mem_offset),0,0x1584u);
        memset((void *)((uintptr_t)(aux+i*0x3e0u)+g_xbox_mem_offset),0,0x3e0u);
        WORD_AT(contexts+i*0x1584u)=i;
        WORD_AT(contexts+i*0x1584u+4u)=UINT32_MAX;
        WORD_AT(contexts+i*0x1584u+0xf74u)=aux+i*0x3e0u;
    }
    arcade_join_context_pool=contexts;arcade_join_actor_pool=pool;
    for(unsigned i=0;i<16u;i++)arcade_join_owners[i]=WORD_AT(0x777244u+i*0x124u);
    if(getenv("TSFP_ARCADE_TRACE"))fprintf(stderr,"[PC-ARCADE-JOIN] reserved context storage=%08X actors=%08X humans=%u\n",contexts,pool,humans);
}
/* Original D2FD0's loop frame at D3143. Only a new remote context uses this
 * entry; ordinary construction retains its complete original prologue. */
int pc_arcade_join_context_entry(void)
{
    if(arcade_join_construct_slot<0 || arcade_join_construct_active)return 0;
    const unsigned slot=(unsigned)arcade_join_construct_slot;
    g_esp-=0x18u;push(g_ebx);push(g_esi);push(g_edi);
    WORD_AT(g_esp+0xcu)=slot;
    push(g_ebp);WORD_AT(g_esp+0x10u)=slot;WORD_AT(g_esp+0x14u)=slot*0x1584u;
    g_ebx=arcade_join_context_pool;g_edi=slot;g_esi=0;
    arcade_join_construct_active=1;
    return 1;
}
int pc_arcade_join_context_tail(void)
{
    if(!arcade_join_construct_active)return 0;
    /* Skip the global input reset at the ordinary tail. Restore exactly the
       original callee-saved registers and stack frame, including its return. */
    g_ebp=WORD_AT(g_esp);g_edi=WORD_AT(g_esp+4u);
    g_esi=WORD_AT(g_esp+8u);g_ebx=WORD_AT(g_esp+12u);g_esp+=0x2cu;
    arcade_join_construct_active=0;
    return 1;
}
static int arcade_join_retire_body(uint32_t actor,uint32_t body,unsigned humans,unsigned bots)
{
    if(!guest_writable(body,0x154u) || WORD_AT(body+0x7cu)!=actor ||
       (WORD_AT(body+0x28u)&0x80000000u))return 0;
    /* Use the complete original retirement, including navigation reservations,
       inventory and mode-object callbacks. Keep this actor row's place for its
       replacement, rather than letting registry removal swap another bot in. */
    const uint32_t count=WORD_AT(0x74c37cu),configs[]={0x7de400u,0x7de340u,0x774044u,0x78ad58u};
    uint32_t registry[32],old_humans=WORD_AT(0x790950u),old_bots=WORD_AT(0x7a2958u);
    uint8_t population[4][3];
    if(count>32u)return 0;
    memcpy(registry,(const void *)((uintptr_t)0x7a3580u+g_xbox_mem_offset),count*4u);
    for(unsigned i=0;i<4u;i++) {
        uint8_t *config=(uint8_t *)((uintptr_t)configs[i]+g_xbox_mem_offset);
        memcpy(population[i],config,3);config[0]=(uint8_t)humans;config[2]=(uint8_t)bots;
    }
    WORD_AT(0x790950u)=humans;WORD_AT(0x7a2958u)=bots;
    guest_call(sub_0015DDA0,1,body);
    WORD_AT(0x790950u)=old_humans;WORD_AT(0x7a2958u)=old_bots;
    for(unsigned i=0;i<4u;i++)memcpy((void *)((uintptr_t)configs[i]+g_xbox_mem_offset),population[i],3);
    memcpy((void *)((uintptr_t)0x7a3580u+g_xbox_mem_offset),registry,count*4u);
    WORD_AT(0x74c37cu)=count;
    return 1;
}
void pc_arcade_join_frame(void)
{
    pc_network_arcade_slots(NULL);
    if(!arcade_join_experimental() || !arcade_join_context_pool ||
       WORD_AT(0x7b0c48u)!=arcade_join_context_pool || WORD_AT(0x7356d8u)!=arcade_join_actor_pool)return;
    const unsigned old=((const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset))[0];
    const unsigned old_bots=((const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset))[2];
    const int changed=pc_arcade_join_sync(&g_xbox_mem_offset,pc_network_arcade_read,pc_network_arcade_write);
    uint32_t slots[4];
    if(pc_arcade_join_slots(&g_xbox_mem_offset,pc_network_arcade_read,slots))pc_network_arcade_slots(slots);
    const unsigned humans=((const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset))[0];
    GuestRegisters saved;save_registers(&saved);
    for(unsigned slot=0;slot<16u;slot++) {
        const uint32_t owner=WORD_AT(0x777244u+slot*0x124u);
        if(!owner){arcade_join_owners[slot]=0;arcade_join_team_mask&=~(1u<<slot);continue;}
        if(slot>=humans || owner==arcade_join_owners[slot])continue;
        arcade_join_owners[slot]=owner;
        const uint32_t actor=arcade_join_actor_pool+slot*0xea0u;
        const uint32_t previous=WORD_AT(actor);
        if(slot>=old && slot<old+old_bots) {
            arcade_join_teams[slot]=((const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset))[0x24u+slot];
            arcade_join_team_mask|=1u<<slot;
        }
        if(slot>=old) {
            if(previous && !arcade_join_retire_body(actor,previous,old,old_bots))continue;
            memset((void *)((uintptr_t)actor+g_xbox_mem_offset),0,0xea0u);
            /* D2FD0's loop clears the current-context global and writes camera
               defaults. A mid-round remote allocation must preserve the
               context/camera currently owned by the running frame. */
            const uint32_t globals[]={0x7b0c7cu,0x7b0cc0u,0x7b0cb8u,0x7b0cbcu,0x7b0c80u,0x75f5d8u};
            uint32_t values[6];
            for(unsigned i=0;i<6u;i++)values[i]=WORD_AT(globals[i]);
            arcade_join_construct_slot=(int)slot;
            guest_call(sub_000D2FD0_gen,0);
            arcade_join_construct_slot=-1;
            for(unsigned i=0;i<6u;i++)WORD_AT(globals[i])=values[i];
            const uint32_t human=WORD_AT(actor);
            if(!human)continue;
            if(slot<old+old_bots) {
                const unsigned removed=slot-old;
                for(unsigned i=removed;i+1u<old_bots;i++)
                    WORD_AT(0x7a2d00u+i*4u)=WORD_AT(0x7a2d00u+(i+1u)*4u);
                WORD_AT(0x7a2d00u+(old_bots-1u)*4u)=0;
            }
            /* A bot replacement keeps the already registered actor row. */
            if(!previous)guest_call(sub_0015BCA0,1,human);
        }
        pc_arcade_join_fresh_score(&g_xbox_mem_offset,pc_network_arcade_read,pc_network_arcade_write,slot);
    }
    for(unsigned slot=0;slot<humans;slot++)if(arcade_join_team_mask&(1u<<slot)) {
        const uint32_t team=arcade_join_teams[slot];
        WORD_AT(0x777288u+slot*0x124u)=team;
        WORD_AT(arcade_join_actor_pool+slot*0xea0u+0x84u)=team;
        const uint32_t configs[]={0x7de400u,0x7de340u,0x774044u,0x78ad58u};
        for(unsigned i=0;i<4u;i++)*((uint8_t *)((uintptr_t)(configs[i]+0x24u+slot)+g_xbox_mem_offset))=(uint8_t)team;
    }
    restore_registers(&saved);
    if(changed && getenv("TSFP_ARCADE_TRACE"))fprintf(stderr,"[PC-ARCADE-JOIN] human prefix %u -> %u\n",old,humans);
}
#endif
