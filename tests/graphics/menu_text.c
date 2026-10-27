#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#include "pc_text_input.h"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL: line %d %s\n",__LINE__,#x);exit(1);} } while(0)
ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
#define WORD_AT(a) MEM32(a)
static uint32_t memory[0x800000/4];
enum { MENU=0x1000, BUFFER=0x2000, DONE=0x3000, LETTER=0x4000 };
static PcTextInput queue;
static int focused=1,accepted,cancelled,insertions,refreshes;
void pc_controls_text_mode(uint32_t owner){pc_text_session(&queue,owner);}
unsigned pc_controls_text_event(uint32_t owner){return owner==queue.owner?pc_text_get(&queue):0;}
static int guest_writable(uint32_t p,size_t n){return p>=0x1000 && p<sizeof memory && n<=sizeof memory-p;}
typedef struct GuestRegisters { uint32_t a,b,c,d,si,sp,di,bp; } GuestRegisters;
static void save_registers(GuestRegisters *r){*r=(GuestRegisters){g_eax,g_ebx,g_ecx,g_edx,g_esi,g_esp,g_edi,g_ebp};}
static void restore_registers(const GuestRegisters *r){g_eax=r->a;g_ebx=r->b;g_ecx=r->c;g_edx=r->d;g_esi=r->si;g_esp=r->sp;g_edi=r->di;g_ebp=r->bp;}
static uint32_t guest_call(void (*fn)(void),unsigned n,...){
    uint32_t sp=g_esp,args[4];va_list ap;va_start(ap,n);
    for(unsigned i=0;i<n;i++)args[i]=va_arg(ap,uint32_t);va_end(ap);
    for(unsigned i=n;i;i--){g_esp-=4;MEM32(g_esp)=args[i-1];}
    g_esp-=4;MEM32(g_esp)=0;fn();g_esp=sp;return g_eax;
}
void sub_00074760(void){g_eax=focused;g_esp+=4;}
void sub_0007C4F0(void){++accepted;g_esp+=4;}
void sub_0007CDE0(void){++cancelled;g_esp+=4;}
void sub_000746D0(void){++refreshes;g_esp+=4;}
void sub_0007CCA0(void){const char *s=(char *)&MEM8(BUFFER);MEM32(DONE+0x28)=!*s || strspn(s," ")==strlen(s);g_esp+=4;}
void sub_00071940(void){g_eax=DONE;g_esp+=4;}
#ifdef ORIGINAL_KEYBOARD
void sub_0007D960(void){g_esp+=4;}
void sub_0003FA10(void){g_esp+=4;}
void sub_000405C0(void){g_fp_top=(g_fp_top+7)&7;g_fp_stack[g_fp_top]=(double)strlen((char *)&MEM8(BUFFER))*10;g_esp+=4;}
void sub_00040650(void){g_eax=(uint32_t)strlen((char *)&MEM8(MEM32(g_esp+4)));g_esp+=4;}
/* ORIGINAL_ROUTINES */
#else
/* CI without copyrighted assets verifies dispatch and the original page/index
 * ABI. Local verified-XBE coverage below executes real insertion/edit code. */
