/* US 45410066 text-entry dialog. Invoke the original on-screen-key callbacks:
 * they own selection replacement, byte/visible-width limits, sounds, callbacks
 * and saving. No host thread writes guest text or profile memory. */
#include "pc_text_input.h"
#include "pc_guest_memory.h"
extern void sub_0007DB20(void),sub_0007C410(void),sub_0007C300(void),sub_0007C390(void);
extern void sub_0007C4F0(void),sub_0007CDE0(void),sub_000746D0(void);
extern void sub_0007CCA0(void),sub_00071940(void);

/* Profile creation and multiplayer names embed the same key callbacks in
 * their own descriptors. Identify the live keyboard rows, not just the
 * standalone dialog descriptor; stale edit-buffer fields survive page changes. */
static uint32_t menu_text_row(uint32_t menu,uint32_t callback,uint32_t id) {
    /* This scan calls no guest routines or event handlers. Reuse one live VM
     * region only until the scan returns; menu ticks can replace these rows. */
    PcGuestRegion region={0};
    uint32_t item=WORD_AT(menu+0x178);
    for(unsigned n=0;item && n<256;n++,item=WORD_AT(item+4)) {
        if(0x10cu>UINT32_MAX-item)break;
        uintptr_t host=(uintptr_t)item+g_xbox_mem_offset;
        if(pc_guest_region_remaining(&region,host,1)<0x10c) {
            region=pc_guest_query_region(host);
            if(pc_guest_region_remaining(&region,host,1)<0x10c)break;
        }
        if(WORD_AT(item+0x38)==callback &&
           (id==UINT32_MAX || WORD_AT(item+0x14)==id))return item;
    }
    return 0;
}
static int menu_text_keyboard(uint32_t menu) {
    return guest_writable(menu,0x23c) && WORD_AT(menu+4)!=0x004D94D4u &&
        menu_text_row(menu,0x0007DB20u,UINT32_MAX)!=0;
}
static int menu_text_ready(uint32_t menu) {
    return menu_text_keyboard(menu) && WORD_AT(menu+0x1b0)==3 &&
        guest_call(sub_00074760,1,menu);
}
static uint32_t menu_text_back;
static void menu_text_focus(uint32_t menu) {
    GuestRegisters r;save_registers(&r);
    if(guest_writable(menu,0x23c) && WORD_AT(menu+0x1b0)==3 &&
       guest_call(sub_00074760,1,menu)) {
        if(menu_text_back!=menu)menu_text_back=0;
        pc_controls_text_mode(menu_text_keyboard(menu)?menu:0);
    }
    restore_registers(&r);
}
/* Feed Escape through the owning menu's Back action. Embedded keyboards
 * have a different Back callback from the standalone dialog. */
static void menu_text_input(uint32_t menu) {
    if(menu_text_back!=menu)return;
    GuestRegisters r;save_registers(&r);
    menu_text_back=0;
    if(menu_text_ready(menu))WORD_AT(menu+0xc4)|=0x20u;
    restore_registers(&r);
}
static void menu_text_insert(uint32_t menu,unsigned character) {
    static const uint32_t pages[2][5]={
        {0x4D8E74,0x4D8F44,0x4D9014,0x4D90E4,0x4D91B4},
        {0x4D9254,0x4D92EC,0x4D9384,0x4D941C,0x4D94B4}};
    static const unsigned modes[]={0,4,8,0x1000,0x1004};
    uint32_t flags=WORD_AT(menu+0x228),selected=WORD_AT(menu+0x18c);
    unsigned compact=!!(flags&0x80),count=compact?29:41;
    for(unsigned page=0;page<5;page++) {
        if(page>=3 && (flags&0x4000))continue;
        const unsigned char *letters=(const unsigned char *)(g_xbox_mem_offset+pages[compact][page]);
        for(unsigned i=0;i<count;i++)if(letters[i]==character) {
            WORD_AT(menu+0x228)=(flags&~0x100eu)|modes[page];
            WORD_AT(menu+0x18c)=i;
            guest_call(sub_0007DB20,1,menu);
            WORD_AT(menu+0x228)=(WORD_AT(menu+0x228)&~0x100eu)|(flags&0x100eu);
            WORD_AT(menu+0x18c)=selected;
            guest_call(sub_000746D0,1,menu);
            return;
        }
    }
}
static void menu_text_events(uint32_t menu) {
    GuestRegisters r;save_registers(&r);
    if(!menu_text_ready(menu)) {restore_registers(&r);return;}
    /* Validate the original temporary edit buffer before invoking its routines. */
    uint32_t buffer=WORD_AT(menu+0x220),limit=WORD_AT(menu+0x230);
    if(!limit || limit>4096 || !guest_writable(buffer,limit+1)) {
        pc_controls_text_mode(0);restore_registers(&r);return;
    }
    for(unsigned n=0;n<64;n++) {
        unsigned event=pc_controls_text_event(menu),length=0;
        if(!event)break;
        const char *text=(const char *)(g_xbox_mem_offset+buffer);
        while(length<=limit && text[length])++length;
        if(length>limit || WORD_AT(menu+0x22c)>length)break;
        if(event<256) menu_text_insert(menu,event);
        else if(event==PC_TEXT_ACCEPT) {
            /* The tick updates Done's disabled state (blank/minimum length). */
            guest_call(sub_0007CCA0,1,menu);
            uint32_t done=menu_text_row(menu,0x0007DA20u,0u);
            if(guest_writable(done,0x2c) && !(WORD_AT(done+0x28)&1u)) {
                pc_controls_text_mode(0);guest_call(sub_0007C4F0,1,menu);break;
            }
        } else if(event==PC_TEXT_CANCEL) {
            pc_controls_text_mode(0);menu_text_back=menu;break;
        } else if(event==PC_TEXT_BACKSPACE || event==PC_TEXT_DELETE) {
            if(event==PC_TEXT_DELETE && !(WORD_AT(menu+0x228)&0x10)) {
                if(WORD_AT(menu+0x22c)==length)continue;
                g_esi=menu;guest_call(sub_0007C390,0);
            } else if(!(WORD_AT(menu+0x228)&0x10) && !WORD_AT(menu+0x22c))continue;
            g_ebx=menu;guest_call(sub_0007C410,0);
        } else if(event==PC_TEXT_LEFT || event==PC_TEXT_RIGHT) {
            g_esi=menu;guest_call(event==PC_TEXT_LEFT?sub_0007C300:sub_0007C390,0);
        } else {
            if(event==PC_TEXT_SELECT_ALL) WORD_AT(menu+0x228)|=0x10u;
            else {
                WORD_AT(menu+0x228)&=~0x10u;
                WORD_AT(menu+0x22c)=event==PC_TEXT_HOME?0:length;
            }
            guest_call(sub_000746D0,1,menu);
        }
    }
    restore_registers(&r);
}
