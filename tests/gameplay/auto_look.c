#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the production guest adapter independently of the OS input pump. */
#define TSFP_PC_CONTROLS_H
#define TSFP_PC_PROMPTS_H
static int mouse_enabled=1,keyboard=1;
static void pc_controls_get_mouse(double *s,int *on,int *invert)
{ *s=.1;*on=mouse_enabled;*invert=0; }
static int pc_prompts_keyboard_active(void) { return keyboard; }
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_eax,g_esi,g_esp;
#include "pc_auto_look_guest.inc"
#define CHECK(x) do { if(!(x)) {fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static uint32_t memory[0x800000/4];
#define WORD(a) memory[(a)/4]
#define BYTE(a) ((uint8_t *)memory)[a]
enum { PLAYER=0x1000, SETTINGS=0x3000, MENU=0x4000, ITEM=0x5000 };
static unsigned calls,expected_flag,expected_value,expected_style;
static void original_camera(void)
{
    CHECK((BYTE(SETTINGS+2)&8u)==expected_flag);
    BYTE(SETTINGS+2)^=0x80; /* Other original writes must survive. */
    ++calls;g_eax=0xCAFE;g_esp+=4;
}
static void original_choice(void)
{
    CHECK(WORD(ITEM+0xc8)==expected_value);
    CHECK(WORD(ITEM+0xb8)==expected_style);
    ++calls;g_eax=0xDEAD;g_esp+=4;
}
static void original_settings(void)
{
    CHECK(WORD(ITEM+0xc8)==expected_value);
    BYTE(0x7BA366)=(uint8_t)((BYTE(0x7BA366)&~8u)|(WORD(ITEM+0xc8)?8u:0u));
    BYTE(0x7BA366)^=0x80; /* A different option changed in this menu tick. */
    ++calls;g_eax=0xBEEF;g_esp+=4;
}
static void setup(void)
{
    memset(memory,0,sizeof memory);calls=0;keyboard=mouse_enabled=1;g_esp=0x9000;
    WORD(0x7B0C7C)=PLAYER;WORD(PLAYER)=7;WORD(PLAYER+4)=0;WORD(PLAYER+0x1500)=SETTINGS;
    BYTE(SETTINGS+2)=0x1f;
    WORD(MENU+4)=0x524A40;WORD(MENU+0x38)=0;WORD(MENU+0x178)=ITEM;
    WORD(ITEM+0x14)=4;WORD(ITEM+0x88)=MENU;WORD(ITEM+0xb4)=1;
    WORD(ITEM+0xb8)=0x20000;WORD(ITEM+0xc8)=1;BYTE(0x7BA366)=0x1f;
    g_eax=ITEM;g_esi=MENU;
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    setup();expected_flag=0;pc_auto_look_camera(original_camera);
    CHECK(BYTE(SETTINGS+2)==0x9f && g_eax==0xCAFE && g_esp==0x9004 && calls==1);
    /* No per-frame expiry: holding W after mouse movement stays mouse input. */
    for(unsigned i=0;i<120;i++) pc_auto_look_camera(original_camera);
    CHECK(BYTE(SETTINGS+2)==0x9f);
    keyboard=0;expected_flag=8;pc_auto_look_camera(original_camera);
    CHECK(BYTE(SETTINGS+2)==0x1f);
    keyboard=1;expected_flag=0;pc_auto_look_camera(original_camera);
    setup();BYTE(SETTINGS+2)=0x17;expected_flag=0;keyboard=0;
    pc_auto_look_camera(original_camera);CHECK(BYTE(SETTINGS+2)==0x97);
    /* Disabled mouse, other local slots and absent player/settings pass through. */
    for(unsigned mode=0;mode<5;mode++) {
        setup();expected_flag=8;
        if(mode==0)mouse_enabled=0;
        if(mode==1)WORD(PLAYER+4)=1;
        if(mode==2)WORD(PLAYER+4)=0xffffffffu;
        if(mode==3)WORD(0x7B0C7C)=0;
        if(mode==4)WORD(PLAYER+0x1500)=0;
        pc_auto_look_camera(original_camera);CHECK(BYTE(SETTINGS+2)==0x9f);
    }
    setup();expected_value=0;expected_style=0x420000;
    pc_auto_look_choice(original_choice);
    CHECK(WORD(ITEM+0xc8)==1 && WORD(ITEM+0xb8)==0x20000);
    CHECK(g_eax==0xDEAD && g_esp==0x9004 && calls==1);
    keyboard=0;g_eax=ITEM;expected_value=1;expected_style=0x20000;
    pc_auto_look_choice(original_choice);
    for(unsigned mode=0;mode<5;mode++) {
        setup();expected_value=1;expected_style=0x20000;
        if(mode==0)mouse_enabled=0;
        if(mode==1)WORD(MENU+0x38)=1;
        if(mode==2)WORD(MENU+4)=0x524A30;
        if(mode==3)WORD(ITEM+0x14)=3;
        if(mode==4)WORD(ITEM+0xb4)=2;
        pc_auto_look_choice(original_choice);
    }
    /* Queued arrow/keyboard input cannot overwrite a saved controller ON. */
    setup();WORD(ITEM+0xc8)=0;expected_value=1;
    pc_auto_look_settings(original_settings);
    CHECK(BYTE(0x7BA366)==0x9f && g_eax==0xBEEF && g_esp==0x9004 && calls==1);
    setup();BYTE(0x7BA366)=0x17;expected_value=0;
    pc_auto_look_settings(original_settings);CHECK(BYTE(0x7BA366)==0x97);
    /* Controller edits remain real, including OFF; no stale cached preference. */
    setup();keyboard=0;WORD(ITEM+0xc8)=0;expected_value=0;
    pc_auto_look_settings(original_settings);CHECK(BYTE(0x7BA366)==0x97);
    keyboard=1;WORD(ITEM+0xc8)=1;pc_auto_look_settings(original_settings);
    CHECK(BYTE(0x7BA366)==0x17 && WORD(ITEM+0xc8)==0);
    setup();WORD(0x7BA9E0)=1;WORD(ITEM+0xc8)=0;expected_value=0;
    pc_auto_look_settings(original_settings);CHECK(BYTE(0x7BA366)==0x97);
    puts("PASS: automatic lookahead gameplay, menu display, preferences and device switching");
    return 0;
}