void sub_0007DB20(void){++insertions;g_esp+=4;}
void sub_0007C410(void){g_esp+=4;}
void sub_0007C300(void){g_esp+=4;}
void sub_0007C390(void){g_esp+=4;}
#endif
#include "menu_guest_region.h"
#include "pc_menu_text.inc"
static void setup(void){
    memset(memory,0,sizeof memory);queue=(PcTextInput){0};g_xbox_mem_offset=(ptrdiff_t)memory;
    g_esp=0x700000;g_eax=11;g_ebx=12;g_ecx=13;g_edx=14;g_esi=15;
    focused=1;accepted=cancelled=insertions=refreshes=0;
    MEM32(MENU+0x178)=DONE;MEM32(DONE+4)=LETTER;
    MEM32(DONE+0x38)=0x7da20;MEM32(LETTER+0x38)=0x7db20;
    MEM32(MENU+4)=0x4d8d78;MEM32(MENU+0x1b0)=3;MEM32(MENU+0x220)=BUFFER;
    MEM32(MENU+0x230)=16;MEM32(MENU+0x228)=0x10;MEM32(MENU+0x18c)=7;
    MEM32(MENU+0x22c)=8;strcpy((char *)&MEM8(BUFFER),"Player 1");
    memcpy(&MEM8(0x4d8e74),"1234567890abcdefghijklmnopqrstuvwxyz,.",38);
    memcpy(&MEM8(0x4d8f44),"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ,.",38);
    MEM8(0x4d8e74+40)=MEM8(0x4d8f44+40)=' ';
    memcpy(&MEM8(0x4d9254),"abcdefghijklmnopqrstuvwxyz,. ",29);
    memcpy(&MEM8(0x4d92ec),"ABCDEFGHIJKLMNOPQRSTUVWXYZ,. ",29);
    memcpy(&MEM8(0x4d9384),"1234567890!$%&*@?()[]-+_:;,. ",29);
    /* ORIGINAL_CONSTANTS */
    menu_text_focus(MENU);CHECK(queue.owner==MENU);
}
static void process(void){menu_text_events(MENU);CHECK(g_eax==11 && g_ebx==12 && g_ecx==13 && g_edx==14 && g_esi==15 && g_esp==0x700000);}
int main(void){
    /* Repeated keyboard detection on large non-keyboard menus must not make
     * one OS query per row. Non-monotonic links exercise full region reuse. */
    g_xbox_mem_offset=(ptrdiff_t)memory;
    for(unsigned i=0;i<200;i++) {
        uint32_t row=0x10000+(199-i)*0x110;
        if(!i)MEM32(MENU+0x178)=row;
        MEM32(row+4)=i==199?0:row-0x110;
    }
    region_queries=0;
    CHECK(!menu_text_row(MENU,0x7db20,UINT32_MAX) && region_queries==1);
    MEM32(0x10000+0x38)=0x7db20;MEM32(0x10000+0x14)=17;
    CHECK(menu_text_row(MENU,0x7db20,17)==0x10000 && region_queries==2);
    CHECK(!menu_text_row(MENU,0x7db20,18));
    /* Never carry permission facts across scans, accept a partial row, or
     * follow an invalid pointer. Cyclic lists remain bounded. */
    region_writable=0;CHECK(!menu_text_row(MENU,0x7db20,17));region_writable=1;
    MEM32(MENU+0x178)=0x10000;region_limit=0x1010b;
    CHECK(!menu_text_row(MENU,0x7db20,17));region_limit=sizeof memory;
    CHECK(menu_text_row(MENU,0x7db20,17)==0x10000);
    MEM32(0x10000+4)=0xfffffff0;CHECK(!menu_text_row(MENU,0x1234,17));
    MEM32(0x10000+4)=sizeof memory;CHECK(!menu_text_row(MENU,0x1234,17));
    MEM32(0x10000+4)=0x10000;region_queries=0;
    CHECK(!menu_text_row(MENU,0x1234,17) && region_queries==1);
    MEM32(MENU+0x178)=0x10000;MEM32(0x10000+4)=0x20000;
    MEM32(0x20000+0x38)=0x1234;MEM32(0x20000+0x14)=17;
    region_split=0x18000;region_queries=0;
    CHECK(menu_text_row(MENU,0x1234,17)==0x20000 && region_queries==2);
    region_second_writable=0;CHECK(!menu_text_row(MENU,0x1234,17));
    region_split=0;region_second_writable=1;
    PcTextInput q={0};pc_text_character(&q,'a');CHECK(!q.count);
    pc_text_session(&q,1);pc_text_utf8(&q,"Efy 9\xc3\xa9\xf0\x9f\x98\x80");
    const unsigned expected[]={'E','f','y',' ','9',233};
    for(unsigned i=0;i<6;i++)CHECK(pc_text_get(&q)==expected[i]);CHECK(!pc_text_get(&q));
    for(unsigned i=0;i<70;i++)pc_text_put(&q,'a');CHECK(q.count==64);
    pc_text_session(&q,2);CHECK(!q.count);pc_text_command(&q,13,1,0);CHECK(!q.count);
    pc_text_command(&q,13,0,0);CHECK(pc_text_get(&q)==PC_TEXT_ACCEPT);
    pc_text_command(&q,'A',0,1);CHECK(pc_text_get(&q)==PC_TEXT_SELECT_ALL);
    pc_text_character(&q,10);pc_text_character(&q,127);pc_text_character(&q,0xd800);CHECK(!q.count);
    CHECK(pc_text_key('E') && pc_text_key('F') && pc_text_key('Y') && pc_text_key(32));
    CHECK(!pc_text_key(1) && !pc_text_key(0x7a));
    setup();pc_text_character(&queue,'E');focused=0;process();CHECK(queue.count==1);focused=1;process();CHECK(!queue.count);
    CHECK(MEM32(MENU+0x18c)==7 && !(MEM32(MENU+0x228)&0x100e));
    pc_text_put(&queue,PC_TEXT_ACCEPT);process();CHECK(accepted==1 && !queue.owner);
    setup();pc_text_put(&queue,PC_TEXT_CANCEL);pc_text_put(&queue,'z');process();CHECK(!cancelled && !queue.count && !queue.owner);menu_text_input(MENU);CHECK(MEM32(MENU+0xc4)==0x20);
    /* Actual Create Profile descriptor, including stale buffer on page exit. */
    setup();MEM32(MENU+4)=0x4e8d54;MEM32(MENU+0x228)|=0x180;
    menu_text_focus(MENU);CHECK(queue.owner==MENU);
    pc_text_character(&queue,'E');process();CHECK(!queue.count);
    MEM32(LETTER+0x38)=0;menu_text_focus(MENU);CHECK(!queue.owner);
    setup();MEM32(MENU+4)=0x4d94d4;menu_text_focus(MENU);CHECK(!queue.owner);
    setup();MEM32(MENU+0x230)=0xffffffff;pc_text_put(&queue,'z');process();CHECK(!queue.owner);
#ifdef ORIGINAL_KEYBOARD
    setup();MEM32(MENU+4)=0x4e8d54;MEM32(MENU+0x228)|=0x180;pc_text_utf8(&queue,"Cortez Efy");process();CHECK(!strcmp((char *)&MEM8(BUFFER),"Cortez Efy"));
    CHECK(MEM32(MENU+0x22c)==10 && !(MEM32(MENU+0x228)&0x10));
    pc_text_put(&queue,PC_TEXT_BACKSPACE);process();CHECK(!strcmp((char *)&MEM8(BUFFER),"Cortez Ef"));
    pc_text_put(&queue,PC_TEXT_HOME);pc_text_put(&queue,PC_TEXT_BACKSPACE);process();CHECK(!strcmp((char *)&MEM8(BUFFER),"Cortez Ef"));
    pc_text_put(&queue,PC_TEXT_DELETE);process();CHECK(!strcmp((char *)&MEM8(BUFFER),"ortez Ef"));
    pc_text_put(&queue,PC_TEXT_END);pc_text_put(&queue,PC_TEXT_DELETE);process();CHECK(!strcmp((char *)&MEM8(BUFFER),"ortez Ef"));
    pc_text_put(&queue,PC_TEXT_SELECT_ALL);pc_text_utf8(&queue,"123456789012345678901234567890");process();CHECK(strlen((char *)&MEM8(BUFFER))==16);
    pc_text_put(&queue,PC_TEXT_SELECT_ALL);pc_text_put(&queue,PC_TEXT_BACKSPACE);pc_text_put(&queue,PC_TEXT_ACCEPT);process();CHECK(!accepted && queue.owner==MENU && !MEM8(BUFFER));
    /* Compact layout, uppercase/symbol page, original visible-width rejection. */
    MEM32(MENU+0x228)|=0x80;pc_text_utf8(&queue,"ABC_9");process();CHECK(!strcmp((char *)&MEM8(BUFFER),"ABC_9"));
    pc_text_put(&queue,PC_TEXT_SELECT_ALL);pc_text_put(&queue,PC_TEXT_BACKSPACE);process();
    MEMF(MENU+0x238)=30;MEMF(0x7ba038)=1;pc_text_utf8(&queue,"ABCDE");process();CHECK(!strcmp((char *)&MEM8(BUFFER),"ABC"));
#endif
    puts("PASS: text sessions, queue boundaries, Latin-1, original keyboard dispatch and edit limits");return 0;
}
