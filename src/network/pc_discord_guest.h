#ifndef TSFP_PC_DISCORD_GUEST_H
#define TSFP_PC_DISCORD_GUEST_H
/* Read-only decoder for the supported US XBE. The caller supplies guarded
 * reads on the guest thread. No guest calls, allocations, or IPC here. */
#include "pc_discord.h"
#include <string.h>
typedef struct PcDiscordGuestCache {
    uint32_t text_table;
    char event[48][64];
} PcDiscordGuestCache;
typedef int (*PcDiscordRead)(void *context,uint32_t address,void *out,size_t size);

static uint32_t discord_word(PcDiscordRead read,void *ctx,uint32_t address,size_t size)
{
    uint32_t value=0;
    if(!read(ctx,address,&value,size)) return 0;
    return value;
}

static void discord_text(PcDiscordRead read,void *ctx,unsigned id,char *out,size_t size)
{
    /* US sub_0003D7B0 splits common and level-local string tables at 0xD87.
     * Game text is single-byte Latin-1 (see sub_0003FB90's case folding).
     * Convert to UTF-8 without splitting a code point or exporting controls. */
    uint32_t table,pointer;size_t used=0;
    out[0]=0;
    if(!id || id>0x2000u || !discord_word(read,ctx,0x006B7B08u,4)) return;
    table=discord_word(read,ctx,id>=0xD87u?0x006B7B08u:0x006B7B10u,4);
    id=id>=0xD87u?id-0xD87u:id;
    if(!table || table>UINT32_MAX-id*4u) return;
    pointer=discord_word(read,ctx,table+id*4u,4);
    if(!pointer) return;
    for(unsigned i=0;i<192u;i++) {
        unsigned char c;
        if(pointer>UINT32_MAX-i || !read(ctx,pointer+i,&c,1)) {out[0]=0;return;}
        if(!c) {out[used]=0;return;}
        if(c<32u || (c>=127u && c<160u)) {out[0]=0;return;}
        if(used+(c<128u?1u:2u)>=size) {out[used]=0;return;}
        if(c>=128u) {out[used++]=(char)(0xC0u|(c>>6));c=(unsigned char)(0x80u|(c&63u));}
        out[used++]=(char)c;
    }
    out[0]=0;
}

