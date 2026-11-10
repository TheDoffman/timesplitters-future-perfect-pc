/* Shared campaign lifecycle. Scene IDs come from the original US mission
   table; movies, rewards and authored mission scripts retain their own data. */
enum { STORY_CAMPAIGN_MISSIONS=13u };
static uint32_t story_campaign_scene(unsigned mission)
{
    if(mission>=STORY_CAMPAIGN_MISSIONS) return UINT32_MAX;
    const uint32_t row=0x004d1ac6u+mission*0x44u;
    if(!guest_writable(row,2u)) return UINT32_MAX;
    int16_t scene;memcpy(&scene,(const void *)((uintptr_t)row+g_xbox_mem_offset),2u);
    return scene>=0 && scene<100?(uint32_t)scene:UINT32_MAX;
}
static int story_campaign_scene_active(void)
{
    return story_network_session() && WORD_AT(0x0076b108u) &&
        story_lobby_mission<STORY_CAMPAIGN_MISSIONS && story_pending_stage==1u &&
        WORD_AT(0x0079094cu)<100u && WORD_AT(0x007de458u)==0x00080008u;
}
static int story_campaign_gameplay(void)
{
    return story_campaign_scene_active() && WORD_AT(0x0076b1a4u)==15u;
}
