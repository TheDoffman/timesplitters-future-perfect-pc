/* US 45410066 camera composition, CCAB0. D2DE0 publishes a context's feet,
 * eye and movement history; 1519E0/153270/152260 build view, combined and
 * inverse matrices at the same four camera slots used by original CEB98. */
#include "pc_developer.h"
extern void sub_000D2DE0(void),sub_001519E0(void),sub_00153270(void),sub_00152260(void);
extern void sub_0006BB90(void),sub_0025EF80(void),sub_000653B0(void),sub_00067450(void);
/* The bookmark stores inventory and the authored checkpoint INDEX, not a
 * captured position. Original 25EC30 resolves that index through 6BB90 on
 * respawn. Original 25F010/25F0C0 restore checkpoint-indexed scripts as well.
 * Keep capture/reload and mission-success transitions owned by those routines. */
/* Fresh ownership checks also protect detached rendering and local cheats. */
static uint32_t developer_actor(void)
{
    uint32_t phase=WORD_AT(0x7de30cu),context=WORD_AT(0x7b0c48u);
    if((phase!=1u&&phase!=7u)||WORD_AT(0x79094cu)>100u||
       (WORD_AT(0x76b0fcu)&&WORD_AT(0x76b108u))||!guest_writable(context,0x1584u)||WORD_AT(context+4u)!=0u)return 0;
    uint32_t body=WORD_AT(context+0x14u);
    if(!guest_writable(body,0x154u)||WORD_AT(body+0x150u)!=context)return 0;
    uint32_t actor=WORD_AT(body+0x7cu);
    return guest_writable(actor,0xea0u)&&WORD_AT(actor)==body?actor:0;
}
int pc_developer_damage_blocked(void)
{
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t actor=state.god?developer_actor():0;
    if(!actor||!guest_writable(g_esp,8u))return 0;
    uint32_t damage=WORD_AT(g_esp+4u);
    int blocked=guest_writable(damage,0x58u)&&WORD_AT(damage)==WORD_AT(actor);
    static unsigned reports;
    if(blocked&&reports++<16u)fprintf(stderr,"[DEVELOPER] God mode blocked local damage\n");
    return blocked;
}
/* 1B66E0 counts the shot before consuming clip/reserve. Zero only its amount;
 * retain native firing/animation/shot statistics and the caller's arguments. */
void pc_developer_ammo_guest(void (*original)(void))
{
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t actor=state.infinite_ammo?developer_actor():0,frame=g_esp;
    if(!actor||!guest_writable(frame,24u)||WORD_AT(frame+4u)!=actor||
       (int32_t)WORD_AT(frame+16u)<=0){original();return;}
    uint32_t amount=WORD_AT(frame+16u);WORD_AT(frame+16u)=0;
    original();WORD_AT(frame+16u)=amount;
    static unsigned reports;
    if(reports++<16u)fprintf(stderr,"[DEVELOPER] Infinite ammo preserved local shot\n");
}
/* The native grenade transaction decrements reserve directly, outside 1B66E0. */
void pc_developer_grenade_guest(void (*original)(void))
{
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t actor=state.infinite_ammo?developer_actor():0;
    if(!actor||!guest_writable(g_esp,8u)||WORD_AT(g_esp+4u)!=WORD_AT(actor)){original();return;}
    int16_t before[44];memcpy(before,(const void *)((uintptr_t)(actor+0x61eu)+g_xbox_mem_offset),sizeof before);
    original();
    if(developer_actor()!=actor)return;
    for(unsigned i=0;i<44;i++) {
        int16_t after;void *address=(void *)((uintptr_t)(actor+0x61eu+i*2u)+g_xbox_mem_offset);
        memcpy(&after,address,2);if(before[i]>0&&after<before[i])memcpy(address,&before[i],2);
    }
}
static void developer_refill_ammo(void)
{
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t actor=state.infinite_ammo?developer_actor():0;
    if(!actor)return;
    for(unsigned hand=0;hand<2;hand++) {
        uint32_t record=actor+0xa4u+hand*0x130u,mode=WORD_AT(record+0x30u);
        uint32_t descriptors=WORD_AT(0x4f9bacu);
        if(mode>255u||!descriptors||!guest_writable(descriptors+mode*0x29cu,0x29cu))continue;
        uint32_t descriptor=descriptors+mode*0x29cu;
        for(unsigned secondary=0;secondary<2;secondary++) {
            uint32_t kind=WORD_AT(descriptor+0x14u+secondary*8u),capacity=WORD_AT(descriptor+0x18u+secondary*8u);
            if(!kind||kind>=44u||capacity>32767u)continue;
            int16_t clip=(int16_t)capacity;
            memcpy((void *)((uintptr_t)(record+0x38u+kind*2u)+g_xbox_mem_offset),&clip,2);
            int32_t limit=(int32_t)WORD_AT(0x4ea3f8u+kind*0xe0u);
            if(limit<=0||limit>32767)continue;
            int16_t reserve=(int16_t)limit;
            memcpy((void *)((uintptr_t)(actor+0x61eu+kind*2u)+g_xbox_mem_offset),&reserve,2);
        }
    }
}
/* Keep the owned actor and its world attachments included in the detached
 * view. A0130 computes ordinary room/portal visibility; retain its clip bits. */
