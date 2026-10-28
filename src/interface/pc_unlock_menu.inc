/* Shared US frontend and access-check hooks. No earned progress is changed. */
#include "pc_unlock_content.h"
#define UC_OPEN 0x1237bu
#define UC_BUILD 0x1237cu
#define UC_TICK 0x1237du
#define UC_BACK 0x1237eu
#define UC_TEXT(i) (uc_storage+32u+(uint32_t)(i)*192u)
extern void sub_00356870(void),sub_000696F0(void),sub_00069720(void);
extern void sub_001CD120(void),sub_001CD040(void),sub_001922D0(void);
static uint32_t uc_storage,uc_parent,uc_previous,uc_profile,uc_identity;
static uint32_t uc_rows[PC_UNLOCK_COUNT];
static const char *const uc_names[PC_UNLOCK_COUNT]={
    "Story levels","Characters","Arcade maps","Arcade modes","Weapons",
    "Arcade League","Challenges","Cheats","Movies"
};
static uint32_t uc_active_profile(void)
{
    uint32_t index=guest_call(sub_00356870,1,0u);
    if(index>=16u)return 0;
    uint32_t profile=guest_call(sub_000696F0,1,index);
    return guest_writable(profile,PC_UNLOCK_PROFILE_SIZE)?profile:0;
}
static uint32_t uc_flags(void)
{
    uint32_t flags=0,profile=uc_active_profile();
    if(profile)pc_unlock_decode((void *)((uintptr_t)profile+g_xbox_mem_offset),&flags);
    return flags;
}
void pc_unlock_requirement(void (*original)(void))
{
    uint32_t caller=WORD_AT(g_esp),descriptor=WORD_AT(g_esp+4);
    int category=pc_unlock_requirement_category(caller,descriptor);
    if(category>=0 && guest_writable(descriptor,20) &&
       WORD_AT(descriptor)>0 && WORD_AT(descriptor)<=8) {
        GuestRegisters r;save_registers(&r);
        uint32_t flags=uc_flags();restore_registers(&r);
        if(flags&(1u<<category)){g_eax=1;g_esp+=4;return;}
    }
    original();
}
void pc_unlock_event(void (*original)(void))
{
    uint32_t event=WORD_AT(g_esp+4),group=WORD_AT(g_esp+8);
    /* These are the 48 real catalog entries, three per group. Their mission
     * requirements are initialized at runtime (zero in the XBE), so grant
     * access at this bounded catalog predicate, not at a zero descriptor. */
    if(event<48u && group==event/3u) {
        GuestRegisters r;save_registers(&r);
        uint32_t flags=uc_flags();restore_registers(&r);
        unsigned category=event<21u?PC_UNLOCK_CHALLENGES:PC_UNLOCK_LEAGUE;
        if(flags&(1u<<category)){g_eax=1;g_esp+=4;return;}
    }
    original();
}
void pc_unlock_movie(void (*original)(void))
{
    uint32_t caller=WORD_AT(g_esp),movie=WORD_AT(g_esp+4);
    /* This getter also checks pickups during play: only Movies' two loops
     * may see the override. Collectibles remain collectable. */
    if((caller==0x2cc17cu || caller==0x2cc1e9u) && movie<20u) {
        GuestRegisters r;save_registers(&r);
        uint32_t flags=uc_flags();restore_registers(&r);
        if(flags&(1u<<PC_UNLOCK_MOVIES)){g_eax=1;g_esp+=4;return;}
    }
    original();
}
void pc_unlock_profile_init(void (*original)(void))
{
    original();
    uint32_t profile=g_eax,identity;
    /* 69D90 resets controls/progress for a new (-1 device) profile. Clear our
     * extension at that same boundary so reused guest slots cannot inherit it.
     * Loading/copying an existing profile retains its signed extension. */
    if(guest_writable(profile,PC_UNLOCK_PROFILE_SIZE) &&
       guest_writable(identity=WORD_AT(profile),0x40) && WORD_AT(identity+0x24)==UINT32_MAX)
        memset((void *)((uintptr_t)(profile+PC_UNLOCK_OFFSET)+g_xbox_mem_offset),0,16);
}
static uint32_t uc_text(unsigned slot,const char *text)
{
    snprintf((char *)((uintptr_t)UC_TEXT(slot)+g_xbox_mem_offset),192,"%s",text);
    return UC_TEXT(slot);
}
static int uc_prepare(void)
{
    if(!uc_storage) {
        uc_storage=xbox_HeapAlloc(32+16*192,4);
        if(!uc_storage)return 0;
        memset((void *)((uintptr_t)uc_storage+g_xbox_mem_offset),0,32+16*192);
        WORD_AT(uc_storage)=UC_BUILD;WORD_AT(uc_storage+4)=UC_TICK;
        uc_text(0,"Unlock Content");uc_text(12,"Off");uc_text(13,"On");
    }
    return 1;
}
static void uc_extras_build(void)
{
    extern void sub_0003D7B0(void),sub_00075E70(void);
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4);save_registers(&r);
    guest_call(sub_00075B70,5,0x800a000u,guest_call(sub_0003D7B0,1,0x40au),bits(45.f),0u,0u);
    WORD_AT(menu+0x218)=9;
    guest_call(sub_00071B10,4,1u,bits(2.f),1u,1u);
    guest_call(sub_00071C50,1,0x32200u);
    guest_call(sub_00079D60_gen,6,guest_call(sub_0003D7B0,1,0x40fu),UINT32_MAX,0u,0u,0x2d08a0u,0u);
    guest_call(sub_00079D60_gen,6,guest_call(sub_0003D7B0,1,0x410u),UINT32_MAX,0u,0u,0x2d0880u,0u);
    if(uc_prepare())guest_call(sub_00079D60_gen,6,UC_TEXT(0),UINT32_MAX,uc_active_profile()?0u:1u,0u,UC_OPEN,0u);
    if(guest_call(sub_001CD120,0))
        guest_call(sub_00079D60_gen,6,guest_call(sub_0003D7B0,1,0x1a0fu),UINT32_MAX,0u,0u,0x2cbe30u,0u);
    guest_call(sub_000771D0,1,menu);guest_call(sub_00075E70,0);
    restore_registers(&r);g_esp+=4;
}
static int uc_current(void)
{
    return uc_profile && uc_active_profile()==uc_profile && WORD_AT(uc_profile)==uc_identity;
}
static void uc_read_choices(void)
{
    uint32_t flags;
    if(!uc_current() || !pc_unlock_decode((void *)((uintptr_t)uc_profile+g_xbox_mem_offset),&flags))return;
    for(unsigned i=0;i<PC_UNLOCK_COUNT;i++) {
        if(!guest_writable(uc_rows[i],0xcc))continue;
        unsigned choice=WORD_AT(uc_rows[i]+0xc8);
        if(choice>1u || choice==((flags>>i)&1u))continue;
        if(pc_unlock_set((void *)((uintptr_t)uc_profile+g_xbox_mem_offset),i,(int)choice)) {
            WORD_AT(0x78ad24u)|=0x10000u;
            if(i==PC_UNLOCK_CHEATS && !choice)
                WORD_AT(0x790954u)&=guest_call(sub_001CD040,0);
        }
    }
}
static void uc_build(void)
{
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4),flags=0;save_registers(&r);
    int valid=uc_current() && pc_unlock_decode((void *)((uintptr_t)uc_profile+g_xbox_mem_offset),&flags);
    /* Reserve distinct label and selector columns at the narrow 4:3 layout. */
    guest_call(sub_00075B70,5,0xa000u,UC_TEXT(0),bits(65.f),0u,UC_BACK);
    guest_call(sub_00071B10,4,1u,bits(1.1f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    for(unsigned i=0;i<PC_UNLOCK_COUNT;i++) {
        uc_rows[i]=guest_call(sub_00079D60_gen,6,uc_text(i+1,uc_names[i]),i,valid?0u:1u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        guest_call(sub_000717D0,1,UC_TEXT(12));guest_call(sub_000717D0,1,UC_TEXT(13));
        guest_call(sub_00075430,2,uc_rows[i],(flags>>i)&1u);
    }
    guest_call(sub_000771D0,1,menu);
    restore_registers(&r);g_esp+=4;
}
static void uc_tick(void)
{
    GuestRegisters r;save_registers(&r);uc_read_choices();restore_registers(&r);g_esp+=4;
}
static void uc_back(void)
{
    GuestRegisters r;save_registers(&r);uc_read_choices();
    /* Existing signed-save/error UI owns persistence and retry. Preserve all
     * other pending profile commits because the game's dirty bit is global. */
    guest_call(sub_001922D0,3,uc_parent,uc_previous,UINT32_MAX);
    memset(uc_rows,0,sizeof uc_rows);restore_registers(&r);g_esp+=4;
}
static void uc_open(void)
{
    GuestRegisters r;uint32_t source=WORD_AT(g_esp+4);save_registers(&r);
    uc_profile=uc_active_profile();
    if(uc_profile && guest_writable(source,0x30) && uc_prepare()) {
        uc_identity=WORD_AT(uc_profile);
        uc_parent=WORD_AT(source+0x2c);uc_previous=WORD_AT(source+4);
        guest_call(sub_00079B70,2,uc_parent,uc_storage);
    }
    restore_registers(&r);g_esp+=4;
}
