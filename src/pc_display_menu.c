/* US Xbox build 45410066. Insert through the title's own button builder:
   Settings sub_002CBFC0, and paused Options sub_002F5520. Original controls,
   audio/video, layout, highlighting and navigation stay in the guest engine. */
#include "pc_display_menu.h"
#include "pc_prompts.h"
#ifdef TSFP_PC_DISCORD
#include "pc_discord_guest.h"
#include "pc_guest_memory.h"
#ifdef _WIN32
#include "pc_network.h"
#endif
#endif
#include "pc_display.h"
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
static int pc_story_menu_button(void (*original)(void));
#ifdef _WIN32
static int pc_story_lobby_button(void (*original)(void));
#endif

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
       (WORD_AT(actor+0x114)!=2 && WORD_AT(actor+0x114)!=4) ||
       WORD_AT(actor+8)!=WORD_AT(0x007B0CC0u)) return 1.f;
    /* US sub_00258F50 enters state 4 when ground contact is lost; the
       landing path returns to state 2. Both retain the player's own lens. */
    return pc_display_fov_scale();
}

#ifdef TSFP_PC_DISCORD
static int discord_guest_read(void *context,uint32_t address,void *out,size_t bytes)
{
    uintptr_t host=(uintptr_t)address+g_xbox_mem_offset;
    (void)context;
    if(!address || bytes>UINT32_MAX-address || !pc_guest_memory(host,bytes,0)) return 0;
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
        pc_discord_submit(&sample);discord_sampled=now;
    }
#endif
    static float last_factor;
    uint32_t player, matrix;
    float factor, zoom, fov_scale=gameplay_fov_scale(WORD_AT(g_esp));
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
    /* US sub_001B73F0 builds the first-person weapon lens; sub_002C44D0
       rebuilds the character picker lens, including when models change.
       Neither goes through pc_display_camera. Correct each freshly built
       matrix, rather than copying the world camera's different FOV/zoom or
       adjusting a retained matrix repeatedly while a new character loads. */
    if(caller==0x001B7482u || caller==0x002C46EFu) {
        if(guest_writable(destination,64)) {
            float *projection=(float *)((uintptr_t)destination+g_xbox_mem_offset);
            projection[0]/=pc_display_camera_factor();
        }
        return;
    }
    /* These two US decal passes rebuild a perspective matrix with biased
       near/far planes instead of using the camera projection. Match the
       active camera's aperture, including widescreen and cinematic zoom,
       while retaining the original depth bias. Other perspective builders
       include menus and offscreen effects and must keep their own aperture. */
    if(caller!=0x00112F7Fu && caller!=0x00115B0Cu) return;
    camera=WORD_AT(0x007B0CC0u);
    if(!guest_writable(camera,0x208) || !guest_writable(destination,64)) return;
    matrix=WORD_AT(camera+0x204u);
    if(!guest_writable(matrix,64)) return;
    WORD_AT(destination)=WORD_AT(matrix);
    WORD_AT(destination+0x14u)=WORD_AT(matrix+0x14u);
}

