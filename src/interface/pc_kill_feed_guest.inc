/* Supported US HUD pass. Observe confirmed score columns, never damage/AI. */
#include "pc_kill_feed.h"
#define PC_FEED_SETTINGS_IMPLEMENTATION
#include "pc_feed_settings.h"
extern void sub_0003FA10(void),sub_0003F8D0(void),sub_0003FCF0(void),sub_00041E30(void);
extern void sub_00021450(void);
static PcKillFeed kill_feed;
static uint32_t feed_scratch,feed_tick;
#include "pc_feed_attribution.h"
static PcFeedAttribution feed_attribution;
extern void sub_00080F20(void),sub_0001ED50(void),sub_0001F090(void);
static void feed_scene(void) {
    pc_feed_attribution_scene(&feed_attribution,WORD_AT(0x7356d8u),
        WORD_AT(0x79094cu));
}
/* Invalidate recycled projectile storage before any constructor initializes it. */
void pc_kill_feed_allocate(void (*original)(void)) {
    original();
    if(g_eax) {feed_scene();pc_feed_shot(&feed_attribution,g_eax,0,0,0);}
}
static uint32_t feed_firing_owner;
static unsigned feed_firing_icon;
static unsigned feed_weapon_icon(uint32_t state) {
    if(!guest_writable(state,0x34u))return 0;
    uint32_t definition=WORD_AT(state+0x30u);
    for(unsigned w=1;w<69u;w++)if(WORD_AT(0x4fa138u+60u*w)==definition) {
        uint32_t icon=WORD_AT(0x4f9bb0u+20u*w);
        if(icon>=29u && icon<=140u)return icon;
    }
    return 0;
}
/* Synchronous hitscan deaths occur inside these audited firing transactions.
   Nested calls restore the prior owner, and delayed projectiles use the cache. */
void pc_kill_feed_fire(void (*original)(void),int npc) {
    uint32_t owner=WORD_AT(g_esp+4u),state=WORD_AT(g_esp+8u);
    if(npc) {
        uint32_t actor=guest_writable(owner,0x80u)?WORD_AT(owner+0x7cu):0;
        state=actor && state<=1u?actor+0xa4u+state*0x130u:0;
    }
    uint32_t saved_owner=feed_firing_owner;unsigned saved_icon=feed_firing_icon;
    feed_firing_owner=owner;feed_firing_icon=feed_weapon_icon(state);
    original();
    feed_firing_owner=saved_owner;feed_firing_icon=saved_icon;
}
/* EAAF0 is the shared weapon projectile constructor. Argument four is the
   firing weapon state; +30 selects its immutable weapon definition. */