static void pc_discord_sample(PcDiscordSnapshot *s,PcDiscordRead read,void *ctx,PcDiscordGuestCache *cache,int network)
{
    uint32_t config,mode,pause;int active;
    memset(s,0,sizeof *s);
    s->scene=(int32_t)discord_word(read,ctx,0x0079094Cu,4);
    s->flags=discord_word(read,ctx,0x007DE458u,4);
    s->menu_mode=discord_word(read,ctx,0x0078AD2Cu,4);
    s->mode=discord_word(read,ctx,0x0078AD20u,4);
    active=s->scene>=0 && s->scene<=100;
    mode=active?s->mode:s->menu_mode;
    pause=discord_word(read,ctx,0x00761C48u,4);
    s->paused=active && pause && (discord_word(read,ctx,pause,4)&1u);
    if(discord_word(read,ctx,0x0076B0FCu,4) && discord_word(read,ctx,0x0076B108u,4)) {
        /* A local-only LAN host can establish a guest session before the
         * first socket/latency peer. Online is explicit; otherwise this is LAN. */
        s->network=network==2?2:1;
        s->role=(int)discord_word(read,ctx,0x0076B140u,4);
        s->lobby=discord_word(read,ctx,0x0076B1A4u,4)==11u;
    }
    /* Event titles live in the frontend's level-local strings, which are
     * unloaded for gameplay. Copy those built-in names while that table exists.
     * Refresh after language/table replacement; never read a new level's text
     * using a frontend ID. */
    if(cache && s->scene==101) {
        uint32_t table=discord_word(read,ctx,0x006B7B10u,4);
        if(table && table!=cache->text_table) {
            memset(cache,0,sizeof *cache);
            for(unsigned i=0;i<48u;i++) {
                uint32_t row=discord_word(read,ctx,(i<21u?0x00510788u:0x00513D04u)+i*4u,4);
                if(row) discord_text(read,ctx,discord_word(read,ctx,row,2),cache->event[i],sizeof cache->event[i]);
            }
            cache->text_table=table;
        }
    }
    /* Frontend settings may retain a previous game's map and roster. Publish
     * them only for an established lobby; active play uses its copied config. */
    if(!active && !(s->network && s->lobby && mode>=11u && mode<=13u)) return;
    config=active?0x007DE400u:0x0078AD58u;
    s->humans=discord_word(read,ctx,config,1);
    s->bots=discord_word(read,ctx,config+2u,1);
    if(s->network) {
        s->humans=0;
        for(unsigned i=0;i<16u;i++) if(discord_word(read,ctx,0x0077728Cu+i*0x124u,4)&1u) s->humans++;
    }
    if(s->humans>16u || s->bots>10u || s->humans+s->bots>16u) s->humans=s->bots=0;
    if(!s->network && (mode==4u || mode==14u || (s->flags&0x20000000u))) {
        unsigned difficulty=discord_word(read,ctx,config+0x57u,1);
        s->difficulty=difficulty<3u?difficulty+1u:0;
        /* sub_00064D90: thirteen 0x44-byte Story records, scene at +6.
         * Use the running scene, never the next mission highlighted in menus. */
        for(unsigned i=0;i<13u;i++) {
            uint32_t row=0x004D1AC0u+i*0x44u;
            if((int32_t)discord_word(read,ctx,row+6u,2)==s->scene) {
                discord_text(read,ctx,discord_word(read,ctx,row+2u,2),s->mission,sizeof s->mission);break;
            }
        }
        /* Cinematic playback temporarily switches scene/configuration. The
         * latched Story selection remains the mission being played. */
        if((s->flags&0x20000000u) && (mode==4u || mode==14u)) {
            unsigned mission=discord_word(read,ctx,0x0078AD48u,4);
            unsigned selected_difficulty=discord_word(read,ctx,0x0078AD54u,1);
            if(mission<13u)
                discord_text(read,ctx,discord_word(read,ctx,0x004D1AC2u+mission*0x44u,2),s->mission,sizeof s->mission);
            s->difficulty=selected_difficulty<3u?selected_difficulty+1u:0;
            s->humans=discord_word(read,ctx,0x005236C4u,1);
            if(s->humans>2u) s->humans=0;
        }
        return;
    }
    if(!s->network && mode!=5u && mode!=6u && mode!=7u && mode!=9u) return;
    /* Arcade rules: sub_001972C0 renders the name at record +0x18.
     * config +0x56 is the rules index; +0x55 is a different engine category. */
    {
        unsigned rules=discord_word(read,ctx,config+0x56u,1);
        if(rules<20u) discord_text(read,ctx,discord_word(read,ctx,0x004D1F90u+rules*0x20u,2),s->rules,sizeof s->rules);
    }
    for(unsigned i=0;i<17u;i++) {
        uint32_t row=0x004D2390u+i*0x3Cu;
        int32_t scene=(int32_t)discord_word(read,ctx,row,4);
        int32_t target=active?s->scene:(int8_t)discord_word(read,ctx,config+0x54u,1);
        if(scene!=target) continue;
        /* sub_002CA3B0: the two name IDs precede the scene by eight bytes;
         * +4/+8 after the scene are thumbnail assets, not string IDs. */
        if(scene==100) strcpy(s->map,"MapMaker");
        else discord_text(read,ctx,discord_word(read,ctx,row-8u,4),s->map,sizeof s->map);
        if(s->network) {
            unsigned column=mode==11u?2u:mode==13u?3u:0u;
            s->capacity=discord_word(read,ctx,row+0xCu+column*4u,4);
            if(s->capacity>16u || s->capacity<s->humans+s->bots) s->capacity=0;
        }
        break;
    }
    if(!s->network && (mode==6u || mode==7u)) {
        /* sub_002D2F71 copies event ID to config +0x70; sub_0022E050
         * has 21 Challenge and 27 League pointers. Reject stale scene IDs. */
        unsigned event=discord_word(read,ctx,config+0x70u,4);
        if(event<48u && ((mode==7u)==(event<21u))) {
            uint32_t row=discord_word(read,ctx,(event<21u?0x00510788u:0x00513D04u)+event*4u,4);
            if(row && row<=UINT32_MAX-12u && (int32_t)discord_word(read,ctx,row+8u,4)==s->scene)
                if(cache) memcpy(s->event,cache->event[event],sizeof s->event);
        }
    }
}
#endif
