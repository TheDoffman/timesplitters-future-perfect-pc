/* US Xbox build 45410066. Insert through the title's own button builder:
   Settings sub_002CBFC0, and paused Options sub_002F5520. Original controls,
   audio/video, layout, highlighting and navigation stay in the guest engine. */
#include "pc_display_menu.h"
#include "pc_display.h"
#include "pc_online.h"
#include <stdarg.h>

#include "recomp_types.h"
#include <stdio.h>
#include <string.h>

extern uint32_t xbox_HeapAlloc(uint32_t bytes, uint32_t alignment);
#define WORD_AT(a) (*(uint32_t *)((uintptr_t)(uint32_t)(a) + g_xbox_mem_offset))

typedef struct GuestRegisters {
    uint32_t a,c,d,sp,b,si,di,bp,seh;
    RecompXmm xmm[8];
    double fp[8];
    int fp_top,fp_cmp,df;
    uint16_t fp_cc,fp_control;
} GuestRegisters;

static void save_registers(GuestRegisters *r)
{
    r->a=g_eax; r->c=g_ecx; r->d=g_edx; r->sp=g_esp;
    r->b=g_ebx; r->si=g_esi; r->di=g_edi; r->bp=g_ebp; r->seh=g_seh_ebp;
    r->xmm[0]=g_xmm0; r->xmm[1]=g_xmm1; r->xmm[2]=g_xmm2; r->xmm[3]=g_xmm3;
    r->xmm[4]=g_xmm4; r->xmm[5]=g_xmm5; r->xmm[6]=g_xmm6; r->xmm[7]=g_xmm7;
    memcpy(r->fp,g_fp_stack,sizeof r->fp);
    r->fp_top=g_fp_top; r->fp_cmp=g_fp_cmp; r->df=g_df;
    r->fp_cc=g_fp_cc; r->fp_control=g_fp_control_word;
}

static void restore_registers(const GuestRegisters *r)
{
    g_eax=r->a; g_ecx=r->c; g_edx=r->d; g_esp=r->sp;
    g_ebx=r->b; g_esi=r->si; g_edi=r->di; g_ebp=r->bp; g_seh_ebp=r->seh;
    g_xmm0=r->xmm[0]; g_xmm1=r->xmm[1]; g_xmm2=r->xmm[2]; g_xmm3=r->xmm[3];
    g_xmm4=r->xmm[4]; g_xmm5=r->xmm[5]; g_xmm6=r->xmm[6]; g_xmm7=r->xmm[7];
    memcpy(g_fp_stack,r->fp,sizeof r->fp);
    g_fp_top=r->fp_top; g_fp_cmp=r->fp_cmp; g_df=r->df;
    g_fp_cc=r->fp_cc; g_fp_control_word=r->fp_control;
}

static void push(uint32_t word)
{ g_esp-=4; WORD_AT(g_esp)=word; }

