/* Uses the same native list widgets in frontend and paused Video Options. */
static uint32_t graphics_rows[6];
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
static void graphics_restart_labels(void)
{
    text_at(32,"MSAA%s",staged_msaa!=running_msaa?" (Requires restart)":"");
    text_at(37,"Supersampling%s",staged_supersampling!=running_supersampling?" (Requires restart)":"");
}
static void graphics_labels(void)
{
    static const char *const msaa[]={"Off","2x","4x","8x"};
    static const char *const af[]={"Original","2x","4x","8x","16x"};
    static const char *const toggle[]={"Off","On"};
    static const char *const supersampling[]={"100% (Off)","125%","150%","200%"};
    TsfpGraphicsOptions options=pc_graphics_get();int sample=0,filter=0;
    options.msaa=staged_msaa;options.supersampling=staged_supersampling;
    options.anisotropy=staged_anisotropy;options.ambient_occlusion=staged_ao;options.shader_cache=staged_cache;
    for(int i=0;i<4;i++)if(graphics_samples[i]==options.msaa)sample=i;
    for(int i=0;i<5;i++)if(graphics_filters[i]<=options.anisotropy)filter=i;
    memset(graphics_rows,0,sizeof graphics_rows);
    if(page==DISPLAY_ADVANCED) {
        graphics_rows[0]=graphics_choice(0,"MSAA",msaa,4,sample);
        graphics_rows[5]=graphics_choice(5,"Supersampling",supersampling,4,options.supersampling);
        graphics_rows[4]=graphics_choice(4,"Shader cache",toggle,2,options.shader_cache);
        guest_call(sub_00079D60_gen,6,text_at(82,"Clear shader cache"),106u,0u,0u,SELECT_CB,0u);
    } else {
        graphics_rows[2]=graphics_choice(2,"Anisotropic filtering",af,5,filter);
        graphics_rows[3]=graphics_choice(3,"Ambient occlusion",toggle,2,options.ambient_occlusion);
    }
    graphics_restart_labels();
}
static void graphics_read_choices(void)
{
    int sample=0,filter=0;
    for(int i=0;i<5;i++)if(graphics_filters[i]<=staged_anisotropy)filter=i;
    for(int i=0;i<4;i++)if(graphics_samples[i]==staged_msaa)sample=i;
    int indices[6]={sample,0,filter,staged_ao,staged_cache,staged_supersampling};
    for(int i=0;i<6;i++) {
        if(!graphics_rows[i])continue;
        if(!guest_writable(graphics_rows[i],0xcc))return;
        indices[i]=(int)WORD_AT(graphics_rows[i]+0xc8);
        if(indices[i]<0 || indices[i]>=(i==0 || i==5?4:i==2?5:2))return;
    }
    staged_msaa=graphics_samples[indices[0]];staged_supersampling=indices[5];
    /* Preserve an existing in-between AF value until its row changes. */
    if(indices[2]!=filter)staged_anisotropy=graphics_filters[indices[2]];
    staged_ao=indices[3];staged_cache=indices[4];
    graphics_restart_labels();
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
