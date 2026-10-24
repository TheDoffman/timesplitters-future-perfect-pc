/* US Xbox build 45410066. Insert through the title's own button builder:
   Settings sub_002CBFC0, and paused Options sub_002F5520. Original controls,
   audio/video, layout, highlighting and navigation stay in the guest engine. */
#include "pc_display_menu.h"
#include "pc_settings.h"
#define KB_OPEN 0x1200fu
static uint32_t kb_caption(void);
#include "pc_display_memory.h"
#include "pc_prompts.h"
#ifdef TSFP_PC_DISCORD
#include "pc_discord_guest.h"
#include "pc_breadcrumbs.h"
#include "pc_guest_memory.h"
#ifdef _WIN32
#include "pc_network.h"
#endif
#endif
#include "pc_display.h"
#include "pc_native_rate.h"
#include "pc_frame_rate_settings.h"
#include "texture_mode.h"
#include "pc_cutscene.h"
#ifndef _WIN32
#include "platform/win32_compat.h"
/* Win32 protection values not produced by the POSIX mapper. */
#define PAGE_GUARD 0x100
#define PAGE_EXECUTE_WRITECOPY 0x80
#endif
#ifdef _WIN32
#include "pc_online.h"
#endif
#include <stdarg.h>
#include <stdlib.h>

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
    /* Every caller supplies a known extent. Query it afresh: these checks may
     * occur on either side of guest callbacks and cannot share VM snapshots. */
    PcGuestRegion region={0,0,0};
    return pc_display_writable(&region,g_xbox_mem_offset,address,bytes);
}

static float cutscene_camera_zoom(void)
{
    PcDisplayMode mode = pc_display_effective();
    uint32_t viewport = WORD_AT(0x007E71C8u);
    int height, full, animated;
    if (WORD_AT(0x007DE458u) & 0x20000000u) {
        /* sub_001CEBC0's story renderer calls sub_0006F150; its special
         * 9/16 mode omits the matte (sub_0003C420). */
        if (WORD_AT(0x004C045Cu)==9 && WORD_AT(0x004C0458u)==16) return 1.f;
        height = (int32_t)WORD_AT(0x004E8CF4u);
        full = animated = (int)(height * .145f);
    } else {
        float phase;
        uint32_t bits = WORD_AT(0x007490ECu);
        memcpy(&phase,&bits,sizeof phase);
        if (WORD_AT(0x007490E8u) || !(phase>0 && phase<=1) ||
            !guest_writable(viewport,0x1C)) return 1.f;
        height = (int32_t)WORD_AT(viewport+0x18u) -
                 (int32_t)WORD_AT(viewport+0x14u) + 1;
        full = 71;
        animated = (int)(70 * phase) + 1;
    }
    return pc_cutscene_camera_zoom(mode.width,mode.height,height,full,animated);
}

static float gameplay_fov_scale(uint32_t caller)
{
    uint32_t actor=WORD_AT(0x007B0C7Cu), phase_bits=WORD_AT(0x007490ECu);
    float phase;
    memcpy(&phase,&phase_bits,4);
    /* Only the two player-camera projections in sub_000CCAB0. Story camera
       sub_000C95E0, terminal views and scripted letterbox transitions retain
       their authored lens. Do not rewrite gameplay/weapon zoom state. */
    /* Frontend scenes reuse a player camera and state 2. Match the signed
       scene > 100 test in US sub_001CE9A0 to retain their authored lens. */
    if((int32_t)WORD_AT(0x0079094Cu)>100 ||
       (caller!=0x000CCBEDu && caller!=0x000CDD35u) ||
       (WORD_AT(0x007DE458u)&0x20000000u) ||
       (!WORD_AT(0x007490E8u) && phase>0.f) ||
       !guest_writable(actor,0x118) ||
       (WORD_AT(actor+0x114)!=2 && WORD_AT(actor+0x114)!=4 &&
        WORD_AT(actor+0x114)!=0x800) ||
       WORD_AT(actor+8)!=WORD_AT(0x007B0CC0u)) return 1.f;
    /* US sub_00258F50 enters state 4 when ground contact is lost; the
       landing path returns to state 2. US sub_00225C70 enters state 0x800
       inside Zeppelin air lifts and returns to 4 on exit. All three states
       retain the player's own lens, including weapon zoom. */
    return pc_display_fov_scale();
}

#ifdef TSFP_PC_DISCORD
static int discord_guest_read(void *context,uint32_t address,void *out,size_t bytes)
{
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    /* Each decoder read has a known extent. Validate it afresh without walking
     * the end of the whole guest mapping for every string byte. */
    PcGuestRegion region={0,0,0};
    (void)context;
    if(!address || bytes>UINT32_MAX-address ||
       !pc_guest_memory_in_span(&region,host,bytes,0)) return 0;
    memcpy(out,(const void *)host,bytes);return 1;
}
#endif

