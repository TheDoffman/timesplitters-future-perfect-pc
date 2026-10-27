/* Called only from the original player weapon-input routine (US 001BF140).
 * Its caller already excludes locked, cutscene and non-player input states.
 * Use the same group preference, usability and selection routines as cycling. */
#include "pc_weapon_slots.h"
extern void sub_001BAA70(void),sub_001BA410(void),sub_001BA130(void),sub_00142820(void);
void pc_display_weapon_slots(void)
{
    uint32_t player=WORD_AT(0x007B0C7Cu),actor,entity;
    unsigned char groups[PC_WEAPON_TABLE_COUNT]={0};
    int slot;
    /* Do not let another local player's update consume player one's press. */
    if(!guest_writable(player,0x1584) || WORD_AT(player+4)!=0)return;
    /* Native high-refresh frames may have no simulation tick yet. */
    if(!WORD_AT(0x004E7944u))return;
    slot=pc_weapon_slot_take();
    if(!slot || slot>PC_WEAPON_SLOT_COUNT ||
       !(WORD_AT(player+0x114)&0x480eu) || (WORD_AT(player+0xc)&0x4000u) ||
       (int32_t)WORD_AT(0x0079094Cu)>100)return;
    uint32_t pause=WORD_AT(0x00761C48u);
    if(guest_writable(pause,4) && (WORD_AT(pause)&1u))return;
    actor=WORD_AT(player+0x14);
    if(!guest_writable(actor,0x80))return;
    entity=WORD_AT(actor+0x7c);
    if(!guest_writable(entity,0xC18) || WORD_AT(entity)!=actor ||
       WORD_AT(entity+8)>=WORD_AT(0x00790950u) ||
       !guest_writable(g_esp-512,512))return;
    for(unsigned i=1;i<PC_WEAPON_TABLE_COUNT;i++) {
        uint32_t weapon=WORD_AT(0x004FC888u+4*i);
        if(weapon>=PC_WEAPON_TABLE_COUNT)return;
        uint32_t group=WORD_AT(0x004FA160u+60*weapon);
        if(group>=PC_WEAPON_TABLE_COUNT)return;
        if(groups[group] || !guest_call(sub_001BAA70,2,entity,weapon) ||
           !guest_call(sub_001BA410,3,entity,weapon,1u))continue;
        groups[group]=1;
        if(--slot)continue;
        if(weapon!=WORD_AT(entity+0x94)) {
            guest_call(sub_001BA130,2,entity,weapon);
            guest_call(sub_00142820,0);
        }
        return;
    }
    /* An empty slot is a no-op; never grant a weapon or wrap around. */
}
