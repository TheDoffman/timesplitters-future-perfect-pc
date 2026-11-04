#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#ifndef _WIN32
#include <strings.h>
#define _strnicmp strncasecmp
#endif
#define RECOMP_TLS _Thread_local
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static unsigned char memory[16384];
static uintptr_t g_xbox_mem_offset;
static uint32_t g_esp;
#define WORD_AT(a) (*(uint32_t *)(memory+(a)))
#define FLOAT_AT(a) (*(float *)(memory+(a)))
typedef struct GuestRegisters {uint32_t sp;} GuestRegisters;
static void save_registers(GuestRegisters *r){r->sp=g_esp;}
static void restore_registers(GuestRegisters *r){g_esp=r->sp;}
static int keyboard=1;
static int pc_prompts_keyboard_active(void){return keyboard;}
static size_t pc_prompts_read_region(uintptr_t a){return a>=(uintptr_t)memory && a-(uintptr_t)memory<sizeof memory?sizeof memory-(a-(uintptr_t)memory):0;}
static int guest_writable(uint32_t a,size_t n){return a<sizeof memory && n<=sizeof memory-a;}
static int pc_prompts_keycap_label(const char *s,size_t n){return (n==1 && (*s=='E' || *s=='<' || *s=='>')) || (n==7 && !memcmp(s,"Mouse 1",7));}
static unsigned glyphs,lines;static int min_x,max_x;static uint32_t seen_colour;
static void sub_0001EFB0(void){}
static void sub_00022FF0(void){}
void sub_000402D0(void){g_esp+=4;}
static uint32_t guest_call(void (*f)(void),unsigned n,...)
{
    if(f==sub_0001EFB0) {
        CHECK(n==5);va_list ap;va_start(ap,n);
        int x=(int)va_arg(ap,uint32_t);(void)va_arg(ap,uint32_t);
        int r=(int)va_arg(ap,uint32_t);(void)va_arg(ap,uint32_t);
        seen_colour=va_arg(ap,uint32_t);va_end(ap);
        if(!lines || x<min_x)min_x=x;
        if(!lines || r>max_x)max_x=r;
        lines++;
    }
    return 0;
}
#include "../src/interface/pc_keycaps.inc"
static void glyph(void){glyphs++;g_esp+=4;}
static int nested;
static void draw(void)
{
    uint32_t frame=g_esp,text=WORD_AT(frame+16);
    for(unsigned i=0;memory[text+i];i++) {
        if(memory[text+i]==' ')continue;
        uint32_t state=1024;WORD_AT(state)=0x60606055;
        FLOAT_AT(state+0x18)=20.f+i*8.f;FLOAT_AT(state+0x1c)=10.f;
        FLOAT_AT(state+0x20)=28.f+i*8.f;FLOAT_AT(state+0x24)=30.f;
        WORD_AT(state+0x40)=0;memory[state+0x38]=memory[text+i];
        g_esp=512;WORD_AT(g_esp+4)=state;pc_display_keycap_glyph(glyph);CHECK(g_esp==516);
        if(nested && memory[text+i]=='E') {
            nested=0;g_esp=4096;WORD_AT(g_esp+16)=9000;
            strcpy((char *)memory+9000,"ordinary");pc_display_keycap_text(draw);
            CHECK(keycap_draw.open);
        }
    }
    g_esp=frame+4;
}
static void run(const char *text)
{g_esp=8192;WORD_AT(g_esp+16)=10000;strcpy((char *)memory+10000,text);glyphs=lines=0;pc_display_keycap_text(draw);CHECK(g_esp==8196 && !keycap_draw.enabled);}
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;
    run("[E]");CHECK(glyphs==1 && lines && min_x==20 && max_x==44 && seen_colour==0x60606055);
    keyboard=0;run("[E]");CHECK(glyphs==3 && !lines);
    keyboard=1;run("[ordinary]");CHECK(glyphs==10 && !lines);
    run("[Mouse 1]");CHECK(glyphs==6 && lines);
    nested=1;run("[E]");CHECK(glyphs==9 && lines);
    run("[E");CHECK(glyphs==2 && !lines);
    run("[<]");CHECK(!glyphs && lines>8);
    run("[>]");CHECK(!glyphs && lines>8);
    g_esp=512;WORD_AT(g_esp+4)=1024;keycap_alternate_glyph();CHECK(g_esp==516);
    puts("PASS: native glyph keycaps, exact colour/positions, mouse labels, controller fallback and nested text");
    return 0;
}
