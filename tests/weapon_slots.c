#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pc_weapon_slots.h"
static unsigned char memory[0x800000];
#define WORD_AT(a) (*(uint32_t *)(memory+(a)))
static uint32_t g_esp=0x700000, selected, hud;
static int request;
static unsigned char owned[70],usable[70],preferred[70];
static int guest_writable(uint32_t address,size_t bytes)
{return address && address<sizeof memory && bytes<=sizeof memory-address;}
int pc_weapon_slot_take(void) {int r=request;request=0;return r;}
void sub_001BAA70(void) {} void sub_001BA410(void) {}
void sub_001BA130(void) {} void sub_00142820(void) {}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    va_list ap;uint32_t entity=0,weapon=0;
    va_start(ap,count);
    if(count)entity=va_arg(ap,uint32_t);
    if(count>1)weapon=va_arg(ap,uint32_t);
    if(count==3)assert(va_arg(ap,uint32_t)==1);
    va_end(ap);
    if(count)assert(entity==0x3000);
    assert(weapon<70);
    if(fn==sub_001BAA70)return preferred[weapon];
    if(fn==sub_001BA410)return owned[weapon] && usable[weapon];
    if(fn==sub_001BA130) {selected=weapon;WORD_AT(entity+0x94)=weapon;return 0;}
    assert(fn==sub_00142820);hud++;return 0;
}
#include "pc_weapon_slots_guest.inc"
static void press(int slot) {selected=0;request=slot;pc_display_weapon_slots();}
int main(void)
{
    PcWeaponKeys state={0};unsigned char down[PC_WEAPON_SLOT_COUNT]={0};
    pc_weapon_keys_sample(&state,down,1,0);
    down[5]=1;pc_weapon_keys_sample(&state,down,1,1);
    assert(pc_weapon_keys_take(&state,1,2)==6);
    pc_weapon_keys_sample(&state,down,1,3);assert(!pc_weapon_keys_take(&state,1,4));
    pc_weapon_keys_sample(&state,down,0,5);pc_weapon_keys_sample(&state,down,1,6);
    assert(!pc_weapon_keys_take(&state,1,7)); /* held through focus/menu */
    down[5]=0;pc_weapon_keys_sample(&state,down,1,8);
    down[5]=1;pc_weapon_keys_sample(&state,down,1,9);
    assert(!pc_weapon_keys_take(&state,1,200)); /* stale/cutscene request */
    down[5]=0;pc_weapon_keys_sample(&state,down,1,201);
    down[9]=1;pc_weapon_keys_sample(&state,down,1,202);
    assert(pc_weapon_keys_take(&state,1,203)==10);
    WORD_AT(0x7b0c7c)=0x1000;WORD_AT(0x1014)=0x2000;WORD_AT(0x207c)=0x3000;
    WORD_AT(0x3000)=0x2000;WORD_AT(0x1114)=2;WORD_AT(0x790950)=1;
    WORD_AT(0x4e7944)=1;
    for(unsigned i=0;i<70;i++) {
        WORD_AT(0x4fc888+4*i)=i;WORD_AT(0x4fa160+60*i)=i;
        preferred[i]=usable[i]=1;
    }
    owned[2]=owned[4]=owned[5]=owned[11]=1;
    WORD_AT(0x4fa160+60*5)=4;preferred[4]=0; /* owned dual variant */
    press(1);assert(selected==2 && hud==1);
    press(2);assert(selected==5 && hud==2);
    press(3);assert(selected==11 && hud==3);
    press(3);assert(!selected && hud==3); /* already equipped */
    press(4);assert(!selected);
    press(11);assert(!selected); /* beyond the ten public slots */ /* no wrap or unowned grant */
    usable[5]=0;press(2);assert(!selected); /* 11 now slot 2, already equipped */
    usable[5]=1;
    WORD_AT(0x1004)=1;press(1);assert(!selected && request==1); /* other local player */
    WORD_AT(0x1004)=0;
    WORD_AT(0x761c48)=0x5000;WORD_AT(0x5000)=1;press(1);assert(!selected);
    WORD_AT(0x5000)=0;WORD_AT(0x1114)=0x80;press(1);assert(!selected); /* mounted */
    WORD_AT(0x1114)=2;WORD_AT(0x100c)=0x4000;press(1);assert(!selected);
    WORD_AT(0x100c)=0;WORD_AT(0x4e7944)=0;press(1);assert(!selected && request==1);
    WORD_AT(0x4e7944)=1;pc_display_weapon_slots();assert(selected==2 && !request);
    WORD_AT(0x3000)=0;press(1);assert(!selected);
    WORD_AT(0x3000)=0x2000;WORD_AT(0x4fc888+4)=70;press(1);assert(!selected);
    puts("PASS: weapon slots, variants, empty inventory, held keys and gameplay guards");
    return 0;
}
