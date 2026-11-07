/* Shared native audio choices. Profile drafts retain their original handlers;
 * mix, enhancements and listening sidecars continue to save live. */
#include "interface/pc_audio_mix_settings.h"
#define AUDIO_BETA_CHANGE 0x00012027u
#define AUDIO_MIX_CHANGE 0x0001202au
#define AUDIO_LISTENING_CHANGE 0x0001202fu
static uint32_t bits(float f);
static uint32_t audio_menu_storage,audio_menu;
static uint32_t audio_feature_rows[5],audio_mix_rows[6],audio_beta_labels;
static uint32_t audio_focus_row,audio_range_row;
static int audio_page; /* 0 profile, 1 mix, 2 enhancements, 3 listening */
#define AUDIO_TEXT(i) (audio_menu_storage+32u+(i)*128u)
#define AUDIO_PERCENT(i) (audio_menu_storage+32u+22u*128u+(i)*8u)
static int audio_menu_prepare(void)
{
    if(!audio_menu_storage) {
        audio_menu_storage=xbox_HeapAlloc(32+22*128+101*8,4);
        if(!audio_menu_storage)return 0;
        memset((void *)((uintptr_t)audio_menu_storage+g_xbox_mem_offset),0,32+22*128+101*8);
        static const char *const labels[]={" ","Enhanced Audio","Master","Effects","Music",
            "Dialogue","Ambience","Movies","Enhancements","Back","Listening",
            "Dialogue focus","Impact range","Original","Reduced","Night","Off","On","VOLUME","MIX","ENHANCEMENTS","LISTENING"};
        for(unsigned i=0;i<22;i++)
            snprintf((char *)((uintptr_t)AUDIO_TEXT(i)+g_xbox_mem_offset),128,"%s",labels[i]);
        for(unsigned i=0;i<=100;i++)
            snprintf((char *)((uintptr_t)AUDIO_PERCENT(i)+g_xbox_mem_offset),8,"%u%%",i);
    }
    return 1;
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
                memset(slot,0,128);snprintf(slot,112,"%s",captions[i]);
                memcpy(slot+112,"Off\0On",7);
            }
        }
    }
    if(!audio_beta_labels)return;
    PcAudioOptions options=pc_audio_get_options();
    int values[]={options.enhanced,options.spatial,options.weapon_range,options.environment,options.ambience};
    for(unsigned i=0;i<5;i++) {
        uint32_t text=audio_beta_labels+i*128u;
        uint32_t item=guest_call(sub_00079D60_gen,6,text,15u+i,0u,0u,0u,0u);
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
    audio_focus_row=guest_call(sub_00079D60_gen,6,AUDIO_TEXT(11),39u,0u,0u,0u,0u);
    if(audio_focus_row) {
        if(guest_writable(audio_focus_row,0x2c))WORD_AT(audio_focus_row+0x28)&=~0x2000u;
        guest_call(sub_0007D480,2,0xA000u,0u);
        guest_call(sub_000717D0,1,AUDIO_TEXT(16));guest_call(sub_000717D0,1,AUDIO_TEXT(17));
        guest_call(sub_00071CF0,2,audio_focus_row,AUDIO_LISTENING_CHANGE);
        guest_call(sub_00075430,2,audio_focus_row,(uint32_t)options.ducking);
    }
    audio_range_row=guest_call(sub_00079D60_gen,6,AUDIO_TEXT(12),40u,0u,0u,0u,0u);
    if(audio_range_row) {
        if(guest_writable(audio_range_row,0x2c))WORD_AT(audio_range_row+0x28)&=~0x2000u;
        guest_call(sub_0007D480,2,0xA000u,0u);
        for(unsigned i=13;i<16;i++)guest_call(sub_000717D0,1,AUDIO_TEXT(i));
        guest_call(sub_00071CF0,2,audio_range_row,AUDIO_LISTENING_CHANGE);
        guest_call(sub_00075430,2,audio_range_row,(uint32_t)options.dynamics);
    }
}
static void audio_mix_add(void)
{
    extern void sub_00071CF0(void);
    PcAudioMixOptions options=pc_audio_mix_get();
    for(unsigned i=0;i<6;i++) {
        uint32_t item=guest_call(sub_00079D60_gen,6,AUDIO_TEXT(i+2),30u+i,0u,0u,0u,0u);
        audio_mix_rows[i]=item;if(!item)continue;
        if(guest_writable(item,0x2c))WORD_AT(item+0x28)&=~0x2000u;
        guest_call(sub_0007D480,2,0xA000u,0u);
        for(unsigned value=0;value<=100;value++)guest_call(sub_000717D0,1,AUDIO_PERCENT(value));
        guest_call(sub_00071CF0,2,item,AUDIO_MIX_CHANGE);
        guest_call(sub_00075430,2,item,(uint32_t)(i?options.volume[i-1]:options.master));
    }
}
#undef AUDIO_TEXT
#undef AUDIO_PERCENT
