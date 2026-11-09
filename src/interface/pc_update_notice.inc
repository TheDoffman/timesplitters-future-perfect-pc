/* Main-menu-only native-font notice, fed by the shared launcher release check. */
#include "pc_update_notice.h"
#include "../diagnostics/pc_build_version.h"
#include <time.h>
extern void sub_00041DC0(void),sub_0003FCF0(void),sub_0003F8D0(void);
static uint32_t update_notice_menu,update_notice_descriptor,update_notice_scratch;
static ULONGLONG update_notice_checked;
static int update_notice_polled,update_notice_available;
static void main_update_notice_open(uint32_t menu)
{
    update_notice_menu=menu;update_notice_descriptor=WORD_AT(menu+4);
    update_notice_polled=0;
}
static void main_update_notice_draw(uint32_t menu)
{
    if(menu!=update_notice_menu || !guest_writable(menu,0x1b4) ||
       WORD_AT(menu+4)!=update_notice_descriptor || WORD_AT(menu+0x38)!=0 ||
       WORD_AT(menu+0x170) || WORD_AT(menu+0x1b0)!=3 || settings_footer_menu!=menu)return;
    ULONGLONG notice_ticks=GetTickCount64();
    if(!update_notice_polled || notice_ticks-update_notice_checked>=2000) {
        update_notice_polled=1;update_notice_checked=notice_ticks;update_notice_available=0;
        const char *path=getenv("TSFP_GAME_UPDATE_NOTICE");
        const char *installed=getenv("TSFP_GAME_INSTALLED_VERSION");
        if(!installed || !*installed)installed=pc_build_version();
        if(path && *path) {
            FILE *file=fopen(path,"rb");
            if(file) {char text[512];size_t bytes=fread(text,1,sizeof text,file);fclose(file);
                update_notice_available=pc_update_notice_available(text,bytes,installed,(long long)time(NULL));}
        }
    }
    if(!update_notice_available)return;
    if(!update_notice_scratch)update_notice_scratch=xbox_HeapAlloc(256,4);
    if(!update_notice_scratch)return;
    const uint32_t scratch=update_notice_scratch;
    snprintf((char *)(g_xbox_mem_offset+scratch+8),248,"UPDATE AVAILABLE - OPEN THE LAUNCHER");
    GuestRegisters saved;save_registers(&saved);
    uint32_t sx=WORD_AT(0x7e7270),sy=WORD_AT(0x7e7280),align=WORD_AT(0x7e7260);
    int placement=ui_placement;uint32_t tag=ui_tag;ui_place(2,0x12060u);WORD_AT(0x7e7260)=0;
    float footer_x,footer_y;memcpy(&footer_x,&settings_footer_x,4);memcpy(&footer_y,&settings_footer_y,4);
    WORD_AT(0x7e7270)=bits(footer_x*.8f);WORD_AT(0x7e7280)=bits(footer_y*.8f);
    guest_call(sub_0003F8D0,0);
    guest_call(sub_0003FCF0,4,scratch,scratch+4,scratch+8,0u);
    float width=(float)WORD_AT(0x4e8cf0),height=(float)WORD_AT(0x4e8cf4);
    float fit=fminf(1.f,(width-32.f)/fmaxf(1.f,FLOAT_AT(scratch)));
    WORD_AT(0x7e7270)=bits(FLOAT_AT(0x7e7270)*fit);WORD_AT(0x7e7280)=bits(FLOAT_AT(0x7e7280)*fit);
    guest_call(sub_0003F8D0,0);guest_call(sub_0003FCF0,4,scratch,scratch+4,scratch+8,0u);
    guest_call(sub_00041DC0,4,bits((width-FLOAT_AT(scratch))*.5f),bits(height-82.f),scratch+8,settings_footer_colour);
    guest_call(sub_00022FF0,0);WORD_AT(0x7e7270)=sx;WORD_AT(0x7e7280)=sy;WORD_AT(0x7e7260)=align;
    guest_call(sub_0003F8D0,0);ui_place(placement,tag);restore_registers(&saved);
}
