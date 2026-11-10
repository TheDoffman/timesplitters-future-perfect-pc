/* Run the same production adapters for every campaign scene and party size. */
#ifdef _WIN32
static unsigned campaign_ready_scene;
static void campaign_ready_original(void)
{
    CHECK(WORD_AT(0x79094c)==campaign_ready_scene && !WORD_AT(0x790950));
    g_esp+=4;
}
static void campaign_lifecycle_matrix(void)
{
    static const unsigned scenes[13]={2,4,18,11,1,17,6,22,5,14,9,26,81};
    static const int endings[13]={1,3,-1,6,-1,9,11,13,-1,16,18,-1,19};
    for(unsigned mission=0;mission<13;mission++) for(unsigned count=2;count<=4;count++) {
        memset(memory,0,sizeof memory);
        online_mode=PC_ONLINE_STORY;story_lobby_mission=mission;story_pending_stage=1;
        story_reload_epoch=0;story_rejoin_pending=0;story_checkpoint_reset();story_cine_reset();
        for(unsigned i=0;i<13;i++) WORD_AT(0x4d1ac6+i*0x44)=scenes[i];
        for(unsigned i=0;i<13;i++) WORD_AT(0x4d1ae4+i*0x44)=(uint32_t)endings[i];
        uint8_t *config=(uint8_t *)memory+0x7de400;
        config[0]=(uint8_t)count;config[1]=1;config[0x54]=(uint8_t)scenes[mission];
        config[0x55]=10;config[0x56]=2;
        WORD_AT(0x7de458)=0x80008;WORD_AT(0x79094c)=scenes[mission];
        WORD_AT(0x76b108)=1;WORD_AT(0x76b140)=1;WORD_AT(0x76b1a4)=15;
        WORD_AT(0x790950)=count;WORD_AT(0x7b0c48)=0x6000;WORD_AT(0x7b0c7c)=0x6000;
        WORD_AT(0x75f5d0)=0x20000;WORD_AT(0x7356d8)=0x50000;
        WORD_AT(0x774028)=0x30000;WORD_AT(0x774040)=7;WORD_AT(0x31000)=7;
        WORD_AT(0x7de30c)=7;
        for(unsigned slot=0;slot<count;slot++) {
            uint32_t context=0x6000+slot*0x1584,body=0x74000+slot*0x800,actor=0x50000+slot*0xea0;
            WORD_AT(context)=slot;WORD_AT(context+4)=slot?UINT32_MAX:0;
            WORD_AT(context+0x14)=body;WORD_AT(context+0x114)=2;
            WORD_AT(body+0x150)=context;WORD_AT(body+0x7c)=actor;WORD_AT(actor)=body;
        }
        CHECK(story_campaign_scene(mission)==scenes[mission]);
        CHECK(story_checkpoint_scope() && story_restart_session() && story_cine_session());
        CHECK(story_drop_session() && story_world_session() && story_effect_session());
        CHECK(story_objectives_session() && story_dialogue_session());
        const unsigned captures=checkpoint_captures,extras=checkpoint_extras;
        g_esp=0x1800;WORD_AT(g_esp+4)=3;
        pc_story_network_checkpoint_capture(checkpoint_capture_original);
        CHECK(g_esp==0x1804 && checkpoint_captures==captures+1 && checkpoint_extras==extras+count-2);
        CHECK(story_checkpoint_available(8));
        for(unsigned slot=0;slot<count;slot++) CHECK(WORD_AT(0x20004+slot*0x238)==8);
        const StoryCheckpoint checkpoint=story_checkpoint_host;
        const uint32_t match=story_npc_match();
        unsigned restarts=restart_calls;
        story_drop_replicas[0].entity=0x74000;story_drop_fresh[0]=1;
        g_esp=0x1800;WORD_AT(g_esp+4)=8;pc_story_network_restart(restart_original);
        CHECK(restart_calls==restarts+1 && story_reload_epoch==1 && story_context_start_pending);
        CHECK(!story_drop_replicas[0].entity && !story_drop_fresh[0]);
        CHECK(story_checkpoint_available(8));

        /* A guest cannot initiate either kind of reload. Reliable capture
           followed immediately by restart must still restore every bookmark. */
        WORD_AT(0x76b140)=2;story_reload_epoch=0;story_checkpoint_reset();
        for(unsigned slot=0;slot<count;slot++) WORD_AT(0x20004+slot*0x238)=0;
        g_esp=0x1800;WORD_AT(g_esp+4)=0;restarts=restart_calls;
        pc_story_network_restart(restart_original);CHECK(restart_calls==restarts && g_esp==0x1804);
        memcpy((uint8_t *)memory+0x90000,&checkpoint,sizeof checkpoint);
        CHECK(story_checkpoint_receive(0x31000,0x90000,0x1e,sizeof checkpoint));
        StoryRestart restart={STORY_RESTART_MAGIC,match,1,8};
        memcpy((uint8_t *)memory+0x91000,&restart,sizeof restart);
        g_esp=0x1800;
        CHECK(story_restart_receive(0x31000,0x91000,0x1e,sizeof restart));
        CHECK(restart_calls==restarts+1 && story_reload_epoch==1 && story_checkpoint_available(8));
        for(unsigned slot=0;slot<count;slot++) CHECK(WORD_AT(0x20004+slot*0x238)==8);
        story_restart_receive(0x31000,0x91000,0x1e,sizeof restart);
        CHECK(restart_calls==restarts+1); /* duplicate/stale epoch */

        /* Whole-level reload uses the same authority/generation rules and
           invalidates the old checkpoint on every mission. */
        WORD_AT(0x76b140)=1;const uint32_t reload_match=story_npc_match();
        restarts=restart_calls;g_esp=0x1800;WORD_AT(g_esp+4)=0;
        pc_story_network_restart(restart_original);
        CHECK(restart_calls==restarts+1 && story_reload_epoch==2 && !story_checkpoint_available(8));
        WORD_AT(0x76b140)=2;story_reload_epoch=1;
        restart=(StoryRestart){STORY_RESTART_MAGIC,reload_match,2,0};
        memcpy((uint8_t *)memory+0x91000,&restart,sizeof restart);g_esp=0x1800;
        CHECK(story_restart_receive(0x31000,0x91000,0x1e,sizeof restart));
        CHECK(restart_calls==restarts+2 && story_reload_epoch==2 && !story_checkpoint_available(8));

        /* Fresh later missions must not wait for mission one's pistol grant
           before releasing the guest camera. */
        WORD_AT(0x76b140)=1;WORD_AT(0x6008)=0x100000;WORD_AT(0x10021c)=1;
        g_esp=0x1800;story_cine_frame();
        CHECK(story_cine_state.serial && story_cine_state.opening==(mission==0));
        if(mission) CHECK(!story_cine_state.active);

        WORD_AT(0x76b140)=2;WORD_AT(0x7bedc0+0x1f54)=1;story_rewards_tracking=0;
        const unsigned records=reward_records;
        story_rewards_record(mission,endings[mission]<0?3u:2u);
        CHECK(reward_records==records+1);
        story_pending_stage=2;story_rewards_record(mission,2);CHECK(reward_records==records+1);
        story_pending_stage=3;WORD_AT(0x79094c)=106;WORD_AT(0x7de458)=0;
        CHECK(story_rewards_guest());
        campaign_ready_scene=scenes[mission];g_esp=0x1800;
        pc_story_network_ready(campaign_ready_original);
        CHECK(g_esp==0x1804 && WORD_AT(0x79094c)==106 && WORD_AT(0x790950)==count);
        story_pending_stage=1;WORD_AT(0x79094c)=scenes[mission];WORD_AT(0x7de458)=0x80008;

        if(mission) {
            /* Execute the authored inventory opcode observed in Scotland,
               including a carried item which must not replace the firearm. */
            WORD_AT(0x76b140)=1;story_loadout_reset();
            g_esp=0x1800;WORD_AT(g_esp)=0x1e43ac;WORD_AT(g_esp+4)=0x50000;WORD_AT(g_esp+8)=5;
            pc_story_network_loadout_grant(loadout_grant_original);
            CHECK(!story_loadout_host[story_loadout_item(5)].serial); /* pickups stay individual */
            WORD_AT(0x4ea3f8+2*0xe0)=200;WORD_AT(0x4ea3f8+3*0xe0)=200;ammo_loaded=0;
            g_esp=0x1800;WORD_AT(g_esp)=0xc03a5;WORD_AT(g_esp+8)=2;WORD_AT(g_esp+12)=60;
            pc_network_ammo_pickup(frame_original);
            g_esp=0x1800;WORD_AT(g_esp)=0xc03bd;WORD_AT(g_esp+8)=3;WORD_AT(g_esp+12)=5;
            pc_network_ammo_pickup(frame_original);
            g_esp=0x1800;WORD_AT(g_esp)=0xc0407;WORD_AT(g_esp+8)=5;
            pc_story_network_loadout_grant(loadout_grant_original);
            g_esp=0x1800;WORD_AT(g_esp+8)=56;
            pc_story_network_loadout_grant(loadout_grant_original);
            WORD_AT(0x50094)=5;g_esp=0x1800;story_loadout_frame();
            const StoryLoadout firearm=story_loadout_host[story_loadout_item(5)],carried=story_loadout_host[story_loadout_item(56)];
            CHECK(story_loadout_valid(&firearm) && firearm.equipped==1 && !firearm.magazines);
            CHECK(firearm.ammo_kind[0]==2 && firearm.ammo_amount[0]==60 && firearm.ammo_kind[1]==3 && firearm.ammo_amount[1]==5);
            StoryLoadout invalid=firearm;invalid.ammo_kind[0]=32;CHECK(!story_loadout_valid(&invalid));
            invalid=firearm;invalid.ammo_amount[0]=32768;CHECK(!story_loadout_valid(&invalid));
            invalid=firearm;invalid.equipped=3;CHECK(!story_loadout_valid(&invalid));
            CHECK(story_loadout_valid(&carried) && carried.equipped==2 && !carried.magazines);
            CHECK(firearm.mask==((1u<<count)-2u));
            WORD_AT(0x76b140)=2;
            memcpy((uint8_t *)memory+0x90000,&carried,sizeof carried);
            CHECK(story_loadout_receive(0x31000,0x90000,0x1e,sizeof carried));
            memcpy((uint8_t *)memory+0x90000,&firearm,sizeof firearm);
            CHECK(story_loadout_receive(0x31000,0x90000,0x1e,sizeof firearm));
            for(unsigned owner=1;owner<count;owner++) {
                StoryLoadout staged=firearm;staged.equipped=2;--staged.serial;
                story_loadout_pending[story_loadout_item(5)]=staged;
                for(unsigned slot=0;slot<count;slot++) WORD_AT(0x6004+slot*0x1584)=slot==owner?0u:UINT32_MAX;
                const uint32_t actor=0x50000+owner*0xea0;
                WORD_AT(actor+0x94)=1;
                const unsigned grants=loadout_grants,ammo=loadout_ammo;
                const unsigned rounds=loadout_round_awards;
                g_esp=0x1800;story_loadout_frame();
                CHECK(loadout_grants==grants+2 && loadout_ammo==ammo && WORD_AT(actor+0x94)==1);
                CHECK(loadout_round_awards==rounds+2);
                CHECK(((uint8_t *)memory)[actor+0x54c+5] && ((uint8_t *)memory)[actor+0x54c+56]);
                const int changed=count==4 && owner==3;
                if(changed) {((uint8_t *)memory)[actor+0x54c+32]=1;WORD_AT(actor+0x94)=32;}
                memcpy((uint8_t *)memory+0x90000,&firearm,sizeof firearm);
                story_loadout_receive(0x31000,0x90000,0x1e,sizeof firearm);
                story_loadout_frame();CHECK(WORD_AT(actor+0x94)==(changed?32u:5u));
                story_loadout_frame();CHECK(loadout_grants==grants+2 && loadout_ammo==ammo && loadout_round_awards==rounds+2);
                const unsigned selections=loadout_selections;
                const uint32_t selected=WORD_AT(actor+0x94),context=0x6000+owner*0x1584;
                story_cine_inventory(context,1);WORD_AT(actor+0x94)=1;WORD_AT(context+0x114)=256;
                story_cine_inventory(context,0);CHECK(WORD_AT(actor+0x94)==1);
                WORD_AT(context+0x114)=2;story_cine_inventory(context,0);
                CHECK(WORD_AT(actor+0x94)==selected && loadout_selections==selections+1);
                story_cine_inventory(context,0);CHECK(loadout_selections==selections+1 && loadout_round_awards==rounds+2);
            }
            StoryCinematic cue={0};cue.active=1;
            story_cine_control_prepare(&cue,0x6000);CHECK(cue.control_lock);
            cue.active=0;WORD_AT(0x600c)=0x8000;
            story_cine_control_prepare(&cue,0x6000);CHECK(!cue.control_lock);
        }
        online_mode=PC_ONLINE_ARCADE;CHECK(!story_campaign_gameplay() && !story_restart_session());
        online_mode=PC_ONLINE_STORY;story_pending_stage=0;CHECK(!story_campaign_gameplay());
        story_pending_stage=1;WORD_AT(0x7de458)=0x20000000;CHECK(!story_checkpoint_scope());
        /* A fresh join must enter the same bootstrap path on every mission,
           rather than staying frozen at the authored beginning. */
        memcpy((uint8_t *)memory+0x78ad58,config,0xa0);
        WORD_AT(0x78adb0)=0x80008;WORD_AT(0x76b140)=2;WORD_AT(0x76b1a4)=12;
        WORD_AT(0x79094c)=101;g_esp=0x1800;WORD_AT(g_esp)=0x2d398d;
        CHECK(pc_story_network_prepare(0x78ad58) && story_rejoin_pending);
        CHECK(story_lobby_mission==mission && story_pending_stage==1);
    }
    CHECK(story_campaign_scene(13)==UINT32_MAX);
}
#endif