void pc_developer_world_visibility(void)
{
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t actor=state.freecam?developer_actor():0;
    if(!actor)return;
    uint32_t context=WORD_AT(0x7b0c48u),camera=WORD_AT(context+8u);
    if(!guest_writable(camera,0x218u)||WORD_AT(camera+0x214u)>=16u)return;
    uint32_t bit=1u<<WORD_AT(camera+0x214u),body=WORD_AT(actor);
    WORD_AT(body+0x10u)|=bit;
    for(unsigned hand=0;hand<2;hand++) {
        uint32_t record=actor+0xa4u+hand*0x130u,attachment=WORD_AT(record+0xe8u);
        if(guest_writable(attachment,0x154u) && WORD_AT(attachment+0x7cu)==record && WORD_AT(record+0xecu)==actor)
            WORD_AT(attachment+0x10u)|=bit;
    }
}
/* 1B9120 draws the three local viewport-room passes, including bare hands.
 * Scope the omission to this owned view; model/weapon update still runs. */
void pc_developer_viewmodel_draw(void (*original)(void))
{
    PcDeveloperState state;pc_developer_get(&state);
    if(state.freecam&&developer_actor()&&WORD_AT(0x7b0c7cu)==WORD_AT(0x7b0c48u))g_esp+=4u;
    else original();
}
/* First-person ownership can omit the local actor from colour submission.
 * Draw its existing world model through the native entity renderer, retaining
 * the title's lighting, materials, animation matrices and depth testing. */