void pc_display_camera(void (*original)(void))
{
#ifdef TSFP_PC_DISCORD
    /* Sample guest RAM on its own thread; publish only plain values to IPC. */
    static ULONGLONG discord_sampled;
    static PcDiscordGuestCache discord_cache;
    ULONGLONG now=GetTickCount64();
    if(now-discord_sampled>=1000) {
        PcDiscordSnapshot sample;
        int network=0;
#ifdef _WIN32
        double latency;unsigned peers;
        network=pc_network_latency(&latency,&peers);
#endif
        pc_discord_sample(&sample,discord_guest_read,NULL,&discord_cache,network);
        pc_capture_gameplay(sample.scene,sample.mode,sample.menu_mode,sample.paused,sample.network,(sample.flags&0x20000000u)!=0);
        pc_discord_submit(&sample);discord_sampled=now;
    }
#endif
    static float last_factor;
    uint32_t player, matrix;
    float factor, zoom, fov_scale=gameplay_fov_scale(WORD_AT(g_esp));
    original();
    PcGuestRegion region={0,0,0};
    /* US build sub_000C77C0 creates the player's perspective matrix through
       sub_00151CF0, then writes tan(vertical FOV/2) at +0x37c and the
       horizontal frustum extent at +0x380. Widen both projection and CPU
       culling together, after the original's cutscene aspect clamp. */
    player=WORD_AT(0x007B0CC0u);
    /* Camera allocations use the Xbox 0x80000000 RAM alias, not low VAs. */
    if(!pc_display_writable(&region,g_xbox_mem_offset,player,0x384)) return;
    matrix=WORD_AT(player+0x204u);
    if(!pc_display_writable(&region,g_xbox_mem_offset,matrix,64)) return;
    factor=pc_display_camera_factor();
    zoom=cutscene_camera_zoom();
    if(factor!=last_factor) {
        last_factor=factor;
        fprintf(stderr,"[PC-DISPLAY] Camera horizontal factor %.6f; projection and CPU frustum\n",factor);
    }
    if(factor!=1.f || zoom!=1.f || fov_scale!=1.f) {
        float *projection=(float *)((uintptr_t)matrix+g_xbox_mem_offset);
        float *horizontal=(float *)((uintptr_t)(player+0x380u)+g_xbox_mem_offset);
        float *vertical=(float *)((uintptr_t)(player+0x37cu)+g_xbox_mem_offset);
        /* Fit the cinematic aperture; ultrawide crops vertically to fill.
         * Widening past that aperture exposes authored backdrop edges.
         * Keep GPU projection and CPU culling in agreement on both axes. */
        projection[0]*=zoom/(factor*fov_scale);
        projection[5]*=zoom/fov_scale;
        *horizontal*=factor*fov_scale/zoom;
        *vertical*=fov_scale/zoom;
    }
}

void pc_display_perspective_projection(void (*original)(void))
{
    const uint32_t caller=WORD_AT(g_esp), destination=WORD_AT(g_esp+4);
    uint32_t camera, matrix;
    original();
    /* US 151CF0 unconditionally writes all 16 destination floats before
       returning, with no guest callbacks. Its successful return establishes
       this output's writable extent; do not query it again below. Camera
       sources still need a fresh, correction-local permission check. */
    PcGuestRegion region={0,0,0};
    /* US sub_001B73F0 builds the first-person weapon lens; sub_002C44D0
       rebuilds the character picker lens, including when models change.
       Neither goes through pc_display_camera. Correct each freshly built
       matrix, rather than copying the world camera's different FOV/zoom or
       adjusting a retained matrix repeatedly while a new character loads. */
    if(caller==0x001B7482u || caller==0x002C46EFu) {
        float *projection=(float *)((uintptr_t)destination+g_xbox_mem_offset);
        projection[0]/=pc_display_camera_factor();
        return;
    }
    /* These two US decal passes rebuild a perspective matrix with biased
       near/far planes instead of using the camera projection. Match the
       active camera's aperture, including widescreen and cinematic zoom,
       while retaining the original depth bias. Other perspective builders
       include menus and offscreen effects and must keep their own aperture. */
    if(caller!=0x00112F7Fu && caller!=0x00115B0Cu) return;
    camera=WORD_AT(0x007B0CC0u);
    if(!pc_display_writable(&region,g_xbox_mem_offset,camera,0x208)) return;
    matrix=WORD_AT(camera+0x204u);
    if(!pc_display_writable(&region,g_xbox_mem_offset,matrix,64)) return;
    WORD_AT(destination)=WORD_AT(matrix);
    WORD_AT(destination+0x14u)=WORD_AT(matrix+0x14u);
}

static uint32_t audio_caption;
static uint32_t audio_options_caption(void)
{
    if(!audio_caption) {
        const char label[]="Audio Options";
        audio_caption=xbox_HeapAlloc(sizeof label,4);
        if(audio_caption)memcpy((void *)((uintptr_t)audio_caption+g_xbox_mem_offset),label,sizeof label);
    }
    return audio_caption;
}
static void sd_width(void);
static void sd_buttons(void (*original)(void));
static void sd_capacity(void (*original)(void));
void pc_display_menu_button(void (*original)(void))
{
    static uint32_t caption;
    static unsigned reported;
    const uint32_t callsite=WORD_AT(g_esp), callback=WORD_AT(g_esp+20);
    const int frontend=callsite==0x002CC116u && callback==0x002CBE50u;
    const int paused=callsite==0x002F561Cu && callback==0x002EC1A0u;
    GuestRegisters r;
    if(callsite==0x0019440bu)sd_width();
    if(callsite==0x001944c3u && callback==0x00192e60u){sd_buttons(original);return;}
    if(callsite==0x0019450au){sd_capacity(original);return;}
    if(callback==0x002CBE10u || callback==0x002EC1F0u) {
        uint32_t frame=g_esp,old_caption=WORD_AT(frame+4),label=audio_options_caption();
        uint32_t builder=WORD_AT(0x007BB1E0u),flags=0;
        int valid=callback==0x002CBE10u && guest_writable(builder,0x2c);
        if(label)WORD_AT(frame+4)=label;
        /* The frontend uses left-aligned rows. Pause Options instead inherits
         * its own centered/split-screen style, just like Controls and Video.
         * Renaming Audio must not replace that context-dependent layout. */
        if(valid) { flags=WORD_AT(builder+0x28);WORD_AT(builder+0x28)=0x26200u; }
        original();WORD_AT(frame+4)=old_caption;
        if(valid)WORD_AT(builder+0x28)=flags;
        return;
    }
    if(callsite==0x002C4042u) {
        /* Keep the native update/save record, but remove its visual row and
         * navigation target; Video Options now owns subtitle changes. */
        uint32_t frame=g_esp,flags=WORD_AT(frame+12);
        WORD_AT(frame+12)=flags|0x21u;original();WORD_AT(frame+12)=flags;return;
    }
#ifdef _WIN32
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
#endif
    if(!frontend && !paused) { original();return; }
    save_registers(&r);
    if(!caption) {
        const char text[]="Video Options";
        caption=xbox_HeapAlloc(sizeof text,4);
        if(caption) memcpy((void *)((uintptr_t)caption+g_xbox_mem_offset),text,sizeof text);
    }
    if(caption) {
        /* Insert Video before Controls, using the same six-argument cdecl
           ABI. Restore the caller before building Controls so its arguments
           and return value still belong to the original row. */
        push(0); push(PC_DISPLAY_MENU_CALLBACK); push(0); push(0); push(UINT32_MAX); push(caption);
        push(callsite);
        original();
        if(!(reported & (paused?2u:1u))) {
            reported|=paused?2u:1u;
            fprintf(stderr,"[PC-DISPLAY-MENU] Video Options added to %s\n",paused?"paused Options":"main Settings");
        }
    }
    restore_registers(&r);
    if(kb_caption()) {
        push(0);push(KB_OPEN);push(0);push(0);push(UINT32_MAX);push(kb_caption());push(callsite);
        original();restore_registers(&r);
    }
    original();
}


