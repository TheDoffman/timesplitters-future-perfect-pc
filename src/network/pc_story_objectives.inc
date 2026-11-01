/* Replicate objective status, never the mission VM's callbacks or pointers.
   Record kinds0/1 are objectives; kind2 records are executable Story scripts. */
#ifdef _WIN32
extern void sub_00227430(void);
typedef struct StoryObjective {
    uint32_t magic,match,serial,index,identity,kind,state,flags;
} StoryObjective;
#define STORY_OBJECTIVE_MAGIC 0x314a4f53u
#define STORY_OBJECTIVE_RECORDS 4096u
#define STORY_OBJECTIVE_LIMIT 64u
static StoryObjective story_objectives_pending[STORY_OBJECTIVE_LIMIT];
static uint32_t story_objectives_seen[STORY_OBJECTIVE_LIMIT];
static unsigned char story_objectives_popup[STORY_OBJECTIVE_LIMIT];
static uint32_t story_objectives_match,story_objectives_tick,story_objectives_serial;
static int story_objectives_published;
static void story_objectives_reset(void)
{
    memset(story_objectives_pending,0,sizeof story_objectives_pending);
    memset(story_objectives_seen,0,sizeof story_objectives_seen);
    memset(story_objectives_popup,0,sizeof story_objectives_popup);
    story_objectives_match=0;story_objectives_serial=0;story_objectives_published=0;
}
static int story_objectives_session(void)
{ return story_npc_session() && WORD_AT(0x0079094cu)==2u && WORD_AT(0x0076b1a4u)==15u; }
static unsigned story_objectives_records(uint32_t *table)
{
    const unsigned count=WORD_AT(0x0078cec8u);
    *table=WORD_AT(0x0078ceccu);
    return count && count<=STORY_OBJECTIVE_RECORDS && *table &&
        guest_writable(*table,count*0x4cu)?count:0u;
}
static int story_objectives_receive(uint32_t peer,uint32_t payload,uint32_t type,uint32_t length)
{
    if(type!=0x1eu || length<sizeof(StoryObjective) || length>sizeof(StoryObjective)+4u ||
       !guest_writable(payload,sizeof(StoryObjective)) || WORD_AT(payload)!=STORY_OBJECTIVE_MAGIC) return 0;
    StoryObjective s;memcpy(&s,(const void *)((uintptr_t)payload+g_xbox_mem_offset),sizeof s);
    if(WORD_AT(0x0076b140u)!=2u || !guest_writable(peer,4u) ||
       WORD_AT(peer)!=WORD_AT(0x00774040u) || !s.serial || s.index>=STORY_OBJECTIVE_RECORDS ||
       s.identity>65535u || s.kind>1u || s.state<1u || s.state>5u || s.flags>255u ||
       s.match!=story_npc_match()) return 1;
    unsigned slot=STORY_OBJECTIVE_LIMIT;
    for(unsigned i=0;i<STORY_OBJECTIVE_LIMIT;i++) {
        StoryObjective *p=&story_objectives_pending[i];
        if(p->serial && p->match==s.match && p->index==s.index) {slot=i;break;}
        if(!p->serial && slot==STORY_OBJECTIVE_LIMIT) slot=i;
    }
    if(slot==STORY_OBJECTIVE_LIMIT) return 1;
    StoryObjective *p=&story_objectives_pending[slot];
    if(p->serial && p->match==s.match && (int32_t)(s.serial-p->serial)<=0) return 1;
    *p=s;
    return 1;
}
static void story_objectives_frame(void)
{
    if(!story_objectives_session()) return;
    uint32_t table;const unsigned count=story_objectives_records(&table);
    if(!count) return;
    const uint32_t match=story_npc_match(),role=WORD_AT(0x0076b140u);
    if(role==1u) {
        const uint32_t tick=WORD_AT(0x007de318u),frame=g_esp,node=WORD_AT(0x00774028u);
        if(story_objectives_match!=match) {
            memset(story_objectives_seen,0,sizeof story_objectives_seen);
            story_objectives_match=match;story_objectives_published=0;
        }
        const int refresh=!story_objectives_published || (uint32_t)(tick-story_objectives_tick)>=60u;
        if(frame<2048u || !guest_writable(frame-2048u,2048u) || !guest_writable(node,0x35u)) return;
        unsigned slot=0;
        GuestRegisters saved;save_registers(&saved);
        for(unsigned i=0;i<count && slot<STORY_OBJECTIVE_LIMIT;i++) {
            const uint8_t *r=(const uint8_t *)((uintptr_t)(table+i*0x4cu)+g_xbox_mem_offset);
            if(r[8]>1u || r[11]<1u || r[11]>5u) continue;
            const uint32_t status=r[8]|((uint32_t)r[10]<<8)|((uint32_t)r[11]<<16);
            if(refresh || story_objectives_seen[slot]!=status) {
                uint16_t id;memcpy(&id,r,sizeof id);
                if(!++story_objectives_serial) ++story_objectives_serial;
                const StoryObjective s={STORY_OBJECTIVE_MAGIC,match,story_objectives_serial,i,id,r[8],r[11],r[10]};
                const uint32_t buffer=frame-512u;WORD_AT(buffer)=0;
                memcpy((void *)((uintptr_t)(buffer+4u)+g_xbox_mem_offset),&s,sizeof s);
                g_esp=frame-1024u;
                guest_call(sub_00353A90,5,node,0x1eu,buffer,(uint32_t)sizeof s+4u,1u);
                story_objectives_seen[slot]=status;
            }
            ++slot;
        }
        restore_registers(&saved);
        if(refresh) {story_objectives_tick=tick;story_objectives_published=1;}
        return;
    }
    if(role!=2u) return;
    GuestRegisters saved;save_registers(&saved);
    for(unsigned i=0;i<STORY_OBJECTIVE_LIMIT;i++) {
        const StoryObjective *s=&story_objectives_pending[i];
        if(!s->serial || s->match!=match || s->index>=count) continue;
        uint8_t *r=(uint8_t *)((uintptr_t)(table+s->index*0x4cu)+g_xbox_mem_offset);
        uint16_t id;memcpy(&id,r,sizeof id);
        if(id!=s->identity || r[8]!=s->kind) continue;
        if(r[11]!=s->state || r[10]!=s->flags) {
            if(r[11]==1u && s->state!=1u) ++WORD_AT(0x0075c134u+s->kind*4u);
            else if(r[11]!=1u && s->state==1u && WORD_AT(0x0075c134u+s->kind*4u))
                --WORD_AT(0x0075c134u+s->kind*4u);
            r[10]=(uint8_t)s->flags;r[11]=(uint8_t)s->state;
            story_objectives_popup[i]=(unsigned char)(s->state>=2u);
        }
        /* A cinematic can temporarily refuse HUD messages. Keep this notification
           pending until the owning local context has control again. */
        const uint32_t contexts=WORD_AT(0x007b0c48u);
        int ready=0;
        if(contexts && guest_writable(contexts,story_npc_humans()*0x1584u))
            for(unsigned j=0;j<story_npc_humans();j++)
                if(WORD_AT(contexts+j*0x1584u+4u)==0u && WORD_AT(contexts+j*0x1584u+0x114u)==2u) ready=1;
        if(story_objectives_popup[i] && ready) {
            guest_call(sub_00227430,2,table+s->index*0x4cu,0u);
            story_objectives_popup[i]=0;
        }
    }
    restore_registers(&saved);
}
#endif