static int guest_writable(uint32_t address,size_t bytes)
{
    MEMORY_BASIC_INFORMATION m;
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    if(!address || bytes>UINT32_MAX-address ||
       !VirtualQuery((const void *)host,&m,sizeof m) || m.State!=MEM_COMMIT ||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return 0;
    return (m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) &&
        host+bytes>=host && host+bytes<=(uintptr_t)m.BaseAddress+m.RegionSize;
}

void pc_display_camera(void (*original)(void))
{
    static float last_factor;
    uint32_t player, matrix;
    float factor;
    original();
    /* US build sub_000C77C0 creates the player's perspective matrix through
       sub_00151CF0, then writes tan(vertical FOV/2) at +0x37c and the
       horizontal frustum extent at +0x380. Widen both projection and CPU
       culling together, after the original's cutscene aspect clamp. */
    player=WORD_AT(0x007B0CC0u);
    /* Camera allocations use the Xbox 0x80000000 RAM alias, not low VAs. */
    if(!guest_writable(player,0x384)) return;
    matrix=WORD_AT(player+0x204u);
    if(!guest_writable(matrix,64)) return;
    factor=pc_display_camera_factor();
    if(factor!=last_factor) {
        last_factor=factor;
        fprintf(stderr,"[PC-DISPLAY] Camera horizontal factor %.6f; projection and CPU frustum\n",factor);
    }
    if(factor!=1.f) {
        float *projection=(float *)((uintptr_t)matrix+g_xbox_mem_offset);
        float *horizontal=(float *)((uintptr_t)(player+0x380u)+g_xbox_mem_offset);
        projection[0]/=factor;
        *horizontal*=factor;
    }
}

void pc_display_menu_button(void (*original)(void))
{
    static uint32_t caption;
    static unsigned reported;
    const uint32_t callsite=WORD_AT(g_esp), callback=WORD_AT(g_esp+20);
    const int frontend=callsite==0x002CC116u && callback==0x002CBE50u;
    const int paused=callsite==0x002F561Cu && callback==0x002EC1A0u;
    GuestRegisters r;
    if(callsite==0x001CC0A3u && callback==0x001CC050u) {
        static uint32_t online_caption;
        const uint32_t frame=g_esp, previous_caption=WORD_AT(frame+4);
        /* Replace the original Xbox Live row in place. Its font, layout and
           position above System Link remain owned by the original builder. */
        if(!online_caption) {
            const char text[]="ONLINE";
            online_caption=xbox_HeapAlloc(sizeof text,4);
            if(online_caption) memcpy((void *)((uintptr_t)online_caption+g_xbox_mem_offset),text,sizeof text);
        }
        if(online_caption) {
            WORD_AT(frame+4)=online_caption;
            WORD_AT(frame+20)=0x1200du;
        }
        original();
        WORD_AT(frame+4)=previous_caption; WORD_AT(frame+20)=callback;
        return;
    }
    original();
    if(!frontend && !paused) return;
    save_registers(&r);
    if(!caption) {
        const char text[]="Display settings";
        caption=xbox_HeapAlloc(sizeof text,4);
        if(caption) memcpy((void *)((uintptr_t)caption+g_xbox_mem_offset),text,sizeof text);
    }
    if(caption) {
        /* Same six-argument cdecl ABI as the adjacent original Controls item.
           At these two sites its return value is unused and no subsequent
           setter acts on it, so appending cannot redirect an existing setter. */
        push(0); push(PC_DISPLAY_MENU_CALLBACK); push(0); push(0); push(UINT32_MAX); push(caption);
        push(callsite);
        original();
        if(!(reported & (paused?2u:1u))) {
            reported|=paused?2u:1u;
            fprintf(stderr,"[PC-DISPLAY-MENU] Display settings added to %s\n",paused?"paused Options":"main Settings");
        }
    }
    restore_registers(&r);
}


/* All five callbacks occupy unused INT3 padding, inside the guest code guard.
   The original XBE remains unchanged. A menu descriptor is four guest VAs:
   construct, update, draw, destroy, as consumed by sub_00078E50. */
#define BUILD_CB 0x12009u
#define TICK_CB 0x1200au
#define SELECT_CB 0x1200bu
#define BACK_CB 0x1200cu
extern void sub_00079B70(void), sub_00075B70(void), sub_00071B10(void);
extern void sub_00071C50(void), sub_00079D60_gen(void), sub_000771D0(void);
extern void sub_00078980(void), sub_000770E0(void);
static uint32_t storage, parent_menu, previous_menu, current_menu;
static PcDisplayMode modes[64], staged, previous;
static int mode_count, selected_mode, page;
static ULONGLONG deadline;
static char status[192];
/* Fixed backing storage survives transitions and renderer references. */
#define TEXT_BYTES 192
#define TEXT_ADDRESS(i) (storage+64u+(uint32_t)(i)*TEXT_BYTES)
static uint32_t text_at(int i,const char *format,...)
{
    va_list args;
    va_start(args,format);
    vsnprintf((char *)((uintptr_t)TEXT_ADDRESS(i)+g_xbox_mem_offset),TEXT_BYTES,format,args);
    va_end(args);
    return TEXT_ADDRESS(i);
}
static uint32_t guest_call(void (*fn)(void),unsigned count,...)
{
    GuestRegisters r;
    uint32_t args[8],result; unsigned i;
    va_list ap;
    save_registers(&r);
    va_start(ap,count); for(i=0;i<count;i++) args[i]=va_arg(ap,uint32_t); va_end(ap);
    for(i=count;i;i--) push(args[i-1]);
    push(PC_DISPLAY_MENU_CALLBACK); fn(); result=g_eax;
    restore_registers(&r);
    return result;
}
static uint32_t bits(float f) { uint32_t u; memcpy(&u,&f,4); return u; }
static void row(int id,const char *label)
{
    guest_call(sub_00079D60_gen,6,text_at(id+1,"%s",label),(uint32_t)id,0u,0u,SELECT_CB,0u);
}
static void labels(void)
{
    char b[192];
    if(page==0) {
        if(staged.width) snprintf(b,sizeof b,"Resolution: %u x %u",staged.width,staged.height);
        else snprintf(b,sizeof b,"Resolution: Original");
        row(0,b); row(1,"Previous resolution"); row(2,"Apply resolution"); row(3,"Back");
    } else if(page==1) { row(0,"Keep resolution"); row(1,"Revert"); }
    else row(0,"Back");
}
static void build_menu(void)
{
    GuestRegisters r; uint32_t menu=WORD_AT(g_esp+4);
    save_registers(&r); current_menu=menu;
    guest_call(sub_00075B70,5,0xa000u,text_at(0,"%s",page?status:"Display settings"),bits(55.f),0u,BACK_CB);
    guest_call(sub_00071B10,4,1u,bits(2.f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    labels(); guest_call(sub_000771D0,1,menu);
    restore_registers(&r); g_esp+=4;
}
static void refresh(int next,int selected)
{
    page=next;
    guest_call(sub_00078980,1,current_menu);
    guest_call(sub_000770E0,2,current_menu,(uint32_t)selected);
}
static void leave(void)
{
    guest_call(sub_00079B70,2,parent_menu,previous_menu);
    fprintf(stderr,"[PC-DISPLAY-MENU] Returned to original menu\n");
}
static void revert(void)
{
    if(pc_display_apply(previous)) {
        int i;
        staged=previous; deadline=0;
        selected_mode=-1;
        for(i=0;i<mode_count;i++) if(modes[i].width==staged.width && modes[i].height==staged.height) selected_mode=i;
        fprintf(stderr,"[PC-DISPLAY-MENU] Reverted\n"); refresh(0,0);
    } else {
        /* Keep retrying from tick; never save an unconfirmed display mode. */
        text_at(0,"Restoring resolution");
    }
}
static void back_callback(void)
{
    GuestRegisters r; save_registers(&r);
    if(page==1) revert(); else if(page==0) leave(); else refresh(0,0);
    restore_registers(&r); g_esp+=4;
}
static void select_callback(void)
{
    GuestRegisters r; uint32_t menu=WORD_AT(g_esp+4);
    int selected=(int)WORD_AT(menu+0x18c);
    save_registers(&r);
    fprintf(stderr,"[PC-DISPLAY-MENU] Selected page %d row %d\n",page,selected);
    if(page==0) {
        if((selected==0 || selected==1) && mode_count) {
            selected_mode=selected_mode<0?(selected==0?0:mode_count-1):
                (selected_mode+(selected==0?1:mode_count-1))%mode_count;
            staged=modes[selected_mode]; refresh(0,selected);
        } else if(selected==2) {
            previous=pc_display_current();
            if(pc_display_preview(staged)) {
                deadline=GetTickCount64()+15000;
                snprintf(status,sizeof status,"Keep changes? 15s"); refresh(1,1);
            } else {
                snprintf(status,sizeof status,"Cannot apply"); refresh(2,0);
            }
        } else if(selected==3) leave();
    } else if(page==1) {
        if(selected==0) {
            if(pc_display_save()) { deadline=0; refresh(0,0); }
            else { revert(); if(!deadline) { snprintf(status,sizeof status,"Save failed"); refresh(2,0); } }
        } else revert();
    } else refresh(0,0);
    restore_registers(&r); g_esp+=4;
}
static void tick_menu(void)
{
    GuestRegisters r; save_registers(&r);
    if(page==1) {
        ULONGLONG now=GetTickCount64();
        if(now>=deadline) revert();
        else text_at(0,"Keep changes? %us",(unsigned)((deadline-now+999)/1000));
    }
    restore_registers(&r); g_esp+=4;
}
static void activate(void)
{
    GuestRegisters r; uint32_t source=WORD_AT(g_esp+4); int i;
    save_registers(&r);
    if(!storage) {
        storage=xbox_HeapAlloc(64+TEXT_BYTES*12,4);
        if(storage) {
            memset((void *)((uintptr_t)storage+g_xbox_mem_offset),0,64+TEXT_BYTES*12);
            WORD_AT(storage)=BUILD_CB; WORD_AT(storage+4)=TICK_CB;
        }
    }
    if(storage) {
        staged=pc_display_current();
        mode_count=pc_display_modes(xbox_D3D8Window(),modes,64); selected_mode=-1;
        for(i=0;i<mode_count;i++) if(modes[i].width==staged.width && modes[i].height==staged.height) selected_mode=i;
        parent_menu=WORD_AT(source+0x2c); previous_menu=WORD_AT(source+4);
        page=0; deadline=0;
        fprintf(stderr,"[PC-DISPLAY-MENU] Opened, %d resolutions\n",mode_count);
        guest_call(sub_00079B70,2,parent_menu,storage);
    }
    restore_registers(&r); g_esp+=4;
}
extern void sub_001A0A60(void);
static void system_link_activate(void)
{
    GuestRegisters r; save_registers(&r);
    pc_online_disconnect();
    _putenv_s("TSFP_ONLINE_CONFIG",""); _putenv_s("TSFP_NETWORK","1");
    guest_call(sub_001A0A60,0);
    restore_registers(&r); g_esp+=4;
}
static void online_activate(void)
{
    GuestRegisters r; save_registers(&r);
    if(pc_online_connect()) guest_call(sub_001A0A60,0);
    restore_registers(&r); g_esp+=4;
}
void (*pc_display_menu_lookup(uint32_t address))(void)
{
    switch(address) {
    case PC_DISPLAY_MENU_CALLBACK: return activate;
    case BUILD_CB: return build_menu;
    case TICK_CB: return tick_menu;
    case SELECT_CB: return select_callback;
    case BACK_CB: return back_callback;
    case 0x1200du: return online_activate;
    /* Any remaining indirect reference to the retired Live entry uses Online. */
    case 0x001CC050u: return online_activate;
    /* System Link is reached through the title's indirect menu table. Online
       deliberately calls its generated body directly after configuring the
       bridge, while ordinary System Link selection resets to LAN transport. */
    case 0x001A0A60u: return system_link_activate;
    default: return NULL;
    }
}