/* All five callbacks occupy unused INT3 padding, inside the guest code guard.
   The original XBE remains unchanged. A menu descriptor is four guest VAs:
   construct, update, draw, destroy, as consumed by sub_00078E50. */
#define BUILD_CB 0x12009u
#define TICK_CB 0x1200au
#define SELECT_CB 0x1200bu
#define BACK_CB 0x1200cu
#define FOV_DRAW_CB 0x1200eu
extern void sub_00079B70(void), sub_00075B70(void), sub_00071B10(void);
extern void sub_00071C50(void), sub_00079D60_gen(void), sub_000771D0(void);
extern void sub_00078980(void), sub_000770E0(void);
extern void sub_000814B0(void), sub_00154FE0(void), sub_00074910(void);
extern void sub_0007D480(void), sub_000717D0(void), sub_00075430(void);
static uint32_t storage, parent_menu, previous_menu, current_menu;
static PcDisplayMode modes[PC_DISPLAY_MODE_CAPACITY], all_modes[PC_DISPLAY_MODE_CAPACITY], staged, previous;
static int all_mode_count, selected_aspect;
enum { DISPLAY_HOME, DISPLAY_CONFIRM };
static int mode_count, selected_mode, page, staged_fov, previous_fov;
static int staged_rate, applied_rate;
static unsigned applied_texture;
static uint32_t apply_row;
static uint32_t rate_row,subtitle_row;
static unsigned staged_subtitles,applied_subtitles;
static uint32_t resolution_row, aspect_row;
static uint32_t texture_row;
static unsigned texture_choice;
static ULONGLONG deadline;
static char status[192];
/* Fixed backing storage survives transitions and renderer references. */
#define TEXT_BYTES 192
#define MODE_TEXT_BASE 32
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
    uint32_t args[12],result; unsigned i;
    va_list ap;
    save_registers(&r);
    va_start(ap,count); for(i=0;i<count;i++) args[i]=va_arg(ap,uint32_t); va_end(ap);
    for(i=count;i;i--) push(args[i-1]);
    push(PC_DISPLAY_MENU_CALLBACK); fn(); result=g_eax;
    restore_registers(&r);
    return result;
}
#include "pc_profile_accept.inc"
#include "pc_weapon_slots_guest.inc"

/* Recreate only a missing guest body using the same constructor as the
 * original Arcade initializer. Keep the existing actor-list entry and scores;
 * never run guest AI decisions or add a duplicate entry to 0x7A3580. */
uint32_t pc_network_bots_revive(unsigned slot)
{
#ifdef _WIN32
    extern void sub_000DAC30(void),sub_000D7570(void);
    const uint32_t pool=WORD_AT(0x007356d8u);
    const unsigned humans=*(uint8_t *)((uintptr_t)0x007de400u+g_xbox_mem_offset);
    const unsigned bots=*(uint8_t *)((uintptr_t)0x007de402u+g_xbox_mem_offset);
    uint32_t actor,entity,character,variant;
    if(!humans || !bots || bots>10u || humans+bots>16u || slot<humans || slot>=humans+bots ||
       !pool || WORD_AT(0x0076b140u)!=2u || WORD_AT(0x0076b1a4u)!=15u) return 0;
    actor=pool+slot*0xea0u;
    if(!guest_writable(actor,0xea0u) || WORD_AT(actor)) return 0;
    character=*(uint8_t *)((uintptr_t)(0x007de414u+slot)+g_xbox_mem_offset);
    variant=(uint32_t)(int32_t)*(int8_t *)((uintptr_t)(0x007de444u+slot)+g_xbox_mem_offset);
    entity=guest_call(sub_000DAC30,10,character,character,(uint32_t)slot,
        0u,0u,0u,0u,UINT32_MAX,1u,variant);
    if(!entity || !guest_writable(entity,0x25cu) || WORD_AT(actor)!=entity) return 0;
    WORD_AT(0x007a2d00u+(slot-humans)*4u)=entity;
    guest_call(sub_000D7570,1,actor);
    return entity;
#else
    (void)slot;return 0;
#endif
}
void pc_network_bots_entity_refresh(uint32_t entity)
{
#ifdef _WIN32
    extern void sub_0009FBE0(void);
    if(guest_writable(entity,0x2e8u)) {
        /* +32 is the transform MODE, not a dirty flag. +28 bit 0x1000
           invalidates the cached rotation consumed by sub_000A14B0. */
        WORD_AT(entity+0x28u)|=0x1000u;
        guest_call(sub_0009FBE0,1,entity);
    }
#else
    (void)entity;
#endif
}
void pc_network_bots_present(uint32_t entity,const PcNetworkBotState *state)
{
#ifdef _WIN32
    extern void sub_000966E0(void);
    uint32_t model,speed,actor;unsigned hand;
    const int32_t clip=state->clip;
    int16_t next_index;
    if(!guest_writable(entity,0x238u) || clip<0 || clip>4095 || state->next_clip<0 || state->next_clip>4095) return;
    actor=WORD_AT(entity+0x7cu);
    if(guest_writable(actor,0xea0u)) for(hand=0;hand<2u;hand++) {
        uint32_t weapon=WORD_AT(actor+0x18cu+hand*0x130u);
        if(guest_writable(weapon,0x2cu)) {
            if(state->body_present && state->health>0.f && (state->entity_flags&1u)) WORD_AT(weapon+0x28u)|=1u;
            else WORD_AT(weapon+0x28u)&=~1u;
        }
    }
    if(!state->body_present) return;
    model=WORD_AT(entity+4u);
    if(!guest_writable(model,0x200u)) return;
    if((int32_t)WORD_AT(model+0xd4u)!=clip) {
        memcpy(&speed,state->clip_channels+11,4);
        guest_call(sub_000966E0,3,model,(uint32_t)clip,speed);
    }
    memcpy((void *)((uintptr_t)(model+0xdcu)+g_xbox_mem_offset),state->clip_channels,72);
    WORD_AT(model+0x124u)=state->clip_cycle;WORD_AT(model+0x128u)=state->clip_flags&~0xc00u;
    /* The original character animator crossfades a second numeric clip block
       into the first. Resolve its asset index locally; never copy host handles. */
    next_index=*(int16_t *)((uintptr_t)(0x007e03c0u+(uint32_t)state->next_clip*2u)+g_xbox_mem_offset);
    WORD_AT(model+0x12cu)=(uint32_t)state->next_clip;WORD_AT(model+0x130u)=(uint32_t)(int32_t)next_index;
    memcpy((void *)((uintptr_t)(model+0x134u)+g_xbox_mem_offset),state->next_channels,72);
    WORD_AT(model+0x17cu)=state->next_cycle;WORD_AT(model+0x180u)=state->next_flags&~0xc00u;
    WORD_AT(model+0xa8u)=state->overlay_phase;
    memcpy((void *)((uintptr_t)(model+0xacu)+g_xbox_mem_offset),state->overlay_channels,32);
    memcpy((void *)((uintptr_t)(model+0x198u)+g_xbox_mem_offset),state->blend_channels,12);
    /* 0xc00 carries pending root translation/rotation, already included in
       the host's body transform. Do not apply it twice. Bone buffers belong
       to the frame arena: evaluate in the normal animation pass, not here.
       sub_00096C20 is lighting preparation, not skeleton evaluation. */
#else
    (void)entity;(void)state;
#endif
}
void pc_network_bots_pose_weapons(uint32_t entity)
{
#ifdef _WIN32
    extern void sub_001BE870(void);
    float health;
    if(!guest_writable(entity,0x2e8u)) return;
    memcpy(&health,(void *)((uintptr_t)(entity+0x1ccu)+g_xbox_mem_offset),4);
    if(health>0.f && (WORD_AT(entity+0x28u)&1u)) {
        /* Only after the engine has allocated/evaluated this frame's bones. */
        guest_call(sub_001BE870,2,entity,0u);
        guest_call(sub_001BE870,2,entity,1u);
    }
#else
    (void)entity;
#endif
}
/* Send the original reliable hit request without predicting NPC health/death.
   The normal serializer still checks local ownership of the damage source. */