extern void sub_000A2740(void);
void pc_developer_draw_body(void)
{
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t actor=state.freecam?developer_actor():0;
    if(!actor||WORD_AT(0x7b0c7cu)!=WORD_AT(0x7b0c48u))return;
    uint32_t body=WORD_AT(actor),model=WORD_AT(body+4u);
    if(!guest_writable(model,0x200u))return;
    uint32_t camera=WORD_AT(0x7b0cc0u);
    if(!guest_writable(camera,0x218u)||WORD_AT(camera+0x214u)>=16u)return;
    /* Native first-person/shadow preparation can leave the view bit set while
     * its colour-pass matrix pointer is empty. Force native A1B80 to prepare
     * the detached world pose; retain its animation, allocator and lighting. */
    WORD_AT(body+0x14u)&=~(1u<<WORD_AT(camera+0x214u));
    for(unsigned pass=0;pass<3;pass++)guest_call(sub_000A2740,2,body,pass);
}
void pc_developer_guest_frame(void)
{
    developer_refill_ammo();
    static unsigned handled;
    PcDeveloperState state;pc_developer_get(&state);
    if(handled==state.checkpoint)return;
    handled=state.checkpoint;
    const int32_t scene=(int32_t)WORD_AT(0x79094Cu);
    uint32_t mode=WORD_AT(0x78AD20u),count=WORD_AT(0x790950u);
    uint32_t contexts=WORD_AT(0x7B0C48u),bookmarks=WORD_AT(0x75F5D0u);
    if(scene<0 || scene>100 || (mode!=4u&&mode!=14u) || WORD_AT(0x7DE30Cu)!=7u ||
       (WORD_AT(0x76B0FCu)&&WORD_AT(0x76B108u)) ||
       !count || count>2u || !guest_writable(contexts,count*0x1584u) ||
       !guest_writable(bookmarks,count*0x238u)) {
        pc_developer_report("Checkpoint advance is available during offline Story gameplay.");return;
    }
    for(unsigned i=0;i<count;i++) {
        uint32_t context=contexts+i*0x1584u,body=WORD_AT(context+0x14u);
        if(!guest_writable(body,0x154u) || WORD_AT(body+0x150u)!=context ||
           !guest_writable(WORD_AT(body+0x7cu),0xea0u)) {
            pc_developer_report("Wait until the Story players have spawned.");return;
        }
    }
    uint32_t next=WORD_AT(bookmarks+4u)?WORD_AT(bookmarks)+1u:0u;
    if(next>32u) {pc_developer_report("Invalid native checkpoint index; no state changed.");return;}
    uint32_t target=next<32u?guest_call(sub_0006BB90,2,next,0u):0u;
    if(target && !guest_writable(target,0x20u)) {
        pc_developer_report("Checkpoint data is unavailable; no state changed.");return;
    }
    if(!target) {
        /* Same success flag and transition as the native script action at
         * US236609 and the completed-objective path at US228B14. */
        WORD_AT(0x7DE45Cu)|=0x8000u;
        guest_call(sub_00067450,1,0u);
        pc_developer_report("Starting the level ending.");
    } else {
        uint32_t previous_context=WORD_AT(0x7B0C7Cu);
        WORD_AT(0x7B0C7Cu)=contexts;
        guest_call(sub_0025EF80,1,next);
        WORD_AT(0x7B0C7Cu)=previous_context;
        if(!(WORD_AT(bookmarks+4u)&(1u<<next))) {
            pc_developer_report("The game did not capture the checkpoint; reload was cancelled.");return;
        }
        guest_call(sub_000653B0,1,WORD_AT(bookmarks+4u));
        pc_developer_report("Loading the next Story checkpoint.");
    }
    pc_developer_close();
}
static uint32_t developer_bits(float value) {uint32_t result;memcpy(&result,&value,4);return result;}
static void developer_vector(uint32_t address,float value[3],int write)
{
    if(write)memcpy((void *)((uintptr_t)address+g_xbox_mem_offset),value,12);
    else memcpy(value,(const void *)((uintptr_t)address+g_xbox_mem_offset),12);
}
void pc_developer_camera_guest(void (*original)(void))
{
    static ULONGLONG previous;
    static int previous_mode;
    PcDeveloperState state;pc_developer_get(&state);
    uint32_t context=WORD_AT(0x7B0C7Cu),camera=WORD_AT(0x7B0CC0u),entity=0;
    int mode=state.freecam?2:state.noclip?1:0;
    int valid=guest_writable(context,0x1584u) && WORD_AT(context+4u)==0u &&
        guest_writable(camera,0x3bcu) && (WORD_AT(0x7DE30Cu)==7u || WORD_AT(0x7DE30Cu)==1u) &&
        !(WORD_AT(0x7DE458u)&0x20000000u);
    if(valid) {
        entity=WORD_AT(context+0x14u);
        valid=guest_writable(entity,0x154u) && WORD_AT(entity+0x150u)==context;
    }
    ULONGLONG now=GetTickCount64();
    double elapsed=previous && now>=previous && (int32_t)WORD_AT(0x7497B4u)<=0?
        (double)(now-previous)*.001:0.;
    previous=now;
    float origin[3]={0},direction[3]={0,0,-1},position[3],forward[3];
    if(!mode || !valid) {
        if(previous_mode)pc_developer_flight_step(0,origin,direction,0,position,forward);
        previous_mode=0;original();return;
    }
    /* The ordinary player input update may be suspended by a scripted camera.
     * Consume raw flight look here too; already-consumed motion is zero. */
    pc_controls_apply_mouse(context,camera,g_xbox_mem_offset);
    float yaw,pitch;
    memcpy(&yaw,(const void *)((uintptr_t)(context+0x100u)+g_xbox_mem_offset),4);
    memcpy(&pitch,(const void *)((uintptr_t)(context+0x104u)+g_xbox_mem_offset),4);
    if(!isfinite(yaw)||!isfinite(pitch)){original();return;}
    yaw*=.01745329252f;pitch*=.01745329252f;
    direction[0]=sinf(yaw)*cosf(pitch);direction[1]=sinf(pitch);direction[2]=cosf(yaw)*cosf(pitch);
    int positioned=0;
    if(mode==1) {
        developer_vector(context+0xa8u,origin,0);
        if((positioned=pc_developer_flight_step(mode,origin,direction,elapsed,position,forward))) {
            developer_vector(context+0xa8u,position,1);
            WORD_AT(context+0x100u)=developer_bits(atan2f(forward[0],forward[2])*57.295779513f);
            WORD_AT(context+0x104u)=developer_bits(asinf(fmaxf(-1.f,fminf(1.f,forward[1])))*57.295779513f);
            guest_call(sub_000D2DE0,1,context);
            pc_network_bots_entity_refresh(entity);
        }
    }
    original();
    previous_mode=mode;
    if(WORD_AT(0x7B0CC0u)!=camera || WORD_AT(0x7B0C7Cu)!=context ||
       !guest_writable(camera,0x3bcu) || !guest_writable(context,0x1584u))return;
    developer_vector(context+0xb4u,origin,0);
    if(mode==2) {
        /* Always start next to the player, even when the original scripted
         * camera is elsewhere. The detached pose remains host-owned. */
        if(!pc_developer_flight_step(mode,origin,direction,elapsed,position,forward))return;
    } else {
        if(!positioned)return;
        /* Noclip owns its eye as well as its feet; scripted views must not
         * leave the player flying away from the camera. */
        memcpy(position,origin,sizeof position);
    }
    uint32_t projection=WORD_AT(camera+0x204u),view=WORD_AT(camera+0x208u);
    uint32_t combined=WORD_AT(camera+0x20cu),inverse=WORD_AT(camera+0x210u);
    if(!guest_writable(projection,64)||!guest_writable(view,64)||
       !guest_writable(combined,64)||!guest_writable(inverse,64))return;
    developer_vector(camera+0x27cu,position,1);
    developer_vector(camera+0x334u,forward,1);
    float up[3]={0,1,0};developer_vector(camera+0x364u,up,1);
    guest_call(sub_001519E0,10,view,
        developer_bits(position[0]),developer_bits(position[1]),developer_bits(position[2]),
        developer_bits(forward[0]),developer_bits(forward[1]),developer_bits(forward[2]),
        developer_bits(0),developer_bits(1),developer_bits(0));
    guest_call(sub_00153270,3,combined,view,projection);
    guest_call(sub_00152260,2,inverse,view);
}
