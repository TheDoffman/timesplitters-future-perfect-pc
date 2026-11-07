/* Native host match Options selector, appended inside the original row builder
 * before its outer menu finalizes layout. The host preference is session state,
 * never a guest packet padding field or a personal settings toggle. */
#define DEVELOPER_MATCH_CHANGE 0x12390u
static int developer_match_tools;
static uint32_t developer_match_row,developer_match_owner,developer_match_descriptor,developer_match_labels;
static int developer_match_host(void) {
    unsigned mode=WORD_AT(0x78AD2Cu);
    return WORD_AT(0x76B108u)&&WORD_AT(0x76B140u)==1u&&mode>=11u&&mode<=13u;
}
static void developer_match_change(void) {
    GuestRegisters r;save_registers(&r);
    if(developer_match_host()&&guest_writable(developer_match_row,0xccu)&&
       guest_writable(developer_match_owner,8u)&&WORD_AT(developer_match_owner+4u)==developer_match_descriptor&&WORD_AT(developer_match_row+0xc8u)<=1u) {
        developer_match_tools=(int)WORD_AT(developer_match_row+0xc8u);
        fprintf(stderr,"[DEVELOPER-MATCH] host Tools %s\n",developer_match_tools?"On":"Off");
        pc_developer_lobby_guest();
    }
    restore_registers(&r);g_esp+=4;
}
static void developer_match_add(uint32_t menu) {
    extern void sub_00071CF0(void);
    if(!developer_match_host()||!guest_writable(menu,8u))return;
    if(!developer_match_labels) {
        static const char labels[]="Developer Tools\0Off\0On";
        developer_match_labels=xbox_HeapAlloc(sizeof labels,4);
        if(developer_match_labels)memcpy((void *)((uintptr_t)developer_match_labels+g_xbox_mem_offset),labels,sizeof labels);
    }
    if(!developer_match_labels)return;
    developer_match_owner=menu;developer_match_descriptor=WORD_AT(menu+4u);
    developer_match_row=guest_call(sub_00079D60_gen,6,developer_match_labels,121u,0u,0u,0u,0u);
    guest_call(sub_0007D480,2,0u,0u);
    guest_call(sub_000717D0,1,developer_match_labels+sizeof("Developer Tools"));
    guest_call(sub_000717D0,1,developer_match_labels+sizeof("Developer Tools")+sizeof("Off"));
    guest_call(sub_00071CF0,2,developer_match_row,DEVELOPER_MATCH_CHANGE);
    guest_call(sub_00075430,2,developer_match_row,(uint32_t)developer_match_tools);
    fprintf(stderr,"[DEVELOPER-MATCH] Options menu=%08x row=%08x\n",menu,developer_match_row);
}