void pc_network_bots_request_hit(uint32_t context)
{
#ifdef _WIN32
    extern void sub_0035C870(void);
    if(guest_writable(context,0x58u)) guest_call(sub_0035C870,1,context);
#else
    (void)context;
#endif
}
/* Announce only a confirmed increase in the host's victim score column.
   Do not replay the damage/death routine: it mutates health, scores and models. */
int pc_network_bots_kill_notice(unsigned victim,unsigned killer)
{
#ifdef _WIN32
    extern void sub_000CF440(void);
    uint32_t pool=WORD_AT(0x007356d8u),players=WORD_AT(0x007b0c48u),player,source;
    unsigned humans=*(uint8_t *)((uintptr_t)0x007de400u+g_xbox_mem_offset);
    if(killer>=humans || victim>=16u || killer==victim) return 1;
    if(!guest_writable(pool,16u*0xea0u) || !guest_writable(players,humans*0x1584u)) return 0;
    player=players+killer*0x1584u;
    if((int32_t)WORD_AT(player+4u)<0) return 1; /* remote player's kill */
    source=WORD_AT(player+0x14u);
    if(!WORD_AT(player+8u) || !guest_writable(source,0x25cu) ||
       WORD_AT(source+0x7cu)!=pool+killer*0xea0u) return 0;
    /* The normal localized "You killed" HUD path uses the victim actor;
       unlike the announcer it needs neither a corpse nor a damage context. */
    guest_call(sub_000CF440,2,player,pool+victim*0xea0u);
    return 1;
#else
    (void)victim;(void)killer;
    return 1;
#endif
}
void pc_network_bots_present_shot(uint32_t entity,const PcNetworkBotShot *shot)
{
#ifdef _WIN32
    extern void sub_001BB5C0(void);
    uint32_t actor,target,pool,previous_target,weapon,result;
    if(!guest_writable(entity,0x25cu) || shot->hand>1u || shot->mode>1u || shot->flags>1u ||
       shot->weapon<0 || shot->weapon>=66 || shot->target_slot>=16u ||
       WORD_AT(0x0076b140u)!=2u || WORD_AT(0x0076b1a4u)!=15u) return;
    actor=WORD_AT(entity+0x7cu);pool=WORD_AT(0x007356d8u);
    if(!guest_writable(actor,0xea0u) || !guest_writable(pool,16u*0xea0u)) return;
    target=WORD_AT(pool+shot->target_slot*0xea0u);
    if(!guest_writable(target,0x25cu)) return;
    weapon=actor+0xa4u+shot->hand*0x130u;
    /* A late shot from the previous weapon must not use the new model. */
    if(WORD_AT(weapon+0x30u)!=(uint32_t)shot->weapon ||
       !guest_writable(WORD_AT(weapon+0xe8u),0x24u)) return;
    previous_target=WORD_AT(actor+4u);WORD_AT(actor+4u)=target;
    /* Preserve local model/owner handles at weapon + E8/EC. */
    memcpy((void *)((uintptr_t)weapon+g_xbox_mem_offset),shot->transform,48);
    memcpy((void *)((uintptr_t)(actor+0xb88u)+g_xbox_mem_offset),shot->aim,12);
    memcpy((void *)((uintptr_t)(actor+0xbb4u)+g_xbox_mem_offset),shot->muzzle,12);
    result=guest_call(sub_001BB5C0,4,entity,shot->hand,shot->mode,shot->flags);
    WORD_AT(actor+4u)=previous_target;
    {
        static unsigned traces;
        const char *path=getenv("TSFP_BOT_TRACE");
        if(path && *path && traces<128u) {
            char name[1024];FILE *log;
            if(snprintf(name,sizeof name,"%s.shots",path)<(int)sizeof name) {
                log=fopen(name,traces++?"a":"w");
                if(log) {fprintf(log,"slot=%u serial=%u weapon=%d hand=%u mode=%u result=%u\n",
                    WORD_AT(actor+8u),shot->serial,shot->weapon,shot->hand,shot->mode,result);fclose(log);}
            }
        }
    }
#else
    (void)entity;(void)shot;
#endif
}
void pc_network_bots_menu(void (*original)(void))
{
#ifdef _WIN32
    const uint32_t mode=WORD_AT(0x0078AD2Cu);
    const int host=WORD_AT(0x0076B108u) && WORD_AT(0x0076B140u)==1u &&
        mode>=11u && mode<=13u;
    /* Only the synchronous menu construction sees Arcade's setup mode. This
       runs the title's own Bot Set row, dropdown, edit callback, positioning
       and footer layout; the transport and installed callbacks stay original.
       Restore the real mode before returning to the lobby or processing input. */
    if(host) {
        WORD_AT(0x0078AD2Cu)=9u;
        _putenv_s("TSFP_NETWORK_BOTS","1");
    }
    original();
    if(host) WORD_AT(0x0078AD2Cu)=mode;
#else
    original();
#endif
}
#include "pc_quit_menu.inc"
#include "pc_menu_mouse_guest.inc"
#include "pc_display_ui.inc"
#include "pc_tutorial_ui.inc"