void pc_display_menu_button(void (*original)(void))
{
    static uint32_t caption;
    static unsigned reported;
    const uint32_t callsite=WORD_AT(g_esp), callback=WORD_AT(g_esp+20);
    const int frontend=callsite==0x002CC116u && callback==0x002CBE50u;
    const int paused=callsite==0x002F561Cu && callback==0x002EC1A0u;
    GuestRegisters r;
    if(pc_story_menu_button(original)) return;
#ifdef _WIN32
    if(pc_story_lobby_button(original)) return;
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
#define FOV_DRAW_CB 0x1200eu
extern void sub_00079B70(void), sub_00075B70(void), sub_00071B10(void);
extern void sub_00071C50(void), sub_00079D60_gen(void), sub_000771D0(void);
extern void sub_00078980(void), sub_000770E0(void);
extern void sub_00079710(void), sub_000814B0(void), sub_00154FE0(void), sub_00074910(void);
extern void sub_0007D480(void), sub_000717D0(void), sub_00075430(void);
static uint32_t storage, parent_menu, previous_menu, current_menu;
static PcDisplayMode modes[PC_DISPLAY_MODE_CAPACITY], all_modes[PC_DISPLAY_MODE_CAPACITY], staged, previous;
static int all_mode_count, selected_aspect;
static int mode_count, selected_mode, page, staged_fov, previous_fov;
static uint32_t resolution_row, aspect_row;
static uint32_t texture_row;
static unsigned texture_choice;
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
static void row(int id,const char *label)
{
    guest_call(sub_00079D60_gen,6,text_at(id+1,"%s",label),(uint32_t)id,0u,0u,SELECT_CB,0u);
}
static void fov_label(void) { text_at(8,"Vertical FOV: %d",staged_fov); }
#if defined(_WIN32) || defined(TSFP_NATIVE_DISPLAY)
static void menu_slider_fov_pointer(float x,float left,float width)
{
    unsigned value=pc_menu_slider_value(x,left,width,PC_FOV_MAX-PC_FOV_MIN);
    if(page==0) {
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
        if(page==0)
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
static void labels(void)
{
    resolution_row=aspect_row=texture_row=0;
    if(page==0) {
        int i;
        aspect_row=guest_call(sub_00079D60_gen,6,text_at(7,"Aspect ratio"),0u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        for(i=0;i<PC_DISPLAY_ASPECT_COUNT;i++)
            guest_call(sub_000717D0,1,text_at(12+PC_DISPLAY_MODE_CAPACITY+i,"%s",pc_display_aspect_name(i)));
        guest_call(sub_00075430,2,aspect_row,(uint32_t)selected_aspect);
        /* The same native choice builder as the original Controls menu
           (US sub_002EA3F0): it owns arrow drawing, input repeat and wrapping.
           +0xC8 is its selected choice, not its +0xC0 text alignment. */
        resolution_row=guest_call(sub_00079D60_gen,6,text_at(1,"Resolution"),1u,0u,0u,0u,0u);
        /* 0x8000 selects the native right-aligned value column; the second
           parameter is extra row height, not a horizontal offset. */
        guest_call(sub_0007D480,2,0x8400u,0u);
        for(i=0;i<mode_count;i++) {
            uint32_t caption=pc_display_is_auto(modes[i])?text_at(12+i,"Auto"):modes[i].width?text_at(12+i,"%u x %u",modes[i].width,modes[i].height):
                text_at(12+i,"Original");
            guest_call(sub_000717D0,1,caption);
        }
        guest_call(sub_00075430,2,resolution_row,(uint32_t)(selected_mode<0?0:selected_mode));
        fov_label();
        guest_call(sub_00079D60_gen,6,TEXT_ADDRESS(8),2u,0x400u,0u,0u,FOV_DRAW_CB);
        guest_call(sub_00079710,1,1u);
        texture_choice=(unsigned)tsfp_texture_is_4k();
        texture_row=guest_call(sub_00079D60_gen,6,text_at(2,"Textures"),7u,0u,0u,0u,0u);
        guest_call(sub_0007D480,2,0x8400u,0u);
        guest_call(sub_000717D0,1,text_at(3,"Classic"));
        guest_call(sub_000717D0,1,text_at(10,"Enhanced"));
        guest_call(sub_00075430,2,texture_row,texture_choice);
        row(3,"Apply settings"); row(4,"Reset FOV to 55");
        row(5,"Back");
    } else if(page==1) { row(0,"Keep resolution"); row(1,"Revert"); }
    else {
#ifdef _WIN32
        /* Long feedback does not fit the original narrow page-header region. */
        guest_call(sub_00079D60_gen,6,text_at(10,"%s",status),1u,0u,0u,SELECT_CB,0u);
#endif
        row(0,"Back");
    }
}
static void read_resolution(void)
{
    if(page==0 && resolution_row) {
        unsigned choice=WORD_AT(resolution_row+0xc8);
        if(choice<(unsigned)mode_count) { selected_mode=(int)choice; staged=modes[choice]; }
    }
}
static void build_menu(void)
{
    GuestRegisters r; uint32_t menu=WORD_AT(g_esp+4);
    save_registers(&r); current_menu=menu;
    /* The builder uses a 0..100 percentage canvas (sub_000771D0 centres
       at (100-width)/2), not the painter's 640-unit coordinates. Reserve
       75% for label + longest resolution + arrows without clipping rows.
       Resolution confirmation keeps its original 55% width. */
    const char *title=page?status:"Display settings";
    float width=page==0?75.f:55.f;
#ifdef _WIN32
    if(page==2){title="Display settings";width=75.f;}
#endif
    guest_call(sub_00075B70,5,0xa000u,text_at(0,"%s",title),bits(width),0u,BACK_CB);
    guest_call(sub_00071B10,4,1u,bits(2.f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    labels(); guest_call(sub_000771D0,1,menu);
    restore_registers(&r); g_esp+=4;
}
static void refresh(int next,int selected)
{
    page=next;
    resolution_row=aspect_row=texture_row=0; /* Rebuild may finish on a later guest tick. */
    guest_call(sub_00078980,1,current_menu);
    /* The guest refresh restores choices by row ID after our builder runs.
       The resolution list has changed: reapply the new list's index after
       that restoration, otherwise the old index clamps/wraps to its first item. */
    if(page==0 && resolution_row && aspect_row) {
        guest_call(sub_00075430,2,aspect_row,(uint32_t)selected_aspect);
        guest_call(sub_00075430,2,resolution_row,(uint32_t)selected_mode);
    }
    if(page==0 && texture_row) {
        /* Guest refresh can restore an old choice over the current F10 state. */
        texture_choice=(unsigned)tsfp_texture_is_4k();
        guest_call(sub_00075430,2,texture_row,texture_choice);
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
        int i;
        pc_display_set_fov(previous_fov); staged_fov=previous_fov;
        staged=previous; deadline=0;selected_aspect=0;filter_modes();
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
        read_resolution();
        if(selected==3) {
            previous=pc_display_current(); previous_fov=pc_display_fov();
            if(staged.width==previous.width && staged.height==previous.height) {
                pc_display_set_fov(staged_fov);
                if(!pc_display_save()) {
                    pc_display_set_fov(previous_fov);
                    snprintf(status,sizeof status,"Save failed"); refresh(2,0);
                }
            } else if(pc_display_preview(staged)) {
                pc_display_set_fov(staged_fov);
                deadline=GetTickCount64()+15000;
                snprintf(status,sizeof status,"Keep changes? 15s"); refresh(1,1);
            } else {
                snprintf(status,sizeof status,"Cannot apply"); refresh(2,0);
            }
        } else if(selected==4) { staged_fov=PC_FOV_DEFAULT; fov_label(); }
        else if(selected==5) leave();
    } else if(page==1) {
        if(selected==0) {
            if(pc_display_save()) { deadline=0; refresh(0,0); }
            else { revert(); if(!deadline) { snprintf(status,sizeof status,"Save failed"); refresh(2,0); } }
        } else revert();
    }
    else refresh(0,0);
    restore_registers(&r); g_esp+=4;
}
static void tick_menu(void)
{
    GuestRegisters r; uint32_t menu=WORD_AT(g_esp+4); save_registers(&r);
    read_resolution();
    if(page==0 && texture_row) {
        unsigned choice=WORD_AT(texture_row+0xc8);
        if(choice<=1u && choice!=texture_choice)
            tsfp_texture_set_4k((int)choice);
        /* Native arrows apply immediately. Otherwise reflect changes from
           F10/host menus without letting a stale guest row undo them. */
        texture_choice=(unsigned)tsfp_texture_is_4k();
        if(choice!=texture_choice)
            guest_call(sub_00075430,2,texture_row,texture_choice);
    }
    if(page==0 && aspect_row) {
        unsigned aspect=WORD_AT(aspect_row+0xc8);
        if(aspect<PC_DISPLAY_ASPECT_COUNT && aspect!=(unsigned)selected_aspect) {
            selected_aspect=(int)aspect;filter_modes();refresh(0,0);
        }
    }
    if(page==0 && WORD_AT(menu+0x18c)==2) {
        uint32_t buttons=WORD_AT(menu+0xc4);
        guest_call(sub_00074910,3,menu,0u,1u);
        if((buttons&0x40000u) && staged_fov>PC_FOV_MIN) --staged_fov;
        else if((buttons&0x80000u) && staged_fov<PC_FOV_MAX) ++staged_fov;
        fov_label();
    }
    if(page==1) {
        ULONGLONG now=GetTickCount64();
        if(now>=deadline) revert();
        else text_at(0,"Keep changes? %us",(unsigned)((deadline-now+999)/1000));
    }
    restore_registers(&r); g_esp+=4;
}
static void activate(void)
{
    GuestRegisters r; uint32_t source=WORD_AT(g_esp+4);
    save_registers(&r);
    if(!storage) {
        storage=xbox_HeapAlloc(64+TEXT_BYTES*(12+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT),4);
        if(storage) {
            memset((void *)((uintptr_t)storage+g_xbox_mem_offset),0,64+TEXT_BYTES*(12+PC_DISPLAY_MODE_CAPACITY+PC_DISPLAY_ASPECT_COUNT));
            WORD_AT(storage)=BUILD_CB; WORD_AT(storage+4)=TICK_CB;
        }
    }
    if(storage) {
        staged=pc_display_current(); staged_fov=pc_display_fov();
        all_mode_count=pc_display_modes(xbox_D3D8Window(),all_modes,PC_DISPLAY_MODE_CAPACITY);
        selected_aspect=0;filter_modes();
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
#include "pc_story_menu.inc"
#include "pc_story_lobby.inc"
#include "pc_story_load.inc"
void (*pc_display_menu_lookup(uint32_t address))(void)
{
#ifdef _WIN32
    void (*story_callback)(void)=story_lobby_lookup(address);
    if(story_callback) return story_callback;
#endif
    switch(address) {
    case PC_DISPLAY_MENU_CALLBACK: return activate;
    case BUILD_CB: return build_menu;
    case TICK_CB: return tick_menu;
    case SELECT_CB: return select_callback;
    case BACK_CB: return back_callback;
    case FOV_DRAW_CB: return fov_draw;
    case STORY_OPEN_CB: return story_open;
    case STORY_BUILD_CB: return story_build;
    case STORY_SELECT_CB: return story_select;
    case STORY_BACK_CB: return story_back;
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
