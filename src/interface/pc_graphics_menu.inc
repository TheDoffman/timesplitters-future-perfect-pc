/* Uses the same native list widgets in frontend and paused Video Options. */
static uint32_t graphics_rows[5];
static const int graphics_samples[]={1,2,4,8};
static const int graphics_filters[]={1,2,4,8,16};
static uint32_t graphics_choice(int slot,const char *label,const char *const *choices,
                                int count,int selected)
{
    uint32_t item=guest_call(sub_00079D60_gen,6,text_at(32+slot,"%s",label),
                            (uint32_t)(100+slot),0u,0u,0u,0u);
    guest_call(sub_0007D480,2,0x8400u,0u);
    for(int i=0;i<count;i++)guest_call(sub_000717D0,1,text_at(48+slot*5+i,"%s",choices[i]));
    guest_call(sub_00075430,2,item,(uint32_t)selected);
    return item;
}
static void graphics_labels(void)
{
    static const char *const msaa[]={"Off","2x","4x","8x"};
    static const char *const edge[]={"Off","FXAA"};
    static const char *const af[]={"Original","2x","4x","8x","16x"};
    static const char *const toggle[]={"Off","On"};
    TsfpGraphicsOptions options=pc_graphics_get();int sample=0,filter=0;
    for(int i=0;i<4;i++)if(graphics_samples[i]==options.msaa)sample=i;
    for(int i=0;i<5;i++)if(graphics_filters[i]<=options.anisotropy)filter=i;
    graphics_rows[0]=graphics_choice(0,"MSAA",msaa,4,sample);
    graphics_rows[1]=graphics_choice(1,"Edge smoothing",edge,2,options.edge_smoothing);
    graphics_rows[2]=graphics_choice(2,"Texture filtering",af,5,filter);
    graphics_rows[3]=graphics_choice(3,"Ambient occlusion",toggle,2,options.ambient_occlusion);
    graphics_rows[4]=graphics_choice(4,"Shader cache",toggle,2,options.shader_cache);
    guest_call(sub_00079D60_gen,6,text_at(82,"Clear shader cache"),106u,0u,0u,SELECT_CB,0u);
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    small_note(81,*status?status:"MSAA changes require a restart");
#else
    small_note(81,*status?status:"Graphics effects require Vulkan");
#endif
}
static void graphics_read_choices(void)
{
    TsfpGraphicsOptions before=pc_graphics_get(),after=before;
    int indices[5];
    for(int i=0;i<5;i++) {
        if(!graphics_rows[i] || !guest_writable(graphics_rows[i],0xcc))return;
        indices[i]=(int)WORD_AT(graphics_rows[i]+0xc8);
        if(indices[i]<0 || indices[i]>=(i==0?4:i==2?5:2))return;
    }
    after.msaa=graphics_samples[indices[0]];after.edge_smoothing=indices[1];
    /* Keep a legacy in-between filtering value until that row changes. */
    int old_filter=0;
    for(int i=0;i<5;i++)if(graphics_filters[i]<=before.anisotropy)old_filter=i;
    if(indices[2]!=old_filter)after.anisotropy=graphics_filters[indices[2]];
    after.ambient_occlusion=indices[3];after.shader_cache=indices[4];
    if(memcmp(&before,&after,sizeof before) && !pc_graphics_set(&after)) {
        int sample=0;for(int i=0;i<4;i++)if(graphics_samples[i]==before.msaa)sample=i;
        const int values[]={sample,before.edge_smoothing,old_filter,before.ambient_occlusion,before.shader_cache};
        for(int i=0;i<5;i++)guest_call(sub_00075430,2,graphics_rows[i],(uint32_t)values[i]);
        snprintf(status,sizeof status,"Could not save graphics settings");
        text_at(81,"\vs75%s\vs100",status);
    } else if(before.msaa!=after.msaa)msaa_restart_prompt=1;
}
static void counter_read_choice(void)
{
    if(counter_row && WORD_AT(counter_row+0xc8)<=1u &&
       (int)WORD_AT(counter_row+0xc8)!=pc_counter_enabled()) {
        if(!pc_counter_set_enabled((int)WORD_AT(counter_row+0xc8))) {
            guest_call(sub_00075430,2,counter_row,(uint32_t)pc_counter_enabled());
            text_at(81,"\vs75Could not save FPS / Ping\vs100");
        }
    }
}
