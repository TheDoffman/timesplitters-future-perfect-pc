/* Production native subpages with the original guest widget calls doubled.
 * Verifies layout population/navigation/persistence, not actual rendering. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define PC_AUDIO_SETTINGS_IMPLEMENTATION
#include "interface/pc_audio_settings.h"
#undef PC_AUDIO_SETTINGS_IMPLEMENTATION
#define PC_AUDIO_MIX_SETTINGS_IMPLEMENTATION
#include "interface/pc_audio_mix_settings.h"
#undef PC_AUDIO_MIX_SETTINGS_IMPLEMENTATION
static uint8_t guest[131072];static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_esp,g_eax,heap=0x4000,next_row=0x10000;
#define WORD_AT(a) (*(uint32_t *)(guest+(uint32_t)(a)))
typedef struct GuestRegisters {uint32_t sp,a;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;r->a=g_eax;}
static void restore_registers(GuestRegisters *r){g_esp=r->sp;g_eax=r->a;}
static uint32_t bits(float f){uint32_t n;memcpy(&n,&f,4);return n;}
static int guest_writable(uint32_t a,size_t n){return a && a<sizeof guest && n<=sizeof guest-a;}
static uint32_t xbox_HeapAlloc(uint32_t n,uint32_t alignment)
{uint32_t result=heap;heap=(heap+n+3)&~3u;assert(heap<0x10000 && alignment==4);return result;}
void sub_00071CF0(void){} void sub_00079D60_gen(void){} void sub_0007D480(void){}
void sub_000717D0(void){} void sub_00075430(void){} void sub_00075B70(void){}
void sub_00071B10(void){} void sub_00071C50(void){} void sub_000771D0(void){}
void sub_00079B70(void){} void sub_00078980(void){} void sub_000770E0(void){}
static unsigned rows,choices,rebuilds,layout_calls,selection;
static uint32_t navigation_parent,navigation_descriptor,last_callback;
static uint32_t guest_call(void (*fn)(void),unsigned n,...)
{
    uint32_t a[6]={0};assert(n<=6);va_list ap;va_start(ap,n);
    for(unsigned i=0;i<n;i++)a[i]=va_arg(ap,uint32_t);va_end(ap);
    if(fn==sub_00079D60_gen) {
        assert(n==6 && a[3]==bits(32.f));
        uint32_t row=next_row;next_row+=0x110;assert(guest_writable(row,0x110));
        WORD_AT(row+0x14)=a[1];WORD_AT(row+0x28)=a[2];last_callback=a[4];rows++;return row;
    }
    if(fn==sub_000717D0){assert(guest_writable(a[0],1));choices++;}
    if(fn==sub_00075430){assert(guest_writable(a[0],0xcc));WORD_AT(a[0]+0xc8)=a[1];}
    if(fn==sub_00079B70){navigation_parent=a[0];navigation_descriptor=a[1];}
    if(fn==sub_00078980)rebuilds++;
    if(fn==sub_000770E0)selection=a[1];
    if(fn==sub_000771D0)layout_calls++;
    return 0;
}
#include "interface/pc_audio_menu.inc"
static void build(uint32_t menu)
{g_esp=0x200;WORD_AT(g_esp+4)=menu;rows=choices=0;audio_mix_build();assert(g_esp==0x204);}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)guest;g_esp=0x200;WORD_AT(g_esp+12)=0x21;
    audio_beta_add(sub_00079D60_gen);
    assert(rows==1 && last_callback==AUDIO_MIX_OPEN && WORD_AT(0x10000+0x28)==0 && g_esp==0x200);
    assert(WORD_AT(audio_menu_storage)==AUDIO_MIX_BUILD && WORD_AT(audio_menu_storage+4)==AUDIO_MIX_TICK);
    uint32_t source=0x1000;WORD_AT(source+0x2c)=0x222;WORD_AT(source+4)=0x333;
    WORD_AT(g_esp+4)=source;audio_mix_open();
    assert(g_esp==0x204 && navigation_parent==0x222 && navigation_descriptor==audio_menu_storage);
    build(0x2000);assert(rows==8 && choices==606 && layout_calls==1);
    assert(WORD_AT(audio_mix_rows[0]+0xc8)==100 && WORD_AT(audio_mix_rows[5]+0xc8)==100);
    for(unsigned i=0;i<6;i++)WORD_AT(audio_mix_rows[i]+0xc8)=i*17;
    g_esp=0x200;audio_mix_change();assert(g_esp==0x204);
    PcAudioMixOptions mix=pc_audio_mix_get();assert(mix.master==0);
    for(unsigned i=0;i<5;i++)assert(mix.volume[i]==(i+1)*17);
    assert(!pc_audio_enhanced_enabled());
    char path[32768];assert(pc_audio_config_path(path,sizeof path,"pc-audio-mix.ini"));
    FILE *file=fopen(path,"wb");assert(file);fputs("TSFP_AUDIO_MIX_V99 future",file);fclose(file);
    WORD_AT(audio_mix_rows[3]+0xc8)=1;g_esp=0x200;audio_mix_change();
    assert(WORD_AT(audio_mix_rows[3]+0xc8)==51 && pc_audio_mix_get().volume[2]==51);
    file=fopen(path,"rb");char text[80]={0};assert(file);fread(text,1,79,file);fclose(file);
    assert(!strcmp(text,"TSFP_AUDIO_MIX_V99 future"));remove(path);
    g_esp=0x200;audio_features_open();assert(audio_features_page && rebuilds==1 && selection==0);
    build(0x2000);assert(rows==6 && choices==10 && layout_calls==2 && !audio_mix_rows[0]);
    WORD_AT(audio_feature_rows[0]+0xc8)=1;WORD_AT(audio_feature_rows[2]+0xc8)=0;
    g_esp=0x200;audio_beta_change();assert(pc_audio_enhanced_enabled() && !pc_audio_get_options().weapon_range);
    g_esp=0x200;audio_mix_back();assert(!audio_features_page && rebuilds==2 && selection==6);
    build(0x2000);assert(!audio_feature_rows[0] && WORD_AT(audio_mix_rows[3]+0xc8)==51);
    g_esp=0x200;audio_mix_back();assert(navigation_parent==0x222 && navigation_descriptor==0x333 && !audio_mix_rows[0]);
    g_esp=0x200;audio_mix_tick();assert(g_esp==0x204);
    puts("PASS: native audio subpages, exact percent choices, saved mix, failure rollback and original-menu return");
}
