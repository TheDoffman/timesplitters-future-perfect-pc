/* Reserve ammo is signed16. The stock pickup narrows the sum before clamping,
   and can also write a negative reserve when loaded clips exceed the limit. */
void pc_network_ammo_pickup(void (*original)(void))
{
#ifdef _WIN32
    extern void sub_001B67E0(void);
    const uint32_t frame=g_esp;
    if(WORD_AT(0x0076b108u) && guest_writable(frame,16u)) {
        const uint32_t actor=WORD_AT(frame+4u),kind=WORD_AT(frame+8u);
        const int32_t amount=(int32_t)WORD_AT(frame+12u);
        if(kind<32u && amount>0 && guest_writable(actor,0xea0u)) {
            const uint32_t entry=0x004ea3f8u+kind*0xe0u;
            int32_t limit=(int32_t)WORD_AT(entry);
            if(limit>=0 && limit<=32767) {
                if(WORD_AT(entry+8u)&1u) {
                    float factor;memcpy(&factor,(const void *)((uintptr_t)0x00478394u+g_xbox_mem_offset),4u);
                    float scaled=(float)limit*(float)(int32_t)WORD_AT(0x004e8cecu);scaled*=factor;
                    if(!isfinite(scaled) || scaled<0.f || scaled>32767.f) {original();return;}
                    limit=(int32_t)scaled;
                }
                GuestRegisters saved;save_registers(&saved);
                const int32_t loaded=(int32_t)guest_call(sub_001B67E0,2,actor,kind);
                restore_registers(&saved);
                if(loaded<0 || loaded>65534) {original();return;}
                const int32_t cap=limit>loaded?limit-loaded:0;
                int16_t before;memcpy(&before,(const void *)((uintptr_t)(actor+0x61eu+kind*2u)+g_xbox_mem_offset),2u);
                const int32_t reserve=before>0?before:0;
                const int64_t sum=(int64_t)reserve+amount;
                /* A pickup must never drain an existing valid reserve, even
                   if the current clips already exceed this kind's total cap. */
                const int32_t ceiling=cap>reserve?cap:reserve;
                const int16_t after=(int16_t)(sum>ceiling?ceiling:sum);
                memcpy((void *)((uintptr_t)(actor+0x61eu+kind*2u)+g_xbox_mem_offset),&after,2u);
                g_eax=after>reserve?(uint32_t)(after-reserve):0u;g_esp=frame+4u;return;
            }
        }
    }
#endif
    original();
}
