#ifndef TSFP_PC_WEAPON_SLOTS_H
#define TSFP_PC_WEAPON_SLOTS_H
#include <stdint.h>
#include <string.h>
/* The supported title has 70 ordered weapon entries, including variants.
 * Inventory slots collapse variants using the game's own selection filters. */
#define PC_WEAPON_SLOT_COUNT 10
#define PC_WEAPON_TABLE_COUNT 70
typedef struct PcWeaponKeys {
    unsigned char held[PC_WEAPON_SLOT_COUNT];
    int armed, pending;
    uint64_t at;
} PcWeaponKeys;
static inline void pc_weapon_keys_sample(PcWeaponKeys *s,
    const unsigned char down[PC_WEAPON_SLOT_COUNT], int active, uint64_t now)
{
    int any=0, pressed=0;
    for(int i=0;i<PC_WEAPON_SLOT_COUNT;i++) {
        any|=down[i];
        if(down[i] && !s->held[i] && !pressed) pressed=i+1;
        s->held[i]=down[i];
    }
    if(!active) { s->armed=0;s->pending=0;return; }
    if(!s->armed) { if(!any)s->armed=1;return; }
    if(pressed) { s->pending=pressed;s->at=now; }
}
static inline int pc_weapon_keys_take(PcWeaponKeys *s,int active,uint64_t now)
{
    int result=active && now-s->at<=150?s->pending:0;
    s->pending=0;
    return result;
}
int pc_weapon_slot_take(void);
int pc_controls_weapon_slots_active(void);
void pc_display_weapon_slots(void);
#endif
