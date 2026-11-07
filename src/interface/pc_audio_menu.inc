/* Shared guest-native audio subpages. Original profile rows/save callbacks stay
 * in the original Audio Options menu; these independent sidecars save live. */
#include "interface/pc_audio_mix_settings.h"
#define AUDIO_BETA_CHANGE 0x00012027u
#define AUDIO_MIX_OPEN 0x00012028u
#define AUDIO_MIX_BUILD 0x00012029u
#define AUDIO_MIX_CHANGE 0x0001202au
#define AUDIO_MIX_BACK 0x0001202bu
#define AUDIO_FEATURES_OPEN 0x0001202cu
#define AUDIO_MIX_TICK 0x0001202du
#define AUDIO_LISTENING_OPEN 0x0001202eu
#define AUDIO_LISTENING_CHANGE 0x0001202fu
static uint32_t bits(float f);
static uint32_t audio_menu_storage,audio_menu,audio_parent,audio_previous;
static uint32_t audio_feature_rows[5],audio_mix_rows[6],audio_beta_labels;
static uint32_t audio_focus_row,audio_range_row;
static int audio_page; /* 0 mix, 1 enhancements, 2 listening */
#define AUDIO_TEXT(i) (audio_menu_storage+32u+(i)*128u)
#define AUDIO_PERCENT(i) (audio_menu_storage+32u+18u*128u+(i)*8u)
static int audio_menu_prepare(void)
{
    if(!audio_menu_storage) {
        audio_menu_storage=xbox_HeapAlloc(32+18*128+101*8,4);
        if(!audio_menu_storage)return 0;
        memset((void *)((uintptr_t)audio_menu_storage+g_xbox_mem_offset),0,32+18*128+101*8);
        WORD_AT(audio_menu_storage)=AUDIO_MIX_BUILD;
        WORD_AT(audio_menu_storage+4)=AUDIO_MIX_TICK;
        static const char *const labels[]={"Audio Mix","Enhanced Audio","Master","Effects","Music",
            "Dialogue","Ambience","Movies","Enhancements","Back","Listening options",
            "Dialogue focus","Impact range","Original","Reduced","Night","Off","On"};
        for(unsigned i=0;i<18;i++)
            snprintf((char *)((uintptr_t)AUDIO_TEXT(i)+g_xbox_mem_offset),128,"\vs75%s\vs100",labels[i]);
        for(unsigned i=0;i<=100;i++)
            snprintf((char *)((uintptr_t)AUDIO_PERCENT(i)+g_xbox_mem_offset),8,"%u%%",i);
    }
    return 1;
}
static void audio_beta_add(void (*original)(void))
{
    if(!audio_menu_prepare())return;
    /* One compact link replaces the former five appended rows. */
    guest_call(original,6,AUDIO_TEXT(0),15u,WORD_AT(g_esp+12)&~0x21u,bits(32.f),AUDIO_MIX_OPEN,0u);
}
static void audio_feature_add(void)
{
    extern void sub_00071CF0(void);
    if(!audio_beta_labels) {
        audio_beta_labels=xbox_HeapAlloc(5u*128u,4);
        if(audio_beta_labels) {
            static const char *const captions[]={PC_AUDIO_BETA_LABEL,"Stereo positioning",
                "Extended gunfire range","Environmental filtering","Background ambience"};
            for(unsigned i=0;i<5;i++) {
                char *slot=(char *)((uintptr_t)(audio_beta_labels+i*128u)+g_xbox_mem_offset);
                memset(slot,0,128);snprintf(slot,112,"\vs75%s\vs100",captions[i]);
                memcpy(slot+112,"Off\0On",7);
            }
        }
    }
    if(!audio_beta_labels)return;
    PcAudioOptions options=pc_audio_get_options();
    int values[]={options.enhanced,options.spatial,options.weapon_range,options.environment,options.ambience};
    for(unsigned i=0;i<5;i++) {
        uint32_t text=audio_beta_labels+i*128u;
        uint32_t item=guest_call(sub_00079D60_gen,6,text,15u+i,0u,bits(32.f),0u,0u);
        audio_feature_rows[i]=item;
        if(!item)continue;
        if(guest_writable(item,0x2c))WORD_AT(item+0x28)&=~0x2000u;
        guest_call(sub_0007D480,2,0xA000u,0u);
        guest_call(sub_000717D0,1,text+112u);guest_call(sub_000717D0,1,text+116u);
        guest_call(sub_00071CF0,2,item,AUDIO_BETA_CHANGE);
        guest_call(sub_00075430,2,item,(uint32_t)values[i]);
    }
}
static void audio_beta_change(void)
{
    GuestRegisters r;save_registers(&r);
    PcAudioOptions options=pc_audio_get_options(),previous=options;
    int *values[]={&options.enhanced,&options.spatial,&options.weapon_range,&options.environment,&options.ambience};
    for(unsigned i=0;i<5;i++) {
        uint32_t item=audio_feature_rows[i];
        if(item && guest_writable(item,0xcc) && WORD_AT(item+0xc8)<2)*values[i]=(int)WORD_AT(item+0xc8);
    }
    if(!pc_audio_set_options(&options)) {
        int old[]={previous.enhanced,previous.spatial,previous.weapon_range,previous.environment,previous.ambience};
        for(unsigned i=0;i<5;i++)if(audio_feature_rows[i] && guest_writable(audio_feature_rows[i],0xcc))
            guest_call(sub_00075430,2,audio_feature_rows[i],(uint32_t)old[i]);
        fprintf(stderr,"[AUDIO] Unable to save audio preferences\n");
    }
    restore_registers(&r);g_esp+=4;
}
static void audio_mix_change(void)
{
    GuestRegisters r;save_registers(&r);
    PcAudioMixOptions options=pc_audio_mix_get(),previous=options;
    for(unsigned i=0;i<6;i++) {
        uint32_t item=audio_mix_rows[i];
        if(item && guest_writable(item,0xcc) && WORD_AT(item+0xc8)<=100) {
            if(!i)options.master=(int)WORD_AT(item+0xc8);
            else options.volume[i-1]=(int)WORD_AT(item+0xc8);
        }
    }
    if(!pc_audio_mix_set(&options)) {
        for(unsigned i=0;i<6;i++)if(audio_mix_rows[i] && guest_writable(audio_mix_rows[i],0xcc))
            guest_call(sub_00075430,2,audio_mix_rows[i],(uint32_t)(i?previous.volume[i-1]:previous.master));
        fprintf(stderr,"[AUDIO] Unable to save audio mix\n");
    }
    restore_registers(&r);g_esp+=4;
}
static void audio_listening_change(void)
{
    GuestRegisters r;save_registers(&r);
    PcAudioMixOptions options=pc_audio_mix_get(),previous=options;
    if(audio_focus_row && guest_writable(audio_focus_row,0xcc) && WORD_AT(audio_focus_row+0xc8)<2)
        options.ducking=(int)WORD_AT(audio_focus_row+0xc8);
    if(audio_range_row && guest_writable(audio_range_row,0xcc) && WORD_AT(audio_range_row+0xc8)<3)
        options.dynamics=(int)WORD_AT(audio_range_row+0xc8);
    if(!pc_audio_mix_set(&options)) {
        if(audio_focus_row)guest_call(sub_00075430,2,audio_focus_row,(uint32_t)previous.ducking);
        if(audio_range_row)guest_call(sub_00075430,2,audio_range_row,(uint32_t)previous.dynamics);
        fprintf(stderr,"[AUDIO] Unable to save listening preferences\n");
    }
    restore_registers(&r);g_esp+=4;
}
static void audio_listening_add(void)
{
    extern void sub_00071CF0(void);
    PcAudioMixOptions options=pc_audio_mix_get();
    audio_focus_row=guest_call(sub_00079D60_gen,6,AUDIO_TEXT(11),39u,0u,bits(32.f),0u,0u);
    if(audio_focus_row) {
        if(guest_writable(audio_focus_row,0x2c))WORD_AT(audio_focus_row+0x28)&=~0x2000u;
        guest_call(sub_0007D480,2,0xA000u,0u);
        guest_call(sub_000717D0,1,AUDIO_TEXT(16));guest_call(sub_000717D0,1,AUDIO_TEXT(17));
        guest_call(sub_00071CF0,2,audio_focus_row,AUDIO_LISTENING_CHANGE);
        guest_call(sub_00075430,2,audio_focus_row,(uint32_t)options.ducking);
    }
    audio_range_row=guest_call(sub_00079D60_gen,6,AUDIO_TEXT(12),40u,0u,bits(32.f),0u,0u);
    if(audio_range_row) {
        if(guest_writable(audio_range_row,0x2c))WORD_AT(audio_range_row+0x28)&=~0x2000u;
        guest_call(sub_0007D480,2,0xA000u,0u);
        for(unsigned i=13;i<16;i++)guest_call(sub_000717D0,1,AUDIO_TEXT(i));
        guest_call(sub_00071CF0,2,audio_range_row,AUDIO_LISTENING_CHANGE);
        guest_call(sub_00075430,2,audio_range_row,(uint32_t)options.dynamics);
    }
}
static void audio_mix_build(void)
{
    extern void sub_00071CF0(void);
    GuestRegisters r;uint32_t menu=WORD_AT(g_esp+4);save_registers(&r);audio_menu=menu;
    memset(audio_mix_rows,0,sizeof audio_mix_rows);memset(audio_feature_rows,0,sizeof audio_feature_rows);
    audio_focus_row=audio_range_row=0;
    guest_call(sub_00075B70,5,0xa000u,AUDIO_TEXT(audio_page==2?10:audio_page),bits(65.f),0u,AUDIO_MIX_BACK);
    guest_call(sub_00071B10,4,1u,bits(1.1f),1u,1u);
    guest_call(sub_00071C50,1,0x26200u);
    if(audio_page==2)audio_listening_add();
    else if(audio_page==1)audio_feature_add();
    else {
        PcAudioMixOptions options=pc_audio_mix_get();
        for(unsigned i=0;i<6;i++) {
            uint32_t item=guest_call(sub_00079D60_gen,6,AUDIO_TEXT(i+2),30u+i,0u,bits(32.f),0u,0u);
            audio_mix_rows[i]=item;if(!item)continue;
            if(guest_writable(item,0x2c))WORD_AT(item+0x28)&=~0x2000u;
            guest_call(sub_0007D480,2,0xA000u,0u);
            for(unsigned value=0;value<=100;value++)guest_call(sub_000717D0,1,AUDIO_PERCENT(value));
            guest_call(sub_00071CF0,2,item,AUDIO_MIX_CHANGE);
            guest_call(sub_00075430,2,item,(uint32_t)(i?options.volume[i-1]:options.master));
        }
        guest_call(sub_00079D60_gen,6,AUDIO_TEXT(8),36u,0u,bits(32.f),AUDIO_FEATURES_OPEN,0u);
        guest_call(sub_00079D60_gen,6,AUDIO_TEXT(10),38u,0u,bits(32.f),AUDIO_LISTENING_OPEN,0u);
    }
    guest_call(sub_00079D60_gen,6,AUDIO_TEXT(9),37u,0u,bits(32.f),AUDIO_MIX_BACK,0u);
    guest_call(sub_000771D0,1,menu);
    restore_registers(&r);g_esp+=4;
}
static void audio_mix_open(void)
{
    GuestRegisters r;uint32_t source=WORD_AT(g_esp+4);save_registers(&r);
    if(guest_writable(source,0x30) && audio_menu_prepare()) {
        audio_parent=WORD_AT(source+0x2c);audio_previous=WORD_AT(source+4);audio_page=0;
        guest_call(sub_00079B70,2,audio_parent,audio_menu_storage);
    }
    restore_registers(&r);g_esp+=4;
}
static void audio_features_open(void)
{
    GuestRegisters r;save_registers(&r);audio_page=1;
    guest_call(sub_00078980,1,audio_menu);guest_call(sub_000770E0,2,audio_menu,0u);
    restore_registers(&r);g_esp+=4;
}
static void audio_mix_back(void)
{
    GuestRegisters r;save_registers(&r);
    if(audio_page) {
        unsigned selected=audio_page==1?6u:7u;audio_page=0;
        guest_call(sub_00078980,1,audio_menu);guest_call(sub_000770E0,2,audio_menu,selected);
    } else {
        guest_call(sub_00079B70,2,audio_parent,audio_previous);
        memset(audio_mix_rows,0,sizeof audio_mix_rows);memset(audio_feature_rows,0,sizeof audio_feature_rows);
        audio_focus_row=audio_range_row=0;
    }
    restore_registers(&r);g_esp+=4;
}
static void audio_mix_tick(void) {g_esp+=4;}
static void audio_listening_open(void)
{
    GuestRegisters r;save_registers(&r);audio_page=2;
    guest_call(sub_00078980,1,audio_menu);guest_call(sub_000770E0,2,audio_menu,0u);
    restore_registers(&r);g_esp+=4;
}
#undef AUDIO_TEXT
#undef AUDIO_PERCENT
