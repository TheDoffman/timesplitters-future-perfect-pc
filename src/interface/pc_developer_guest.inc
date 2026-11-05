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
void pc_developer_guest_frame(void)
{
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
    developer_vector(camera+0x334u,direction,0);
    int positioned=0;
    if(mode==1) {
        developer_vector(context+0xa8u,origin,0);
        if((positioned=pc_developer_flight_step(mode,origin,direction,elapsed,position,forward))) {
            developer_vector(context+0xa8u,position,1);
            WORD_AT(context+0x100u)=developer_bits(atan2f(-forward[0],-forward[2])*57.295779513f);
            WORD_AT(context+0x104u)=developer_bits(-asinf(fmaxf(-1.f,fminf(1.f,forward[1])))*57.295779513f);
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
        developer_vector(camera+0x334u,direction,0);
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
