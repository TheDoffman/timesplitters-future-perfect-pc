/* US 45410066: Auto Lookahead is bit 3 at player->controls + 2.
 * sub_0025D520 (0x25DB8F) uses it to recenter pitch while moving.
 * Override only that read, never the stored profile/controller preference. */
#include "pc_controls.h"
#include "pc_prompts.h"
#include "pc_guest_memory.h"

static uintptr_t pc_auto_look_memory(uint32_t address,size_t bytes,int writing)
{
    uintptr_t p=(uintptr_t)address+g_xbox_mem_offset;
    return address && pc_guest_memory(p,bytes,writing)?p:0;
}
static uintptr_t pc_auto_look_memory_in_regions(PcGuestRegion regions[2],
                                               uint32_t address,size_t bytes,int writing)
{
    uintptr_t p=(uintptr_t)address+g_xbox_mem_offset;
    if(!address || bytes>UINTPTR_MAX-p)return 0;
    for(unsigned i=0;i<2;i++)
        if(bytes<=pc_guest_region_remaining(&regions[i],p,writing))return p;
    /* Keep the current-player global's map: settings may return to that map
       after the player record in high guest RAM. The second slot is bounded. */
    PcGuestRegion *region=regions[0].size?&regions[1]:&regions[0];
    return pc_guest_memory_in_region(region,p,bytes,writing)?p:0;
}
static uint32_t pc_auto_look_word(uintptr_t address)
{
    uint32_t value;
    memcpy(&value,(const void *)address,sizeof value);
    return value;
}
static int pc_auto_look_mouse(void)
{
    double sensitivity;
    int enabled,inverted;
    pc_controls_get_mouse(&sensitivity,&enabled,&inverted);
    return enabled && pc_prompts_keyboard_active();
}
static void pc_auto_look_camera(void (*original)(void))
{
    /* Reuse permissions only for the consecutive pre-call pointer reads.
       This scope ends before original(); never retain it for another update. */
    PcGuestRegion regions[2]={{0}};
    uintptr_t current=pc_auto_look_memory_in_regions(regions,0x007B0C7Cu,4,0),p=0,settings=0;
    uint8_t saved=0;
    if(current && pc_auto_look_mouse()) {
        p=pc_auto_look_memory_in_regions(regions,pc_auto_look_word(current),0x1504,0);
        /* +4 is the local input slot. +0 is a network/player ID and may be
         * nonzero even for the local keyboard/mouse player. */
        if(p && pc_auto_look_word(p+4)==0)
            settings=pc_auto_look_memory_in_regions(regions,pc_auto_look_word(p+0x1500),3,1);
        if(settings) {
            saved=*(uint8_t *)(settings+2)&8u;
            *(uint8_t *)(settings+2)&=(uint8_t)~8u;
        }
    }
    original();
    if(settings) *(uint8_t *)(settings+2)=(uint8_t)((*(uint8_t *)(settings+2)&~8u)|saved);
}

/* Controls uses descriptor 0x524A40 in both frontend and pause menus.
 * Render OFF without changing the draft that Accept/Cancel owns. Hide the
 * choice arrows while the effective value is fixed by mouse input. */
static uintptr_t pc_auto_look_menu(uint32_t address)
{
    uintptr_t menu=pc_auto_look_memory(address,0x17c,0);
    return menu && pc_auto_look_word(menu+4)==0x00524A40u &&
        pc_auto_look_word(menu+0x38)==0?menu:0;
}
static void pc_auto_look_choice(void (*original)(void))
{
    uintptr_t item=0;
    uint32_t saved=0,style=0;
    if(pc_auto_look_mouse()) {
        item=pc_auto_look_memory(g_eax,0xcc,1);
        if(item && (pc_auto_look_word(item+0x14)!=4 ||
           pc_auto_look_word(item+0xb4)!=1 ||
           !pc_auto_look_menu(pc_auto_look_word(item+0x88)))) item=0;
        if(item) {
            saved=pc_auto_look_word(item+0xc8);style=pc_auto_look_word(item+0xb8);
            *(uint32_t *)(item+0xc8)=0;
            *(uint32_t *)(item+0xb8)=style|0x400000u;
        }
    }
    original();
    if(item) {
        *(uint32_t *)(item+0xc8)=saved;
        *(uint32_t *)(item+0xb8)=style;
    }
}

/* The native menu can still receive keyboard left/right or a queued mouse
 * click. Before it copies rows into the draft, restore this row's saved
 * preference; every other setting and the original Accept/Cancel stay intact. */
static void pc_auto_look_settings(void (*original)(void))
{
    uintptr_t menu=pc_auto_look_menu(g_esi);
    if(menu && pc_auto_look_mouse()) {
        uintptr_t slot=pc_auto_look_memory(0x007BA9E0u,4,0);
        uintptr_t draft=pc_auto_look_memory(0x007BA366u,1,0);
        if(slot && draft && pc_auto_look_word(slot)==0) {
            uint32_t address=pc_auto_look_word(menu+0x178);
            for(unsigned count=0;address && count<256;count++) {
                uintptr_t item=pc_auto_look_memory(address,0xcc,1);
                if(!item || pc_auto_look_word(item+0x88)!=g_esi) break;
                if(pc_auto_look_word(item+0x14)==4 && pc_auto_look_word(item+0xb4)==1) {
                    *(uint32_t *)(item+0xc8)=!!(*(const uint8_t *)draft&8u);
                    break;
                }
                address=pc_auto_look_word(item+4);
            }
        }
    }
    original();
}
