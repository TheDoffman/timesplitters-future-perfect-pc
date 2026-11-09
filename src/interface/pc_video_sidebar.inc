/* Game-thread adapter for the native sidebar. Reuse the same settings files,
 * display preview watchdog, graphics setters and native profile save flag as
 * Video Options. No guest pointers cross the mailbox. */
static PcVideoSnapshot video_host;
static PcVideoValues video_before,video_pending;
static ULONGLONG video_preview_deadline,video_sample_at,video_modes_at;
static int video_running_msaa,video_running_ss;
static int video_equal(const PcVideoValues *a,const PcVideoValues *b) {return !memcmp(a,b,sizeof *a);}
static PcVideoValues video_read(void) {
    PcVideoValues v={0};PcDisplayMode m=pc_display_current();TsfpGraphicsOptions g=pc_graphics_get();
    v.width=m.width;v.height=m.height;
    v.value[VIDEO_RATE]=pc_frame_rate_value(pc_frame_rate_choice(pc_native_rate()));
    v.value[VIDEO_FOV]=pc_display_fov();v.value[VIDEO_TEXTURE]=tsfp_texture_is_4k();
    v.value[VIDEO_AF]=g.anisotropy;v.value[VIDEO_AO]=g.ambient_occlusion;v.value[VIDEO_AA]=(int)pc_post_aa_get();
    v.value[VIDEO_MSAA]=g.msaa;v.value[VIDEO_SS]=g.supersampling;v.value[VIDEO_CACHE]=g.shader_cache;
    extern void sub_00069640(void);uint32_t profile=guest_call(sub_00069640,0);
    v.value[VIDEO_SUBTITLES]=(guest_writable(profile,0x134)?WORD_AT(profile+0x130):WORD_AT(0x007613DCu))!=0;
    v.value[VIDEO_FEED]=pc_feed_enabled();v.value[VIDEO_COUNTER]=pc_counter_enabled();
    v.value[VIDEO_FULLSCREEN]=pc_controls_developer_is_fullscreen();return v;
}
static int video_valid(const PcVideoValues *v) {
    if(!pc_display_valid((PcDisplayMode){v->width,v->height}) ||
       v->value[VIDEO_FOV]<PC_FOV_MIN || v->value[VIDEO_FOV]>PC_FOV_MAX ||
       (v->value[VIDEO_RATE]!=0&&v->value[VIDEO_RATE]!=120&&v->value[VIDEO_RATE]!=180&&v->value[VIDEO_RATE]!=240) ||
       v->value[VIDEO_AA]<0 || v->value[VIDEO_AA]>2)return 0;
    TsfpGraphicsOptions g=pc_graphics_get();g.msaa=v->value[VIDEO_MSAA];g.supersampling=v->value[VIDEO_SS];
    g.anisotropy=v->value[VIDEO_AF];g.ambient_occlusion=v->value[VIDEO_AO];g.shader_cache=v->value[VIDEO_CACHE];
    if(!tsfp_graphics_valid(&g))return 0;
    const int toggles[]={VIDEO_TEXTURE,VIDEO_SUBTITLES,VIDEO_FEED,VIDEO_COUNTER,VIDEO_FULLSCREEN};
    for(unsigned i=0;i<sizeof toggles/sizeof *toggles;i++)if(v->value[toggles[i]]<0||v->value[toggles[i]]>1)return 0;
    return 1;
}
static int video_restore_preview(void) {
    if(!pc_display_apply((PcDisplayMode){video_before.width,video_before.height})) {
        snprintf(video_host.message,sizeof video_host.message,"Could not restore resolution; retry Revert.");return 0;
    }
    pc_display_set_fov(video_before.value[VIDEO_FOV]);video_preview_deadline=0;return 1;
}
static int video_save(const PcVideoValues *v,const PcVideoValues *before) {
    TsfpGraphicsOptions original=pc_graphics_get(),g=original;
    int display=v->width!=before->width||v->height!=before->height||v->value[VIDEO_FOV]!=before->value[VIDEO_FOV];
    int rate=v->value[VIDEO_RATE]!=before->value[VIDEO_RATE];
    int aa=v->value[VIDEO_AA]!=before->value[VIDEO_AA];
    int feed=v->value[VIDEO_FEED]!=before->value[VIDEO_FEED],counter=v->value[VIDEO_COUNTER]!=before->value[VIDEO_COUNTER];
    int display_saved=0,rate_saved=0,graphics_saved=0,aa_saved=0,feed_saved=0,counter_saved=0;
    g.msaa=v->value[VIDEO_MSAA];g.supersampling=v->value[VIDEO_SS];g.anisotropy=v->value[VIDEO_AF];
    g.ambient_occlusion=v->value[VIDEO_AO];g.shader_cache=v->value[VIDEO_CACHE];
    pc_display_set_fov(v->value[VIDEO_FOV]);
    if(display){if(!pc_display_save())goto failed;display_saved=1;}
    if(rate){if(!pc_frame_rate_save(v->value[VIDEO_RATE]))goto failed;rate_saved=1;}
    if(memcmp(&g,&original,sizeof g)){if(!pc_graphics_set(&g))goto failed;graphics_saved=1;}
    if(aa){if(!pc_post_aa_set((unsigned)v->value[VIDEO_AA]))goto failed;aa_saved=1;}
    if(feed){if(!pc_feed_set_enabled(v->value[VIDEO_FEED]))goto failed;feed_saved=1;}
    if(counter){if(!pc_counter_set_enabled(v->value[VIDEO_COUNTER]))goto failed;counter_saved=1;}
    /* Publish nonsaving/guest state only after every fallible write succeeds. */
    if(rate)recomp_config_request_native_rate(v->value[VIDEO_RATE]);
    if(v->value[VIDEO_SUBTITLES]!=before->value[VIDEO_SUBTITLES]) {
        extern void sub_00069640(void);uint32_t profile=guest_call(sub_00069640,0);
        WORD_AT(0x007613DCu)=(uint32_t)v->value[VIDEO_SUBTITLES];
        if(guest_writable(profile,0x134)){WORD_AT(profile+0x130)=(uint32_t)v->value[VIDEO_SUBTITLES];WORD_AT(0x0078AD24u)|=0x10000u;}
    }
    if(feed)pc_feed_clear(&kill_feed);
    tsfp_texture_set_4k(v->value[VIDEO_TEXTURE]);
    if(v->value[VIDEO_FULLSCREEN]!=pc_controls_developer_is_fullscreen())pc_controls_developer_fullscreen();
    video_preview_deadline=0;
    video_host.restart=v->value[VIDEO_MSAA]!=video_running_msaa||v->value[VIDEO_SS]!=video_running_ss;
    snprintf(video_host.message,sizeof video_host.message,video_host.restart?"Video settings saved. Restart to apply MSAA / supersampling.":"Video settings saved.");return 1;
failed: {
    int restored=1;
    if(counter_saved&&!pc_counter_set_enabled(before->value[VIDEO_COUNTER]))restored=0;
    if(feed_saved&&!pc_feed_set_enabled(before->value[VIDEO_FEED]))restored=0;
    if(aa_saved&&!pc_post_aa_set((unsigned)before->value[VIDEO_AA]))restored=0;
    if(graphics_saved&&!pc_graphics_set(&original))restored=0;
    if(rate_saved&&!pc_frame_rate_save(before->value[VIDEO_RATE]))restored=0;
    pc_display_set_fov(before->value[VIDEO_FOV]);
    if(!pc_display_apply((PcDisplayMode){before->width,before->height}))restored=0;
    if(display_saved&&!pc_display_save())restored=0;
    video_preview_deadline=0;
    snprintf(video_host.message,sizeof video_host.message,restored?"Could not save video settings. Changes reverted; try again.":"Could not restore all video settings. Review settings before continuing.");return 0;
}}
static void video_sidebar_tick(void) {
    ULONGLONG now=GetTickCount64();PcVideoRequest request={0};
    int pending=pc_developer_video_take(&request);
    if(!video_running_msaa){TsfpGraphicsOptions g=pc_graphics_get();video_running_msaa=g.msaa;video_running_ss=g.supersampling;}
    if(video_preview_deadline&&(!pc_developer_open()||now>=video_preview_deadline)) {
        if(video_restore_preview())snprintf(video_host.message,sizeof video_host.message,"Resolution preview reverted.");
    }
    if(!pending&&(!pc_developer_open()||now<video_sample_at))return;
    video_sample_at=now+200;
    if(pending) {
        video_host.completed=request.serial;
        if(request.action==VIDEO_REVERT) {if(video_preview_deadline)video_restore_preview();}
        else if(request.action==VIDEO_CLEAR_CACHE) {
            int count=pc_graphics_clear_cache();snprintf(video_host.message,sizeof video_host.message,count<0?"Could not clear some shader cache files.":"Shader cache cleared.");
        } else if(request.action==VIDEO_RESTART) {
            if(!pc_game_restart())snprintf(video_host.message,sizeof video_host.message,"Could not restart. Relaunch the game manually.");
        } else if(request.action==VIDEO_KEEP&&video_preview_deadline) {
            if(pc_developer_open()&&now<video_preview_deadline)video_save(&video_pending,&video_before);
        } else if(request.action==VIDEO_APPLY&&!video_preview_deadline&&pc_developer_open()) {
            PcVideoValues before=video_read();
            if(!video_valid(&request.desired))snprintf(video_host.message,sizeof video_host.message,"Invalid video settings.");
            else if(request.desired.width!=before.width||request.desired.height!=before.height) {
                video_before=before;video_pending=request.desired;
                if(pc_display_preview((PcDisplayMode){request.desired.width,request.desired.height})) {
                    video_preview_deadline=now+15000;snprintf(video_host.message,sizeof video_host.message,"Keep this resolution? Reverts automatically in 15 seconds.");
                } else snprintf(video_host.message,sizeof video_host.message,"Cannot preview this resolution.");
            } else video_save(&request.desired,&before);
        }
    }
    PcVideoValues current=video_read();
    if(!video_equal(&current,&video_host.current)||pending)video_host.revision++;
    video_host.current=current;video_host.available=1;
    video_host.restart=current.value[VIDEO_MSAA]!=video_running_msaa||current.value[VIDEO_SS]!=video_running_ss;
#if defined(_WIN32) || defined(TSFP_RHI_RENDERER)
    video_host.effects=1;
#endif
    if(now>=video_modes_at||!video_host.mode_count) {
        PcDisplayMode list[128];int count=pc_display_modes(xbox_D3D8Window(),list,128);
        video_host.mode_count=(unsigned)(count>0?count:0);
        for(unsigned i=0;i<video_host.mode_count;i++){video_host.modes[i][0]=list[i].width;video_host.modes[i][1]=list[i].height;}
        video_modes_at=now+2000;
    }
    video_host.preview=video_preview_deadline!=0;
    video_host.seconds=video_preview_deadline>now?(unsigned)((video_preview_deadline-now+999)/1000):0;
    pc_developer_video_publish(&video_host);
}