void pc_kill_feed_projectile(void (*original)(void)) {
    uint32_t state=WORD_AT(g_esp+16u);unsigned icon=0;
    if(guest_writable(state,0x34u)) {
        uint32_t definition=WORD_AT(state+0x30u);
        for(unsigned w=1;w<69u;w++) if(WORD_AT(0x4fa138u+60u*w)==definition) {
            uint32_t candidate=WORD_AT(0x4f9bb0u+20u*w);
            if(candidate>=29u && candidate<=140u) {icon=candidate;break;}
        }
    }
    original();
    uint32_t shot=g_eax;
    if(guest_writable(shot,0x110u)) {
        feed_scene();pc_feed_shot(&feed_attribution,shot,WORD_AT(shot+0x10cu),WORD_AT(shot),icon);
    }
}
/* The original copies projectile ownership and kind into a hit record. */
void pc_kill_feed_hit_record(void (*original)(void)) {
    uint32_t hit=WORD_AT(g_esp+4u),shot=WORD_AT(g_esp+8u);
    original();
    if(guest_writable(hit,0x28u) && guest_writable(shot,0x110u)) {
        feed_scene();pc_feed_hit(&feed_attribution,hit,shot,WORD_AT(hit+4u),WORD_AT(hit+0x14u),WORD_AT(0x7de318u));
    }
}
static unsigned feed_actor_slot(uint32_t entity) {
    if(!guest_writable(entity,0x80u))return 16u;
    uint32_t actor=WORD_AT(entity+0x7cu),pool=WORD_AT(0x7356d8u);
    if(!pool || actor<pool || (actor-pool)%0xea0u)return 16u;
    unsigned slot=(actor-pool)/0xea0u;
    return slot<16u && guest_writable(actor,0xea0u) && WORD_AT(actor)==entity?slot:16u;
}
void pc_kill_feed_death(void (*original)(void)) {
    uint32_t hit=WORD_AT(g_esp+4u),pool=WORD_AT(0x7356d8u);
    unsigned killer=16,victim=16,icon=0;uint16_t before=0;
    if(guest_writable(hit,0x28u)) {
        victim=feed_actor_slot(WORD_AT(hit));killer=feed_actor_slot(WORD_AT(hit+4u));
        if(killer<16u && victim<16u) {
            feed_scene();
            icon=pc_feed_hit_icon(&feed_attribution,hit,WORD_AT(hit+4u),WORD_AT(hit+0x14u),WORD_AT(0x7de318u));
            if(!icon && feed_firing_owner==WORD_AT(hit+4u))icon=feed_firing_icon;
            memcpy(&before,(void *)((uintptr_t)(pool+killer*0xea0u+0xa10u+victim*2u)+g_xbox_mem_offset),2);
        }
    }
    original(); /* Keep original authorization, scoring and Story departure rules. */
    if(killer<16u && victim<16u && pool==WORD_AT(0x7356d8u)) {
        uint16_t after;memcpy(&after,(void *)((uintptr_t)(pool+killer*0xea0u+0xa10u+victim*2u)+g_xbox_mem_offset),2);
        if((uint16_t)(after-before)==1u) {
            feed_attribution.counters[killer][victim]=after;
            feed_attribution.icons[killer][victim]=(uint16_t)icon;
        }
    }
}
static void feed_snapshot(PcFeedSnapshot *s,uint32_t pool,unsigned count) {
    feed_scene();memset(s,0,sizeof(*s));s->pool=pool;s->count=count;
    s->match=(*(uint8_t *)((uintptr_t)0x007DE499u+g_xbox_mem_offset)<<24) ^ WORD_AT(0x0079094Cu);
    for(unsigned i=0;i<count;i++) {
        uint32_t actor=pool+i*0xea0u,name=WORD_AT(actor+0x70u);
        char raw[PC_FEED_NAME]={0};
        /* The native crown-owner announcement (67CD0) uses actor+70 as %s. */
        for(unsigned j=0;j<sizeof(raw)-1 && guest_writable(name+j,1);j++) {
            raw[j]=*(char *)((uintptr_t)(name+j)+g_xbox_mem_offset);
            if(!raw[j])break;
        }
        if(!raw[0])snprintf(raw,sizeof raw,"Player %u",i+1);
        pc_feed_name(s->names[i],raw);s->teams[i]=WORD_AT(actor+0x84u);
        memcpy(s->kills[i],(void *)((uintptr_t)(actor+0xa10u)+g_xbox_mem_offset),sizeof s->kills[i]);
        for(unsigned v=0;v<count;v++)if(s->kills[i][v]==feed_attribution.counters[i][v])
            s->weapons[i][v]=feed_attribution.icons[i][v];
    }
}
static float feed_measure(const char *name,float *height) {
    snprintf((char *)((uintptr_t)feed_scratch+g_xbox_mem_offset),192,"%s",name);
    guest_call(sub_0003FCF0,4,feed_scratch+256u,feed_scratch+260u,feed_scratch,0u);
    float width;
    memcpy(&width,(void *)((uintptr_t)(feed_scratch+256u)+g_xbox_mem_offset),4);
    memcpy(height,(void *)((uintptr_t)(feed_scratch+260u)+g_xbox_mem_offset),4);
    return width;
}
static float feed_fit(char *name,float limit,float *height) {
    float width=feed_measure(name,height);size_t len=strlen(name);
    while(width>limit && len>3) {
        name[--len]=0;name[len-1]='.';name[len-2]='.';name[len-3]='.';
        width=feed_measure(name,height);
    }
    return width;
}
static void feed_text(const char *name,float x,float y,unsigned color,unsigned alpha) {
    snprintf((char *)((uintptr_t)feed_scratch+g_xbox_mem_offset),192,"%s",name);
    guest_call(sub_00041E30,5,bits(x),bits(y),feed_scratch,color|alpha,alpha);
    guest_call(sub_00022FF0,0);
}
void pc_kill_feed_hud(void (*original)(void)) {
    original();
    GuestRegisters registers;save_registers(&registers);
    uint32_t pool=WORD_AT(0x007356D8u),tick=WORD_AT(0x007DE318u);
    unsigned count=WORD_AT(0x00790950u)+WORD_AT(0x007A2958u);
    unsigned mode=(WORD_AT(0x007DE454u)>>8)&255u;
    /* Arcade Custom/League and System Link use mode9. Story uses mode10. */
    if(!pc_feed_enabled() || mode!=9u || WORD_AT(0x007DE30Cu)!=7u || !count || count>16 ||
       !guest_writable(pool,count*0xea0u)) {
        pc_feed_clear(&kill_feed);restore_registers(&registers);return;
    }
    if(tick<feed_tick)pc_feed_clear(&kill_feed);
    feed_tick=tick;
    uint64_t now=GetTickCount64();PcFeedSnapshot snapshot;
    feed_snapshot(&snapshot,pool,count);pc_feed_sample(&kill_feed,&snapshot,now,1);
    uint32_t view=WORD_AT(0x007E71C8u);
    if(!kill_feed.count || !guest_writable(view,0x24)) {restore_registers(&registers);return;}
    if(!feed_scratch)feed_scratch=xbox_HeapAlloc(512,4);
    if(!feed_scratch) {restore_registers(&registers);return;}
    int old=ui_placement;uint32_t old_tag=ui_tag;
    unsigned views=guest_call(sub_001CDAE0,0);
    ui_place(views==1?4:2,0x0014F0B0u);
    uint32_t font=guest_call(sub_0003FA10,1,views==1?11u:9u);
    uint32_t sx=WORD_AT(0x007E7270u),sy=WORD_AT(0x007E7280u);
    float xscale,yscale;memcpy(&xscale,&sx,4);memcpy(&yscale,&sy,4);
    /* Compact authored HUD units, not host pixels. RIGHT/CENTRE placement
       applies the renderer's uniform resolution fit, so 4K grows naturally
       without ultrawide stretching or a second resolution multiplier. */
    WORD_AT(0x007E7270u)=bits(xscale*.40f);WORD_AT(0x007E7280u)=bits(yscale*.40f);
    guest_call(sub_0003F8D0,0);
    guest_call(sub_00021450,2,32u,0u);
    float left=(float)(int32_t)WORD_AT(view),right=(float)(int32_t)WORD_AT(view+4u)+1.f;
    float top=(float)(int32_t)WORD_AT(view+0x14u),bottom=(float)(int32_t)WORD_AT(view+0x18u)+1.f;
    float y=top+(bottom-top)*.23f;
    uint32_t context=WORD_AT(0x7b0c7cu);
    unsigned viewer=guest_writable(context,0x18u)?feed_actor_slot(WORD_AT(context+0x14u)):16u;
    unsigned team=viewer<count?snapshot.teams[viewer]:0;
    int teams=(WORD_AT(0x7de458u)&1u)!=0;
    for(unsigned i=0;i<kill_feed.count;i++) {
        PcFeedRow *row=&kill_feed.rows[i];unsigned alpha=pc_feed_alpha(row,now);
        char killer[PC_FEED_NAME],victim[PC_FEED_NAME];
        snprintf(killer,sizeof killer,"%s",row->killer);
        snprintf(victim,sizeof victim,"%s",row->victim);
        float height=0,other=0,space=24.f;
        float limit=(right-left-24.f-space)*.5f;
        float kw=feed_fit(killer,limit,&height),vw=feed_fit(victim,limit,&other);
        if(other>height)height=other;
        float row_height=height*1.25f;
        float text_y=y+(row_height-height)*.5f;
        if(y+row_height>=bottom-12.f)break;
        float x=right-12.f-kw-space-vw;
        feed_text(killer,x,text_y,pc_feed_color(row->killer_id,row->killer_team,viewer,team,teams),alpha);
        unsigned texture=row->weapon?guest_call(sub_00080F20,1,row->weapon):UINT32_MAX;
        uint32_t table=WORD_AT(0x4b85c8u),descriptors=guest_writable(table,4)?WORD_AT(table):0;
        uint32_t descriptor=texture<4096u?descriptors+texture*0x34u:0;
        uint16_t iw=0,ih=0;
        if(descriptors && guest_writable(descriptor,0x34u)) {
            memcpy(&iw,(void *)((uintptr_t)(descriptor+0x26u)+g_xbox_mem_offset),2);
            memcpy(&ih,(void *)((uintptr_t)(descriptor+0x2au)+g_xbox_mem_offset),2);
        }
        if(iw && ih) {
            float scale=(space-4.f)/(float)iw;
            if(scale*(float)ih>row_height)scale=row_height/(float)ih;
            float w=(float)iw*scale,h=(float)ih*scale;
            int ix=(int)(x+kw+(space-w)*.5f+.5f),iy=(int)(y+(row_height-h)*.5f+.5f);
            guest_call(sub_0001ED50,1,texture);
            guest_call(sub_0001F090,9,(uint32_t)ix,(uint32_t)iy,(uint32_t)(ix+(int)(w+.5f)),
                /* Native inventory silhouettes face left. Reverse U only so
                   the barrel points right, toward the victim; preserve aspect. */
                (uint32_t)(iy+(int)(h+.5f)),(uint32_t)iw<<4,0u,0u,(uint32_t)ih<<4,0x7f7f7f00u|alpha);
            guest_call(sub_00022FF0,0);
        } else {
            const char *separator=row->killer_id==row->victim_id?"x":">";
            float separator_height=0,separator_width=feed_measure(separator,&separator_height);
            feed_text(separator,x+kw+(space-separator_width)*.5f,
                y+(row_height-separator_height)*.5f,0x707f7f00u,alpha);
        }
        feed_text(victim,x+kw+space,text_y,pc_feed_color(row->victim_id,row->victim_team,viewer,team,teams),alpha);
        y+=row_height>6.f?row_height+2.f:8.f;
    }
    WORD_AT(0x007E7270u)=sx;WORD_AT(0x007E7280u)=sy;
    guest_call(sub_0003F8D0,0);guest_call(sub_0003FA10,1,font);
    guest_call(sub_00021450,2,32u,1u);ui_place(old,old_tag);
    restore_registers(&registers);
}
