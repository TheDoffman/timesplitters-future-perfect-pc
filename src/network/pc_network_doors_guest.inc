/* Door callbacks still run through the original world update. Only their
 * animation numbers come from the host; no AI or host pointers run on guests. */
static uint32_t pc_network_door_entity(uint32_t control)
{
    const uint32_t id=WORD_AT(control),world=WORD_AT(0x72f5e8u),capacity=WORD_AT(0x7b4facu),slot=id>>22;
    if(!id || !world || capacity>16384u || slot>=capacity || slot>(UINT32_MAX-world)/0x2e8u)return 0;
    const uint32_t entity=world+slot*0x2e8u;
    return guest_writable(entity,0x2e8u) && WORD_AT(entity+0x20u)==4u &&
        WORD_AT(entity+0x2cu)==id && WORD_AT(entity+0x7cu)==control &&
        !(WORD_AT(entity+0x28u)&0x80000000u)?entity:0u;
}
void pc_network_doors_frame(void)
{
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
    static PcNetworkDoorState previous[PC_NETWORK_DOORS_MAX];
    static unsigned char seen[PC_NETWORK_DOORS_MAX];
    static uint32_t refreshed[PC_NETWORK_DOORS_MAX],last_tick,last_pool,last_table,last_match,last_role;
    static int have_tick;
    const unsigned role=WORD_AT(0x76b140u);
    if(pc_online_mode()!=PC_ONLINE_ARCADE || !WORD_AT(0x76b108u) ||
       WORD_AT(0x76b1a4u)!=15u || WORD_AT(0x7de30cu)!=7u ||
       (role!=1u && role!=2u)) {
        pc_network_doors_context(0,0,0);have_tick=0;last_role=0;memset(seen,0,sizeof seen);return;
    }
    uint32_t host_ip=0;
    if(role==2u) {
        const uint32_t host=WORD_AT(0x774040u);
        if(host>=16u) {pc_network_doors_context(0,0,0);return;}
        host_ip=WORD_AT(0x66648cu+host*16u);
    }
    const uint8_t *config=(const uint8_t *)((uintptr_t)0x7de400u+g_xbox_mem_offset);
    const uint32_t match=config[0x99]|((uint32_t)config[0x54]<<8)|
        ((uint32_t)config[0x55]<<16)|((uint32_t)config[0x56]<<24);
    pc_network_doors_context(role,host_ip,match);
    const uint32_t table=WORD_AT(0x732a04u),count=WORD_AT(0x732a08u),pool=WORD_AT(0x72f5e8u);
    if(!count || count>PC_NETWORK_DOORS_MAX || !guest_writable(table,count*0x98u))return;
    const uint32_t tick=WORD_AT(0x7de318u);
    if(role!=last_role || pool!=last_pool || table!=last_table || match!=last_match) {
        memset(seen,0,sizeof seen);have_tick=0;
        last_role=role;last_pool=pool;last_table=table;last_match=match;
    }
    PcNetworkDoorState states[PC_NETWORK_DOORS_MAX];unsigned n=0;
    if(role==1u) {
        uint32_t addresses[16];unsigned targets=0;
        for(unsigned i=0;i<16u;i++)if(WORD_AT(0x666040u+i*4u))
            addresses[targets++]=WORD_AT(0x66648cu+i*16u);
        pc_network_doors_targets(addresses,targets);
        if(have_tick && tick==last_tick)return;
        have_tick=1;last_tick=tick;
        for(unsigned i=0;i<count;i++) {
            const uint32_t control=table+i*0x98u;
            if((WORD_AT(control+0xcu)&0x1000000u) || !pc_network_door_entity(control))continue;
            PcNetworkDoorState s={0};s.slot=i;s.key=WORD_AT(control+4u);s.kind=WORD_AT(control+8u);
            s.phase=WORD_AT(control+0x10u);s.flags=WORD_AT(control+0xcu)&0x04000000u;
            s.timer=WORD_AT(control+0x20u);
            memcpy(&s.progress,(const void *)((uintptr_t)(control+0x24u)+g_xbox_mem_offset),4u);
            memcpy(&s.extra,(const void *)((uintptr_t)(control+0x28u)+g_xbox_mem_offset),4u);
            memcpy(&s.target,(const void *)((uintptr_t)(control+0x74u)+g_xbox_mem_offset),4u);
            memcpy(&s.velocity,(const void *)((uintptr_t)(control+0x38u)+g_xbox_mem_offset),4u);
            if(!pc_network_door_valid(&s))continue;
            /* Moving doors publish every tick; quiet rows refresh at least
             * every quarter-second so packet loss cannot retain an endpoint. */
            if(!seen[i] || memcmp(&s,&previous[i],sizeof s) || tick-refreshed[i]>=15u) {
                states[n++]=s;previous[i]=s;seen[i]=1;refreshed[i]=tick;
            }
        }
        pc_network_doors_send(tick,states,n);
    } else {
        n=pc_network_doors_receive(states,PC_NETWORK_DOORS_MAX);
        for(unsigned i=0;i<n;i++) {
            const PcNetworkDoorState *s=&states[i];
            if(!pc_network_door_valid(s) || s->slot>=count)continue;
            const uint32_t control=table+s->slot*0x98u;
            if(WORD_AT(control+4u)!=s->key || WORD_AT(control+8u)!=s->kind ||
               (WORD_AT(control+0xcu)&0x1000000u))continue;
            const uint32_t entity=pc_network_door_entity(control);if(!entity)continue;
            WORD_AT(control+0x10u)=s->phase;WORD_AT(control+0x20u)=s->timer;
            WORD_AT(control+0xcu)=(WORD_AT(control+0xcu)&~0x44000000u)|s->flags;
            memcpy((void *)((uintptr_t)(control+0x24u)+g_xbox_mem_offset),&s->progress,4u);
            memcpy((void *)((uintptr_t)(control+0x28u)+g_xbox_mem_offset),&s->extra,4u);
            memcpy((void *)((uintptr_t)(control+0x74u)+g_xbox_mem_offset),&s->target,4u);
            memcpy((void *)((uintptr_t)(control+0x38u)+g_xbox_mem_offset),&s->velocity,4u);
            WORD_AT(entity+0x18u)|=0x42u;
        }
    }
#endif
}