static uint32_t bits(float f) { uint32_t u; memcpy(&u,&f,4); return u; }
/* The title's focus painter takes (menu, row, float bounds[4]). Replace
 * every shared focus bar, retaining native background, tint and fade. */
extern void sub_002D9390(void);
void pc_display_menu_highlight(void (*original)(void))
{
    extern void sub_00072510(void), sub_00019D70(void), sub_0001EFB0(void);
    uint32_t menu=WORD_AT(g_esp+4),rect=WORD_AT(g_esp+12),style;
    float bounds[4],fade;
    int left,top,right,bottom,radius=6;
    uint32_t color;
    if(!guest_writable(menu,0xf8) || !guest_writable(rect,16) ||
       !guest_writable(style=WORD_AT(menu+0x20),0x80)) {
        original();return;
    }
    memcpy(bounds,(void *)((uintptr_t)rect+g_xbox_mem_offset),sizeof bounds);
    memcpy(&fade,(void *)((uintptr_t)(menu+0xf4)+g_xbox_mem_offset),4);
    left=(int)bounds[0];top=(int)bounds[1];
    right=(int)bounds[2];bottom=(int)bounds[3];
    if(right-left<12 || bottom-top<12){original();return;}
    if(original!=sub_002D9390)guest_call(sub_00072510,2,menu,(uint32_t)(int)(127.f*fade));
    guest_call(sub_00019D70,1,original==sub_002D9390?2u:8u);
    color=guest_call(sub_00154FE0,2,original==sub_002D9390?0x2f3c503cu:WORD_AT(style+0x74),bits(fade));
    /* Circular six-unit corners; disjoint strips avoid alpha accumulation. */
    for(int y=0;y<bottom-top;y++) {
        static const unsigned inset[6]={4,2,1,1,0,0};
        int edge=y<radius?y:bottom-top-1-y;
        unsigned cut=edge<radius?inset[edge]:0;
        int end=edge>=radius?bottom-top-radius:y+1;
        guest_call(sub_0001EFB0,5,(uint32_t)(left+cut),(uint32_t)(top+y),
            (uint32_t)(right-cut),(uint32_t)(top+end),color);
        y=end-1;
    }
    g_esp+=4;
}
/* Frontend style dispatches its focus painter indirectly; paused menus may
 * use the generic painter instead. Both use the same rounded geometry. */
static void display_highlight_draw(void) { pc_display_menu_highlight(sub_002D9390); }
static void audio_options_build(void)
{
    extern void sub_002C3E20(void);
    uint32_t menu=WORD_AT(g_esp+4);
    sub_002C3E20();
    uint32_t caption=audio_options_caption();
    if(caption && guest_writable(menu,0x208))WORD_AT(menu+0x204)=caption;
}
/* Reuse the title's Settings actions and profile selector, with a single
 * left-aligned list and Cheats last. The native back/save flow is unchanged. */
static void settings_build(void)
{
    extern void sub_0003D7B0(void),sub_001CD120(void),sub_00069720(void);
    extern void sub_00071CF0(void);
    static uint32_t video_caption;
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4);save_registers(&r);
    if(!video_caption) {
        const char label[]="Video Options";
        video_caption=xbox_HeapAlloc(sizeof label,4);
        if(video_caption)memcpy((void *)((uintptr_t)video_caption+g_xbox_mem_offset),label,sizeof label);
    }
    guest_call(sub_00075B70,5,0x8008000u,guest_call(sub_0003D7B0,1,0x409u),bits(70.f),0u,0x002CBFA0u);
    WORD_AT(menu+0x218)=9u;
    guest_call(sub_00071B10,4,1u,bits(2.f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    guest_call(sub_00079D60_gen,6,audio_options_caption(),UINT32_MAX,0u,0u,0x002CBE10u,0u);
    guest_call(sub_00079D60_gen,6,video_caption,UINT32_MAX,0u,0u,PC_DISPLAY_MENU_CALLBACK,0u);
    if(kb_caption())guest_call(sub_00079D60_gen,6,kb_caption(),UINT32_MAX,0u,0u,KB_OPEN,0u);
    guest_call(sub_00079D60_gen,6,guest_call(sub_0003D7B0,1,0x17A4u),UINT32_MAX,0u,0u,0x002CBE50u,0u);
    if(WORD_AT(0x005236C4u)>1u) {
        uint32_t selector=guest_call(sub_00079D60_gen,6,guest_call(sub_0003D7B0,1,0x39Cu),0u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0xA000u,0u);
        for(uint32_t i=0;i<WORD_AT(0x005236C4u);i++)
            guest_call(sub_000717D0,1,guest_call(sub_00069720,1,i));
        guest_call(sub_00071CF0,2,selector,0x002CBEE0u);
        guest_call(sub_00075430,2,selector,WORD_AT(0x0078AD34u));
    }
    guest_call(sub_00079D60_gen,6,0x0047602Cu,1u,0u,0u,0x002CBEC0u,0u);
    if(guest_call(sub_001CD120,0))
        guest_call(sub_00079D60_gen,6,guest_call(sub_0003D7B0,1,0x1A0Fu),UINT32_MAX,0u,0u,0x002CBE30u,0u);
    guest_call(sub_000771D0,1,menu);
    restore_registers(&r);g_esp+=4;
}
static void row(int id,const char *label)
{
    guest_call(sub_00079D60_gen,6,text_at(id+1,"%s",label),(uint32_t)id,0u,0u,SELECT_CB,0u);
}
static void fov_label(void) { text_at(8,"Vertical FOV: %d",staged_fov); }
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY)
static void menu_slider_fov_pointer(float x,float left,float width)
{
    unsigned value=pc_menu_slider_value(x,left,width,PC_FOV_MAX-PC_FOV_MIN);
    if(page==DISPLAY_HOME) {
        staged_fov=PC_FOV_MIN+(int)value;
        fov_label();
    }
}
#endif
static void fov_draw(void)
{
    GuestRegisters r;
    uint32_t menu=WORD_AT(g_esp+4), rect=WORD_AT(g_esp+12);
    save_registers(&r);
    if(guest_writable(menu,0xf8) && guest_writable(rect,16)) {
        float bounds[4]; uint32_t dark,light;
        memcpy(bounds,(void *)((uintptr_t)rect+g_xbox_mem_offset),sizeof bounds);
        dark=guest_call(sub_00154FE0,2,0x11193c7fu,WORD_AT(menu+0xf4));
        light=guest_call(sub_00154FE0,2,0x69737f7fu,WORD_AT(menu+0xf4));
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY)
        if(page==DISPLAY_HOME)
            menu_slider_capture(MENU_FOV,menu,WORD_AT(g_esp+8),
                (float)(int)(bounds[2]-102.f),(float)(int)((bounds[1]+bounds[3]-8.f)*.5f),100.f,8.f);
#endif
        /* Original sensitivity-slider painter, in the menu's logical units. */
        guest_call(sub_000814B0,9,bits((float)(staged_fov-PC_FOV_MIN)/(PC_FOV_MAX-PC_FOV_MIN)),
            (uint32_t)(int)(bounds[2]-102.f),(uint32_t)(int)((bounds[1]+bounds[3]-8.f)*.5f),
            100u,8u,dark,dark,light,0u);
    }
    restore_registers(&r); g_esp+=4;
}
static void refresh(int next,int selected);
static void filter_modes(void)
{
    mode_count=pc_display_filter_modes(all_modes,all_mode_count,selected_aspect,
        staged,modes,PC_DISPLAY_MODE_CAPACITY,&selected_mode);
    if(mode_count)staged=modes[selected_mode];
}
static void small_note(int slot,const char *label)
{
    uint32_t item=guest_call(sub_00079D60_gen,6,text_at(slot,"\vs75%s\vs100",label),(uint32_t)slot,4u,0u,0u,0u);
    /* Preserve native size escapes instead of uppercasing them into icons. */
    if(guest_writable(item,0x2c))WORD_AT(item+0x28)&=~0x2000u;
}
static int mode_equal(PcDisplayMode a,PcDisplayMode b)
{ return a.width==b.width && a.height==b.height; }
static int display_dirty(void)
{
    return !mode_equal(staged,previous) || staged_fov!=previous_fov ||
        staged_rate!=applied_rate || texture_choice!=applied_texture || staged_subtitles!=applied_subtitles;
}
static void update_apply(void)
{
    if(page==DISPLAY_HOME && guest_writable(apply_row,0x2c)) {
        /* Native disabled-row bit: gray paint and no keyboard/mouse action. */
        if(display_dirty())WORD_AT(apply_row+0x28)&=~1u;
        else WORD_AT(apply_row+0x28)|=1u;
    }
}
static void labels(void)
{
    resolution_row=aspect_row=texture_row=rate_row=subtitle_row=apply_row=0;
    if(page==DISPLAY_HOME) {
        aspect_row=guest_call(sub_00079D60_gen,6,text_at(7,"Aspect ratio"),0u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        for(int i=0;i<PC_DISPLAY_ASPECT_COUNT;i++)
            guest_call(sub_000717D0,1,text_at(MODE_TEXT_BASE+PC_DISPLAY_MODE_CAPACITY+i,"%s",pc_display_aspect_name(i)));
        guest_call(sub_00075430,2,aspect_row,(uint32_t)selected_aspect);
        resolution_row=guest_call(sub_00079D60_gen,6,text_at(1,"Resolution"),1u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        for(int i=0;i<mode_count;i++)
            guest_call(sub_000717D0,1,pc_display_is_auto(modes[i])?text_at(MODE_TEXT_BASE+i,"Auto"):modes[i].width?
                text_at(MODE_TEXT_BASE+i,"%u x %u",modes[i].width,modes[i].height):text_at(MODE_TEXT_BASE+i,"Original"));
        guest_call(sub_00075430,2,resolution_row,(uint32_t)selected_mode);
        fov_label();
        guest_call(sub_00079D60_gen,6,TEXT_ADDRESS(8),2u,0x400u,0u,0u,FOV_DRAW_CB);
        texture_row=guest_call(sub_00079D60_gen,6,text_at(2,"Texture style"),7u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        guest_call(sub_000717D0,1,text_at(10,"Classic"));
        guest_call(sub_000717D0,1,text_at(11,"Enhanced"));
        guest_call(sub_00075430,2,texture_row,texture_choice);
        rate_row=guest_call(sub_00079D60_gen,6,text_at(9,"Frame rate"),8u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        for(int i=0;i<PC_FRAME_RATE_COUNT;i++)
            guest_call(sub_000717D0,1,text_at(24+i,"%s",pc_frame_rate_label(i)));
        guest_call(sub_00075430,2,rate_row,(uint32_t)staged_rate);
        subtitle_row=guest_call(sub_00079D60_gen,6,text_at(12,"Subtitles"),9u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        guest_call(sub_000717D0,1,text_at(13,"Off"));
        guest_call(sub_000717D0,1,text_at(14,"On"));
        guest_call(sub_00075430,2,subtitle_row,staged_subtitles);
        if(*status)small_note(21,status);
        apply_row=guest_call(sub_00079D60_gen,6,text_at(4,"Apply"),3u,0u,0u,SELECT_CB,0u);
        update_apply();
    } else { row(0,"Keep changes");row(1,"Revert"); }
}
static void read_choices(void)
{
    if(page!=DISPLAY_HOME)return;
    if(resolution_row) {
        unsigned choice=WORD_AT(resolution_row+0xc8);
        if(choice<(unsigned)mode_count) { selected_mode=(int)choice;staged=modes[choice]; }
    }
    if(texture_row && WORD_AT(texture_row+0xc8)<=1u)
        texture_choice=WORD_AT(texture_row+0xc8);
    if(subtitle_row && WORD_AT(subtitle_row+0xc8)<=1u)staged_subtitles=WORD_AT(subtitle_row+0xc8);
    if(rate_row && WORD_AT(rate_row+0xc8)<PC_FRAME_RATE_COUNT)
        staged_rate=(int)WORD_AT(rate_row+0xc8);
}
static void build_menu(void)
{
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4);
    save_registers(&r);current_menu=menu;
    guest_call(sub_00075B70,5,0xa000u,text_at(0,"%s",page==DISPLAY_CONFIRM?"Keep changes?":"Video Options"),
        bits(page==DISPLAY_CONFIRM?55.f:85.f),0u,BACK_CB);
    guest_call(sub_00071B10,4,1u,bits(2.f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    labels();guest_call(sub_000771D0,1,menu);
    restore_registers(&r);g_esp+=4;
}
static void refresh(int next,int selected)
{
    page=next;resolution_row=aspect_row=texture_row=rate_row=subtitle_row=apply_row=0;
    guest_call(sub_00078980,1,current_menu);
    /* Guest rebuild restores old choices by row ID. Reapply every staged value,
     * including the newly filtered resolution list, without committing it. */
    if(page==DISPLAY_HOME) {
        if(aspect_row)guest_call(sub_00075430,2,aspect_row,(uint32_t)selected_aspect);
        if(resolution_row)guest_call(sub_00075430,2,resolution_row,(uint32_t)selected_mode);
        if(texture_row)guest_call(sub_00075430,2,texture_row,texture_choice);
        if(rate_row)guest_call(sub_00075430,2,rate_row,(uint32_t)staged_rate);
        if(subtitle_row)guest_call(sub_00075430,2,subtitle_row,staged_subtitles);
        update_apply();
    }
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
        pc_display_set_fov(previous_fov);staged=previous;deadline=0;
        selected_aspect=pc_display_aspect(previous);if(selected_aspect<0)selected_aspect=0;
        filter_modes();*status=0;
        refresh(DISPLAY_HOME,0);
    } else text_at(0,"Restoring resolution");
}
static void back_callback(void)
{
    GuestRegisters r;save_registers(&r);
    if(page==DISPLAY_CONFIRM)revert();else leave();
    restore_registers(&r);g_esp+=4;
}
static void apply_changes(void)
{
    int display_changed=!mode_equal(staged,previous) || staged_fov!=previous_fov;
    int rate_changed=staged_rate!=applied_rate,display_saved=0;
    /* Resolution is already previewed/confirmed. Persist before publishing
     * FPS/texture changes so a failed save leaves those running values alone. */
    pc_display_set_fov(staged_fov);
    if(display_changed) {
        if(!pc_display_save())goto failed;
        display_saved=1;
    }
    if(rate_changed && !pc_frame_rate_save(pc_frame_rate_value(staged_rate)))goto failed;
    if(rate_changed)recomp_config_request_native_rate(pc_frame_rate_value(staged_rate));
    if(staged_subtitles!=applied_subtitles) {
        extern void sub_00069640(void);
        uint32_t profile=guest_call(sub_00069640,0);
        WORD_AT(0x007613DCu)=staged_subtitles;
        if(guest_writable(profile,0x134)) {
            WORD_AT(profile+0x130)=staged_subtitles;
            WORD_AT(0x0078AD24u)|=0x10000u; /* Native profile/settings save flag. */
        }
        applied_subtitles=staged_subtitles;
    }
    tsfp_texture_set_4k((int)texture_choice);
    previous=staged;previous_fov=staged_fov;applied_rate=staged_rate;applied_texture=texture_choice;
    deadline=0;*status=0;filter_modes();
    refresh(DISPLAY_HOME,0);
    return;
failed:
    pc_display_set_fov(previous_fov);
    int restored=mode_equal(pc_display_current(),previous) || pc_display_apply(previous);
    int saved_back=!display_saved || (restored && pc_display_save());
    snprintf(status,sizeof status,saved_back?"Could not save changes. Please try again.":"Could not restore saved display settings.");
    if(!restored) { deadline=GetTickCount64();return; }
    deadline=0;refresh(DISPLAY_HOME,0);
}
static void select_callback(void)
{
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4);
    int selected=(int)WORD_AT(menu+0x18c);save_registers(&r);
    fprintf(stderr,"[PC-DISPLAY-MENU] Selected page %d row %d\n",page,selected);
    if(page==DISPLAY_HOME && selected==3) {
        read_choices();update_apply();
        if(display_dirty()) {
            if(!mode_equal(staged,previous)) {
                if(pc_display_preview(staged)) {
                    deadline=GetTickCount64()+15000;refresh(DISPLAY_CONFIRM,1);
                } else {
                    snprintf(status,sizeof status,"Cannot apply resolution");refresh(DISPLAY_HOME,0);
                }
            } else apply_changes();
        }
    } else if(page==DISPLAY_CONFIRM) {
        if(selected==0)apply_changes();else revert();
    }
    restore_registers(&r);g_esp+=4;
}
static void tick_menu(void)
{
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4);save_registers(&r);
    read_choices();
    if(page==DISPLAY_HOME) {
        unsigned actual=(unsigned)tsfp_texture_is_4k();
        if(actual!=applied_texture) {
            if(texture_choice==applied_texture) {
                texture_choice=actual;
                if(texture_row)guest_call(sub_00075430,2,texture_row,actual);
            }
            applied_texture=actual;
        }
        if(aspect_row) {
            unsigned aspect=WORD_AT(aspect_row+0xc8);
            if(aspect<PC_DISPLAY_ASPECT_COUNT && aspect!=(unsigned)selected_aspect) {
                selected_aspect=(int)aspect;filter_modes();refresh(DISPLAY_HOME,0);
            }
        }
        if(WORD_AT(menu+0x18c)==2) {
            uint32_t buttons=WORD_AT(menu+0xc4);
            guest_call(sub_00074910,3,menu,0u,1u);
            if((buttons&0x40000u) && staged_fov>PC_FOV_MIN)--staged_fov;
            else if((buttons&0x80000u) && staged_fov<PC_FOV_MAX)++staged_fov;
            fov_label();
        }
        update_apply();
    } else {
        ULONGLONG now=GetTickCount64();
        if(now>=deadline)revert();
        else text_at(0,"Keep changes? %us",(unsigned)((deadline-now+999)/1000));
    }
    restore_registers(&r);g_esp+=4;
}
static void activate(void)
{
    GuestRegisters r; uint32_t source=WORD_AT(g_esp+4);
    save_registers(&r);
    if(!storage) {
        storage=xbox_HeapAlloc(64+TEXT_BYTES*(MODE_TEXT_BASE+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT),4);
        if(storage) {
            memset((void *)((uintptr_t)storage+g_xbox_mem_offset),0,64+TEXT_BYTES*(MODE_TEXT_BASE+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT));
            WORD_AT(storage)=BUILD_CB; WORD_AT(storage+4)=TICK_CB;
        }
    }
    if(storage) {
        staged=previous=pc_display_current();staged_fov=previous_fov=pc_display_fov();
        staged_rate=applied_rate=pc_frame_rate_choice(pc_native_rate());
        texture_choice=applied_texture=(unsigned)tsfp_texture_is_4k();*status=0;
        {
            extern void sub_00069640(void);
            uint32_t profile=guest_call(sub_00069640,0);
            staged_subtitles=applied_subtitles=(guest_writable(profile,0x134)?
                WORD_AT(profile+0x130):WORD_AT(0x007613DCu))?1u:0u;
        }
        all_mode_count=pc_display_modes(xbox_D3D8Window(),all_modes,PC_DISPLAY_MODE_CAPACITY);
        selected_aspect=pc_display_aspect(staged);if(selected_aspect<0)selected_aspect=0;filter_modes();
        parent_menu=WORD_AT(source+0x2c); previous_menu=WORD_AT(source+4);
        page=0; deadline=0;
        fprintf(stderr,"[PC-DISPLAY-MENU] Opened, %d resolutions\n",mode_count);
        guest_call(sub_00079B70,2,parent_menu,storage);
    }
    restore_registers(&r); g_esp+=4;
}
#ifdef _WIN32
extern void sub_001A0A60(void);
/* Indirect editor callbacks run after the constructor has returned. Give
   those callbacks the same Arcade Custom rules (including custom bot sets),
   with the network mode restored at every callback boundary. */
extern void sub_002C2C20(void),sub_002C0490(void),sub_002C19F0(void);
extern void sub_002C1570(void),sub_002C1490(void);
static void network_bots_build(void) { pc_network_bots_menu(sub_002C2C20); }
static void network_bots_tick(void) { pc_network_bots_menu(sub_002C0490); }
static void network_bots_list(void) { pc_network_bots_menu(sub_002C19F0); }
static void network_bots_list_tick(void) { pc_network_bots_menu(sub_002C1570); }
static void network_bots_item_draw(void) { pc_network_bots_menu(sub_002C1490); }
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
#endif
#include "pc_keyboard_menu.inc"
#include "pc_storage_menu.inc"

void (*pc_display_menu_lookup(uint32_t address))(void)
{
    if(address==0x002D9390u)return display_highlight_draw;
    if(address==0x002C3E20u)return audio_options_build;
    if(address==0x002CBFC0u)return settings_build;
    switch(address) {
    case 0x00192770u:return sd_profile_tick;
    case SD_SELECT:return sd_select;
    case SD_DRAW:return sd_draw;
    case KB_OPEN:return kb_open;
    case KB_BUILD:return kb_build;
    case KB_TICK:return kb_tick;
    case KB_SELECT:return kb_select;
    case KB_BACK:return kb_back;
    case KB_DRAW:return kb_draw;
    case PC_DISPLAY_MENU_CALLBACK: return activate;
    case BUILD_CB: return build_menu;
    case TICK_CB: return tick_menu;
    case SELECT_CB: return select_callback;
    case BACK_CB: return back_callback;
    case FOV_DRAW_CB: return fov_draw;
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY)
    case QUIT_OPEN_CB: return quit_open;
    case QUIT_BUILD_CB: return quit_build;
    case QUIT_SELECT_CB: return quit_select;
    case QUIT_BACK_CB: return quit_back;
#endif
#ifdef _WIN32
    case 0x1200du: return online_activate;
    case 0x002C2C20u: return network_bots_build;
    case 0x002C0490u: return network_bots_tick;
    case 0x002C19F0u: return network_bots_list;
    case 0x002C1570u: return network_bots_list_tick;
    case 0x002C1490u: return network_bots_item_draw;
    /* Any remaining indirect reference to the retired Live entry uses Online. */
    case 0x001CC050u: return online_activate;
    /* System Link is reached through the title's indirect menu table. Online
       deliberately calls its generated body directly after configuring the
       bridge, while ordinary System Link selection resets to LAN transport. */
    case 0x001A0A60u: return system_link_activate;
#endif
    default: return NULL;
    }
}
