/* Native multiplayer flow. The hidden companion owns directory/transport work.
 * 123F3..123FA are verified unused INT3 padding in US 45410066. */
#if defined(_WIN32) || defined(TSFP_NATIVE_NETWORK)
#include "network/pc_online_menu.h"
#include "network/pc_online_roster.h"
#include "pc_feature_flags.h"
#define MP_BUILD 0x123f3u
#define MP_SELECT 0x123f4u
#define MP_BACK 0x123f5u
#define MP_TEXT_DONE 0x123f6u
#define MP_TEXT_CANCEL 0x123f7u
#define MP_MATCH_DRAW 0x123f8u
#define MP_CODE_DRAW 0x123f9u
#define MP_CHOICE_CHANGE 0x123fau
#define MP_DIRECTORY_PORT 30820u
#define MP_MATCH_PORT 30821u
enum {MP_NETWORK,MP_PRIVATE,MP_HOST_SERVER,MP_JOIN_SERVER,MP_BROWSER,MP_CREATE,MP_JOIN_CODE,MP_JOINING,MP_PAGE_COUNT};
static struct {
    unsigned page,source,mode,public_lobby,name_custom,editing,pending,action,autojoin,host,pasting,paste_page,port,copy_down,refresh_down,refresh_queued,paste_down,copy_hover;
    uint32_t choices[2];
    uint32_t storage,menu,parent,request_sequence;
    uint64_t poll_at,connect_at,copy_at,copied_until,metadata_at;
    float copy_bounds[4];
    char name[64],code[2048],address[256],lobby_id[32],notice[256],return_notice[256];
    PcOnlineMenuState state;
} mp_ui;
static int mp_native_passthrough;
extern void sub_001A2730(void),sub_001A2720(void),sub_001A4350(void);
extern void sub_00356870(void),sub_00069720(void);
extern void sub_001A8CE0(void),sub_001A8EE0(void),sub_001CBA50(void),sub_001CBD40(void);
extern void sub_001A8D30(void),sub_001A8F30(void);
extern void sub_00019D70(void),sub_0001ED60(void);
extern void sub_001A4510(void),sub_001CBDA0(void),sub_00064E20(void),sub_0003D7B0(void),sub_00077110(void),sub_0003FA10(void),sub_00072560(void),sub_00075980(void);
static void (*multiplayer_exit_lookup(uint32_t address))(void);
static void multiplayer_lobby_tick(void (*original)(void));
extern void sub_001CC360(void);
extern void sub_00352980(void),sub_00352990(void),sub_0007D090(void);
extern void sub_00071CF0(void),sub_00074A00(void),sub_00071730(void);
extern void sub_00075400(void),sub_00077140(void),sub_00077190(void),sub_00075E70(void),sub_000780E0(void),sub_000781A0(void),sub_00079710(void);
static uint32_t mp_descriptor(unsigned page){return mp_ui.storage+page*16u;}
static uint32_t mp_buffer(unsigned index){return mp_ui.storage+MP_PAGE_COUNT*16u+(mp_ui.page*32u+index)*256u;}
static uint32_t mp_text(unsigned index,const char *value)
{
    uint32_t address=mp_buffer(index);
    snprintf((char *)((uintptr_t)address+g_xbox_mem_offset),256,"%s",value?value:"");return address;
}
static int mp_prepare(void)
{
    if(mp_ui.storage)return 1;
    const unsigned size=MP_PAGE_COUNT*(16u+32u*256u);
    mp_ui.storage=xbox_HeapAlloc(size,4u);if(!mp_ui.storage)return 0;
    memset((void *)((uintptr_t)mp_ui.storage+g_xbox_mem_offset),0,size);
    for(unsigned i=0;i<MP_PAGE_COUNT;i++)WORD_AT(mp_descriptor(i))=MP_BUILD;
    mp_ui.mode=PC_ONLINE_ARCADE;mp_ui.public_lobby=1;
    mp_ui.port=MP_MATCH_PORT;
    snprintf(mp_ui.name,sizeof mp_ui.name,"My Lobby");return 1;
}
static void mp_focus(uint32_t menu)
{
    uint32_t selected=WORD_AT(menu+0x18cu),cursor=WORD_AT(menu+0x178u),first=0;int found=0,has_first=0;
    for(unsigned i=0;i<64u && guest_writable(cursor,0x2cu);i++,cursor=WORD_AT(cursor+4u)) {
        unsigned type=WORD_AT(cursor);
        if((type!=4u && type!=7u && type!=8u) || (WORD_AT(cursor+0x28u)&5u))continue;
        if(!has_first){first=WORD_AT(cursor+0x14u);has_first=1;}
        if(WORD_AT(cursor+0x14u)==selected){found=1;break;}
    }
    if(!found && has_first)guest_call(sub_00077110,2,menu,first);
}
static void mp_rebuild(void)
{
    if(guest_writable(mp_ui.menu,0x23cu)) {
        guest_call(sub_00078980,1,mp_ui.menu);
        /* 78980 restores its saved selection AFTER calling our builder. */
        mp_focus(mp_ui.menu);
    }
}
static void mp_transition(uint32_t menu,int changed)
{
    /* The engine owns each page's fade and sound. A page already in its
       opening phase and an ordinary rebuild retain that original phase. */
    if(changed && guest_writable(menu,0x1b4u) && WORD_AT(menu+0x1b0u)==3u)
        guest_call(sub_00075980,2,menu,2u);
}
static void mp_default_lobby_name(void)
{
    if(mp_ui.name_custom)return;
    char name[32]={0};
    GuestRegisters saved;save_registers(&saved);
    /* Original profile-name accessor resolves the local player even after
       network peer slots move. Its metadata name is bounded to31 characters. */
    const unsigned local=guest_call(sub_00356870,1,0u);
    const uint32_t text=local<16u?guest_call(sub_00069720,1,local):0;
    if(guest_writable(text,sizeof name)) {
        const unsigned char *source=(const unsigned char *)((uintptr_t)text+g_xbox_mem_offset);
        if(memchr(source,0,sizeof name))
            for(unsigned i=0;i<sizeof name-1u && source[i];i++)
                name[i]=source[i]>=32u && source[i]<127u?(char)source[i]:'?';
    }
    restore_registers(&saved);
    size_t length=strlen(name);while(length && name[length-1u]==' ')name[--length]=0;
    snprintf(mp_ui.name,sizeof mp_ui.name,"%s%s",length?name:"My",length?"'s Lobby":" Lobby");
}
static void mp_close_directory(void)
{
    /* Multiplayer is the directory boundary. Lobby/browser returns retain it. */
    pc_online_disconnect();
    mp_ui.pending=mp_ui.autojoin=mp_ui.host=mp_ui.request_sequence=0;
    memset(&mp_ui.state,0,sizeof mp_ui.state);
}
static void mp_show(unsigned page)
{
    if(page==MP_NETWORK)mp_close_directory();
    if(!mp_prepare() || !mp_ui.parent)return;
    if(page==MP_CREATE)mp_default_lobby_name();
    const int changed=mp_ui.page!=page;
    if(changed)mp_ui.notice[0]=0;
    if(changed)mp_ui.refresh_down=mp_ui.refresh_queued=0;
    /* 79B70 may leave the outgoing child attached during its close animation.
       Never rebuild it with the destination page's data or footer callbacks. */
    mp_ui.menu=0;
    mp_ui.page=page;guest_call(sub_00079B70,2,mp_ui.parent,mp_descriptor(page));
    const uint32_t child=WORD_AT(mp_ui.parent+0x170u);
    if(guest_writable(child,0x23cu) && WORD_AT(child+4u)==mp_descriptor(page)){mp_ui.menu=child;mp_rebuild();
        if(changed && (page==MP_JOIN_CODE || page==MP_JOIN_SERVER || page==MP_CREATE))guest_call(sub_00077110,2,child,1u);
        mp_transition(child,changed);}
}
static int mp_story_available(void)
{
#ifdef _WIN32
    return pc_story_coop_enabled();
#else
    return 0; /* Existing native Story transport exception, MAC-013. */
#endif
}
static void mp_request(unsigned action)
{
    if(mp_ui.pending && action!=PC_OM_CANCEL && action!=PC_OM_LEAVE)return;
    if(!pc_online_menu_start()) {
        snprintf(mp_ui.notice,sizeof mp_ui.notice,"Online support could not start");mp_rebuild();return;
    }
    PcOnlineMenuRequest request={0};request.action=action;request.mode=mp_ui.mode;
    request.source=mp_ui.source==1u;request.public_lobby=mp_ui.public_lobby;
    /* Retain the original window's forwarded TCP directory / UDP match pair. */
    request.port=action==PC_OM_PRIVATE_HOST?MP_DIRECTORY_PORT:mp_ui.port;
    snprintf(request.name,sizeof request.name,"%s",mp_ui.name);
    snprintf(request.code,sizeof request.code,"%s",mp_ui.code);
    snprintf(request.address,sizeof request.address,"%s",action==PC_OM_JOIN?mp_ui.lobby_id:mp_ui.address);
    if(pc_online_menu_send(&request)) {
        mp_ui.request_sequence=request.sequence;mp_ui.pending=1;mp_ui.action=action;
        snprintf(mp_ui.notice,sizeof mp_ui.notice,"%s",action==PC_OM_BROWSE?"Refreshing matches...":action==PC_OM_PRIVATE_HOST?"Starting private server...":"Connecting...");
    } else snprintf(mp_ui.notice,sizeof mp_ui.notice,"Could not send the connection request");
    mp_rebuild();
}
static void mp_row(unsigned row,unsigned id,const char *label)
{
    const uint32_t item=guest_call(sub_00079D60_gen,6,mp_text(row,label),id,0u,0u,MP_SELECT,0u);
    const char *description=NULL;
    if(mp_ui.page==MP_NETWORK) {
        const char *help[]={"Browse matches online via the community server","Connect to a private server","Browse matches on your local network"};
        if(id>=1u && id<=3u)description=help[id-1u];
    } else if(mp_ui.page==MP_PRIVATE) {
        if(id==1u)description="Host a server on your own network";
        if(id==2u)description="Join a server using an invite code";
    }
    /* Stock main-menu builder2D0BE0 attaches its scrolling description
       through71730(item,text). The native ticker owns clipping and the logo. */
    if(description && guest_writable(item,0x28u))
        guest_call(sub_00071730,2,item,mp_text(16u+id,description));
}
static void mp_choice(unsigned row,unsigned id,const char *label,const char *first,const char *second,unsigned selected)
{
    uint32_t item=guest_call(sub_00079D60_gen,6,mp_text(row,label),id,0x4200u,0u,0u,0u);
    mp_ui.choices[id-2u]=item;
    if(!guest_writable(item,0xccu))return;
    guest_call(sub_0007D480,2,0x8400u,0u);
    guest_call(sub_000717D0,1,mp_text(21u+(id-2u)*2u,first));
    guest_call(sub_000717D0,1,mp_text(22u+(id-2u)*2u,second));
    guest_call(sub_00071CF0,2,item,MP_CHOICE_CHANGE);
    guest_call(sub_00075430,2,item,selected);
}
static void mp_choice_change(void)
{
    GuestRegisters saved;save_registers(&saved);
    if(mp_ui.page==MP_CREATE && !mp_ui.pending) {
        uint32_t game=mp_ui.choices[0],privacy=mp_ui.choices[1];
        if(guest_writable(game,0xccu) && WORD_AT(game+0xc8u)<2u) {
            if(!WORD_AT(game+0xc8u) || mp_story_available())
                mp_ui.mode=WORD_AT(game+0xc8u)?PC_ONLINE_STORY:PC_ONLINE_ARCADE;
            else {
                guest_call(sub_00075430,2,game,0u);
                snprintf(mp_ui.notice,sizeof mp_ui.notice,"Online Story is unavailable on this host");
            }
        }
        if(guest_writable(privacy,0xccu) && WORD_AT(privacy+0xc8u)<2u)
            mp_ui.public_lobby=!WORD_AT(privacy+0xc8u);
    }
    restore_registers(&saved);g_esp+=4u;
}
static unsigned mp_story_mission(unsigned scene)
{
    for(unsigned i=0;i<13u;i++)
        if(*(const int16_t *)((uintptr_t)(0x4d1ac6u+i*0x44u)+g_xbox_mem_offset)==(int)scene)return i;
    return UINT32_MAX;
}
/* Native separators default to IDFFFFFFFF, also used by LAN's create action.
 * Give decorative rows their own IDs so saved focus cannot resolve to one. */
static void mp_divider(uint32_t menu,unsigned id)
{
    guest_call(sub_00079710,1,0u);
    uint32_t last=0,cursor=WORD_AT(menu+0x178u);
    for(unsigned i=0;i<64u && guest_writable(cursor,0x2cu);i++,cursor=WORD_AT(cursor+4u))
        if(WORD_AT(cursor)==9u)last=cursor;
    if(last)WORD_AT(last+0x14u)=id;
}
static void mp_heading(uint32_t menu,const char *label)
{
    const int compact_heading=mp_ui.page==MP_HOST_SERVER || mp_ui.page==MP_JOIN_SERVER || mp_ui.page==MP_JOIN_CODE;
    char caption[96];snprintf(caption,sizeof caption,"\vs85%s\vs100",label);
    const uint32_t heading=guest_call(sub_00079D60_gen,6,mp_text(30,compact_heading?caption:label),250u,0x33006u,0u,0u,0u);
    /* Uppercase conversion must not turn the native size escape into glyphs. */
    if(compact_heading && guest_writable(heading,0x2cu))WORD_AT(heading+0x28u)&=~0x2000u;
    mp_divider(menu,249u);
}
static void mp_build(void)
{
    GuestRegisters saved;save_registers(&saved);const uint32_t menu=WORD_AT(g_esp+4u);
    if(!mp_prepare()){restore_registers(&saved);g_esp+=4u;return;}
    mp_ui.menu=menu;mp_ui.parent=WORD_AT(menu+0x2cu);
    const char *titles[]={"Multiplayer","Private Server","Host Server","Join Server","Match Browser","Start New Match","Join Match","Join Match"};
    const int browser=mp_ui.page==MP_BROWSER;
    /* 75B70 ORs flags into a reused menu. Clear the preceding page's header
       style so the browser receives the original inset title and full frame. */
    WORD_AT(menu+0x40u)&=~0x4002a000u;
    guest_call(sub_00075B70,5,browser?0x40020000u:0xa000u,mp_text(0,browser?"Matches Available":titles[mp_ui.page]),bits(browser?80.f:55.f),0u,MP_BACK);
    guest_call(sub_00071B10,4,1u,bits(browser?2.5f:2.f),1u,1u);
    guest_call(sub_00071C50,1,mp_ui.page==MP_BROWSER?0x32000u:0x32200u);
    char label[256];unsigned row=1;
    if(browser)mp_heading(menu,"Matches Available");
    else if(mp_ui.page==MP_CREATE)mp_heading(menu,"Lobby Options");
    switch(mp_ui.page) {
    case MP_NETWORK:
        mp_row(row++,1,"Online");mp_row(row++,2,"Private Server");mp_row(row++,3,"LAN");break;
    case MP_PRIVATE:
        mp_row(row++,1,"Host a Server");mp_row(row++,2,"Join by Code");break;
    case MP_HOST_SERVER:
        if(mp_ui.pending && mp_ui.action==PC_OM_PRIVATE_HOST) {
            guest_call(sub_00079D60_gen,6,mp_text(row++,"Starting private server..."),250u,0x32104u,0u,0u,0u);break;
        }
        mp_heading(menu,"INVITE CODE");
        guest_call(sub_00079D60_gen,6,0u,1u,0x20000u,bits(7.f),MP_SELECT,MP_CODE_DRAW);
        mp_row(row++,2,"Copy");mp_row(row++,3,"Next");
        mp_divider(menu,248u);
        guest_call(sub_00079D60_gen,6,mp_text(29,"Share this code with your friends"),251u,0x32104u,bits(3.f),0u,0u);break;
    case MP_JOIN_SERVER:
        mp_heading(menu,"ENTER INVITE CODE");
        guest_call(sub_00079D60_gen,6,0u,1u,0x20000u,bits(7.f),MP_SELECT,MP_CODE_DRAW);
        mp_row(row++,2,"Paste");
        mp_row(row++,3,"Join");break;
    case MP_CREATE:
        guest_call(sub_00079D60_gen,6,0u,1u,0x20000u,bits(7.f),MP_SELECT,MP_CODE_DRAW);
        mp_choice(row++,2,"Game Type","Arcade","Story",mp_ui.mode==PC_ONLINE_STORY);
        mp_choice(row++,3,"Privacy","Open","Invite Only",!mp_ui.public_lobby);
        mp_row(row++,4,"Create Lobby");break;
    case MP_JOIN_CODE:
        mp_heading(menu,"ENTER LOBBY CODE");
        guest_call(sub_00079D60_gen,6,0u,1u,0x20000u,bits(7.f),MP_SELECT,MP_CODE_DRAW);
        mp_row(row++,2,"Paste");mp_row(row++,3,"Join");break;
    case MP_JOINING:
        /* Keep the original selector's index0 record while discovery runs;
           the player sees one progress row rather than another browser. */
        guest_call(sub_00079D60_gen,6,mp_text(1,"Joining lobby..."),0u,0x32204u,bits(7.f),0x1cbd40u,0u);break;
    case MP_BROWSER: {
        WORD_AT(menu+0x40u)|=0x100u;
        guest_call(sub_000780E0,2,bits(65.f),0u);
        /* Stock 1CC0B0 uses a 7-unit action followed by 20-unit 1CBDA0
           advertisements. Let the original list own clipping and scrolling. */
        const int lan=mp_native_passthrough && mp_ui.source==2u;
        guest_call(sub_00079D60_gen,6,mp_text(row++,"Start New Match"),lan?UINT32_MAX:1u,0x32200u,bits(7.f),lan?0x1cba50u:MP_SELECT,0u);
        if(lan) {
            const int count=WORD_AT(menu+0x1b0u)==3u?(int)guest_call(sub_00352990,1,0x790960u):0;
            for(int i=0;i<count && i<20;i++)
                guest_call(sub_00079D60_gen,6,0u,i,0x20000u,bits(20.f),0x1cbd40u,0x1cbda0u);
        } else {
            for(unsigned i=0;i<mp_ui.state.count;i++) {
                const PcOnlineMenuLobby *lobby=&mp_ui.state.lobbies[i];
                snprintf(label,sizeof label,"%.36s   %u/%u   %s%s%s",lobby->name,lobby->players,lobby->capacity,
                    lobby->mode==PC_ONLINE_STORY?"Story":"Arcade",lobby->joinable?"":" - FULL",lobby->invite_only?" - INVITE ONLY":"");
                if((lobby->mode==PC_ONLINE_ARCADE && lobby->game_type_id<32u &&
                   (int32_t)guest_call(sub_00064E20,1,lobby->map_scene)>=0) ||
                   (lobby->mode==PC_ONLINE_STORY && mp_story_mission(lobby->map_scene)<13u))
                    guest_call(sub_00079D60_gen,6,0u,100u+i,0x20000u,bits(20.f),MP_SELECT,MP_MATCH_DRAW);
                else mp_row(row++,100u+i,label);
            }
        }
        guest_call(sub_000781A0,0);
        guest_call(sub_00074A00,4,0u,mp_text(23,"Select"),0u,0u);
        guest_call(sub_00074A00,4,1u,mp_text(24,"Back"),0u,0u);
        guest_call(sub_00074A00,4,2u,mp_text(25,"Refresh"),0u,0u);
        break;
    }}
    if(browser) {
        /* Finalize the native list and publish its bounds. Stock1A42E0 also
           hides its network-dialog parent with70F30; our menu is the list. */
        guest_call(sub_00075400,1,menu);
        WORD_AT(menu+0x68u)=bits(70.f); /* Leave a status band above the native footer. */
        guest_call(sub_00077140,1,menu);guest_call(sub_00077190,1,menu);
        guest_call(sub_00075E70,0);
    } else guest_call(sub_000771D0,1,menu);
    mp_focus(menu);
    restore_registers(&saved);g_esp+=4u;
}
static void mp_code_draw(void)
{
    const uint32_t menu=WORD_AT(g_esp+4u),item=WORD_AT(g_esp+8u),rect=WORD_AT(g_esp+12u);
    /* Native row callbacks also run while pages open/close and while the
       outgoing child is still attached. Fields must not paint over that reveal
       or use the destination page's text on an outgoing page. */
    if(!guest_writable(menu,0x1b4u) || !guest_writable(item,0x20u) || !guest_writable(rect,16u) ||
       mp_native_passthrough || WORD_AT(menu+0x1b0u)!=3u || menu!=mp_ui.menu || WORD_AT(menu+4u)!=mp_descriptor(mp_ui.page)) {g_esp+=4u;return;}
    const float left=FLOAT_AT(rect)+10.f,right=FLOAT_AT(rect+8u)-10.f;
    const float top=FLOAT_AT(rect+4u)+3.f,bottom=FLOAT_AT(rect+12u)-3.f;
    const int name_field=mp_ui.page==MP_CREATE,server_field=mp_ui.page==MP_JOIN_SERVER,host_field=mp_ui.page==MP_HOST_SERVER;
    const char *input=name_field?mp_ui.name:host_field?mp_ui.state.invitation:mp_ui.code;
    const char *caption="LOBBY NAME";
    const char *placeholder=name_field?"ENTER NAME...":server_field?"PASTE INVITE...":"ENTER CODE...";
    const uint32_t bounds=mp_buffer(26),measure=mp_buffer(27);
    const uint32_t font=WORD_AT((name_field && guest_writable(mp_ui.choices[0],0x20u)?mp_ui.choices[0]:item)+0x1cu); /* Stock76193 uses the row's authored face. */
    guest_call(sub_00022FF0,0);guest_call(sub_00019D70,1,1u);guest_call(sub_0003FA10,1,font);
    const uint32_t sx=WORD_AT(0x7e7270u),sy=WORD_AT(0x7e7280u);
    WORD_AT(0x7e7270u)=bits(FLOAT_AT(0x7e7270u)*(name_field?1.f:.8f));WORD_AT(0x7e7280u)=bits(FLOAT_AT(0x7e7280u)*(name_field?1.f:.8f));
    guest_call(sub_0003F8D0,0);
    const uint32_t heading=mp_text(31,caption);
    guest_call(sub_0003FCF0,4,measure,measure+4u,heading,0u);
    const float field=name_field?fmaxf(left+(right-left)*.48f,left+FLOAT_AT(measure)+16.f):left;
    const float field_top=top;
    const float heading_x=name_field?left:(left+right-FLOAT_AT(measure))*.5f;
    const float heading_y=name_field?top+fmaxf(0.f,(bottom-top-FLOAT_AT(measure+4u))*.5f):top;
    if(name_field) {
        const uint32_t colour=guest_call(sub_00154FE0,2,0x7073777fu,WORD_AT(menu+0xf4u));
        guest_call(sub_00041DC0,4,bits(heading_x),bits(heading_y),heading,colour);
    }
    FLOAT_AT(bounds)=field;FLOAT_AT(bounds+4u)=field_top;FLOAT_AT(bounds+8u)=right;FLOAT_AT(bounds+12u)=bottom;
    guest_call(sub_00072560,2,menu,bounds);
    guest_call(sub_00019D70,1,1u);guest_call(sub_0003FA10,1,font);guest_call(sub_0003F8D0,0);
    char value[65];snprintf(value,sizeof value,"%.64s",input);
    if(name_field)for(unsigned i=0;value[i];i++)if(value[i]>='a' && value[i]<='z')value[i]-='a'-'A';
    const uint32_t text=mp_text(28,*input?value:placeholder);
    guest_call(sub_0003FCF0,4,measure,measure+4u,text,0u);
    char *display=(char *)((uintptr_t)text+g_xbox_mem_offset);
    for(unsigned length=(unsigned)strlen(display);length>3u && FLOAT_AT(measure)>right-field-12.f;length--) {
        display[length-4u]='.';display[length-3u]='.';display[length-2u]='.';display[length-1u]=0;
        guest_call(sub_0003FCF0,4,measure,measure+4u,text,0u);
    }
    const float y=field_top+fmaxf(0.f,(bottom-field_top-FLOAT_AT(measure+4u))*.5f);
    const uint32_t colour=guest_call(sub_00154FE0,2,*input?0x7f7f7f7fu:0x545c657fu,WORD_AT(menu+0xf4u));
    guest_call(sub_00041DC0,4,bits(field+6.f),bits(y),text,colour);
    guest_call(sub_00022FF0,0);
    WORD_AT(0x7e7270u)=sx;WORD_AT(0x7e7280u)=sy;guest_call(sub_0003F8D0,0);g_esp+=4u;
}
/* US locked mission/challenge painters2BEC4B/2BEEFD/2BF1B3 resolve
   sprite147: the original padlock record at4D4C04 in table4D2D5C. */
static void mp_privacy_draw(uint32_t menu,float x,float y)
{
    guest_call(sub_00022FF0,0);
    const uint32_t texture=guest_call(sub_00080F20,1,0x147u);
    const uint32_t table=WORD_AT(0x4b85c8u);
    const uint32_t descriptors=guest_writable(table,4u)?WORD_AT(table):0u;
    const uint32_t descriptor=texture<4096u?descriptors+texture*0x34u:0u;
    uint16_t width=0,height=0;
    if(descriptors && guest_writable(descriptor,0x34u)) {
        memcpy(&width,(const void *)((uintptr_t)(descriptor+0x26u)+g_xbox_mem_offset),2u);
        memcpy(&height,(const void *)((uintptr_t)(descriptor+0x2au)+g_xbox_mem_offset),2u);
    }
    if(width && height) {
        const float scale=fminf(24.f/(float)width,30.f/(float)height);
        const int left=(int)(x+.5f),top=(int)(y+(30.f-(float)height*scale)*.5f+.5f);
        const uint32_t colour=guest_call(sub_00154FE0,2,0x7f7f7f7fu,WORD_AT(menu+0xf4u));
        guest_call(sub_0001ED60,1,2u);guest_call(sub_0001ED50,1,texture);guest_call(sub_00019D70,1,1u);
        guest_call(sub_0001F090,9,(uint32_t)left,(uint32_t)top,
            (uint32_t)(left+(int)((float)width*scale+.5f)),(uint32_t)(top+(int)((float)height*scale+.5f)),
            0u,0u,(uint32_t)width<<4,(uint32_t)height<<4,colour);
        guest_call(sub_0001ED60,1,1u);guest_call(sub_00022FF0,0);
    }
    guest_call(sub_00019D70,1,1u);
    const uint32_t font=guest_call(sub_0003FA10,1,3u);
    const uint32_t text=mp_text(28,"Invite Only"),measure=mp_buffer(27);
    guest_call(sub_0003FCF0,4,measure,measure+4u,text,0u);
    const uint32_t colour=guest_call(sub_00154FE0,2,0x7f50507fu,WORD_AT(menu+0xf4u));
    guest_call(sub_00041DC0,4,bits(x+32.f),bits(y+(30.f-FLOAT_AT(measure+4u))*.5f),text,colour);
    guest_call(sub_00022FF0,0);guest_call(sub_0003FA10,1,font);
}
static void mp_match_draw(void)
{
    const uint32_t menu=WORD_AT(g_esp+4u),item=WORD_AT(g_esp+8u),rect=WORD_AT(g_esp+12u);
    if(!guest_writable(item,0x18u) || WORD_AT(item+0x14u)<100u){g_esp+=4u;return;}
    const unsigned index=WORD_AT(item+0x14u)-100u;
    if(index>=mp_ui.state.count){g_esp+=4u;return;}
    const PcOnlineMenuLobby *row=&mp_ui.state.lobbies[index];
    /* The original row painter supplies the game's map thumbnail, mode icon,
       fonts and population caption. Its scratch advertisement exists only
       for this draw and is restored before any discovery/selection runs. */
    uint8_t saved_ad[0xcc];void *ad=(void *)((uintptr_t)0x790960u+g_xbox_mem_offset);
    memcpy(saved_ad,ad,sizeof saved_ad);memset(ad,0,sizeof saved_ad);
    WORD_AT(0x790968u)=row->capacity>row->players?row->capacity-row->players:0u;
    const unsigned mission=row->mode==PC_ONLINE_STORY?mp_story_mission(row->map_scene):UINT32_MAX;
    WORD_AT(0x7909b4u)=row->players;
    /* The stock Arcade painter first resolves an Arcade-map texture. A valid
       local placeholder keeps that lookup bounded; the scoped Story adapter
       replaces the actual picture and adds the authored chapter caption. */
    WORD_AT(0x790974u)=mission<13u?48u:row->map_scene;
    WORD_AT(0x790978u)=mission<13u?2u:row->game_type_id;
    char *title=(char *)ad+0x96u;snprintf(title,54,"%.53s",row->name);
    const uint32_t measure=mp_buffer(29);
    /* Match the original title font and the actual thumbnail boundary. */
    guest_call(sub_0003FA10,1,0x15u);
    const float available=FLOAT_AT(rect+8u)-FLOAT_AT(rect)-FLOAT_AT(0x47d67cu)-FLOAT_AT(0x47862cu)-6.f;
    for(unsigned length=(unsigned)strlen(title);length>3u;length--) {
        guest_call(sub_0003FCF0,4,measure,measure+4u,0x7909f6u,0u);
        if(FLOAT_AT(measure)<=available)break;
        title[length-4u]='.';title[length-3u]='.';title[length-2u]='.';title[length-1u]=0;
    }
    /* Place the lock below the mode text, above the population caption.
       Save the row bounds before the stock painter reuses the rectangle. */
    const float lock_x=FLOAT_AT(rect)+10.f;
    const float lock_y=(FLOAT_AT(rect+4u)+FLOAT_AT(rect+12u))*.5f-8.f;
    const uint32_t id=WORD_AT(item+0x14u);WORD_AT(item+0x14u)=0;
    const unsigned previous=story_browser_preview(mission);
    guest_call(sub_001CBDA0,3,menu,item,rect);
    story_browser_preview(previous);
    if(row->invite_only)mp_privacy_draw(menu,lock_x,lock_y);
    WORD_AT(item+0x14u)=id;memcpy(ad,saved_ad,sizeof saved_ad);g_esp+=4u;
}
extern void sub_002BE690(void),sub_002BE650(void);
static int mp_roster_read(void *ctx,uint32_t address,void *out,size_t bytes)
{
    (void)ctx;
    if(!guest_writable(address,bytes)){if(getenv("TSFP_NATIVE_MENU_TRACE"))fprintf(stderr,"[PC-ONLINE-UI] roster unreadable address=%08X bytes=%zu\n",address,bytes);return 0;}
    memcpy(out,(const void *)((uintptr_t)address+g_xbox_mem_offset),bytes);return 1;
}
static void mp_roster_portrait(void *ctx,int character,int skin,int32_t *portrait,char *label,size_t size)
{
    *portrait=(int32_t)guest_call(sub_002BE690,2,(uint32_t)character,(uint32_t)skin);
    if(*portrait<0 || *portrait>=1024){*portrait=-1;return;}
    /* The original selector resolves skin-specific display names. Entry+C
     * is biography text, not the character name. */
    const uint32_t id=guest_call(sub_002BE650,2,(uint32_t)character,(uint32_t)skin);
    if(id && id<65536u)pc_roster_text(ctx,mp_roster_read,guest_call(sub_0003D7B0,1,id),label,size);
}
static void mp_publish_metadata(void)
{
    if(GetTickCount64()<mp_ui.metadata_at || !WORD_AT(0x76b108u) || WORD_AT(0x76b140u)!=1u || mp_ui.source==2u)return;
    mp_ui.metadata_at=GetTickCount64()+1000u;
    const uint32_t scene=WORD_AT(0x79094cu),config=scene<=100u?0x7de400u:0x78ad58u;
    if(mp_ui.mode==PC_ONLINE_ARCADE && scene<=100u) {
        PcOnlineRoster roster;
        const int captured=pc_online_roster_capture(&roster,NULL,mp_roster_read,mp_roster_portrait);
        const int published=captured?pc_online_menu_publish_roster(&roster):0;
        if(getenv("TSFP_NATIVE_MENU_TRACE"))fprintf(stderr,"[PC-ONLINE-UI] roster captured=%d published=%d game=%u mode=%u\n",captured,published,WORD_AT(0x7de30cu),(unsigned)pc_online_mode());
    }
    const uint8_t *settings=(const uint8_t *)((uintptr_t)config+g_xbox_mem_offset);
    const unsigned map=settings[0x54u],type=settings[0x56u];
    if(type>=32u || map>100u)return;
    uint32_t name_id=0;
    if(mp_ui.mode==PC_ONLINE_ARCADE) {
        const int index=(int32_t)guest_call(sub_00064E20,1,map);
        if(index<0 || index>=17)return;name_id=WORD_AT(0x4d2388u+(unsigned)index*0x3cu);
    } else {
        for(unsigned i=0;i<13u;i++)if(*(const int16_t *)((uintptr_t)(0x4d1ac6u+i*0x44u)+g_xbox_mem_offset)==(int)map)name_id=WORD_AT(0x4d1ad0u+i*0x44u);
        if(!name_id)return;
    }
    const uint32_t name=guest_call(sub_0003D7B0,1,name_id);
    const int16_t rules_id=*(const int16_t *)((uintptr_t)(0x4d1f90u+type*32u)+g_xbox_mem_offset);
    if(rules_id<0 || !guest_writable(name,64u))return;
    PcOnlineMatchInfo info={0};info.mode=mp_ui.mode;info.map_scene=map;info.game_type_id=type;
    info.in_progress=scene<=100u && WORD_AT(0x76b1a4u)==15u;
    snprintf(info.map_name,sizeof info.map_name,"%.63s",(const char *)((uintptr_t)name+g_xbox_mem_offset));
    const uint32_t rules=guest_call(sub_0003D7B0,1,(uint32_t)rules_id);
    if(!guest_writable(rules,32u))return;
    snprintf(info.game_type,sizeof info.game_type,"%.31s",(const char *)((uintptr_t)rules+g_xbox_mem_offset));
    pc_online_menu_publish(&info);
}
static void mp_lan_browser_build(void)
{
    mp_ui.page=MP_BROWSER;mp_build();
}
static void mp_joining_browser_build(void){mp_ui.page=MP_JOINING;mp_build();}
static void mp_root_build(void)
{
    mp_close_directory();
    mp_ui.page=MP_NETWORK;mp_native_passthrough=0;mp_build();
}
static const char *native_multiplayer_notice(uint32_t menu)
{
    static char lobby[256];
    if(native_multiplayer_lobby_notice(menu)) {
        snprintf(lobby,sizeof lobby,"%s %s %s",pc_prompts_keyboard_active()?"[C]":"\vM",
            GetTickCount64()<mp_ui.copied_until?"COPIED":"LOBBY CODE:",mp_ui.state.code);return lobby;
    }
    if(!mp_ui.storage || mp_ui.menu!=menu || mp_native_passthrough)return NULL;
    if(mp_ui.pending && (mp_ui.page==MP_JOINING || (mp_ui.page==MP_HOST_SERVER && mp_ui.action==PC_OM_PRIVATE_HOST)))return NULL;
    if(*mp_ui.return_notice)return mp_ui.return_notice;
    if(*mp_ui.notice)return mp_ui.notice;
    return NULL;
}
static int native_multiplayer_browser_notice(uint32_t menu)
{
    return mp_ui.storage && mp_ui.menu==menu && !mp_native_passthrough && mp_ui.page==MP_BROWSER;
}
static int native_multiplayer_lobby_notice(uint32_t menu)
{
    if(!mp_native_passthrough || mp_ui.source==2u || !mp_ui.state.code[0] ||
       WORD_AT(0x79094cu)!=101u || !guest_writable(menu,8u))return 0;
    const uint32_t descriptor=WORD_AT(menu+4u);
    return descriptor==0x4e99c4u || descriptor==0x4e99d4u || descriptor==0x4e9594u;
}
static int native_multiplayer_notice_bounds(uint32_t menu,float x,float y,float w,float h)
{
    if(!native_multiplayer_lobby_notice(menu))return 0;
    mp_ui.copy_bounds[0]=x;mp_ui.copy_bounds[1]=y;mp_ui.copy_bounds[2]=x+w;mp_ui.copy_bounds[3]=y+h;
    mp_ui.copy_at=GetTickCount64();
    unsigned sw,sh;d3d8_display_output_shape(WORD_AT(0x4e8cf0u),WORD_AT(0x4e8cf4u),&sw,&sh);
    PcMenuPointer pointer=pc_controls_menu_hover_pointer();float px,py;
    mp_ui.copy_hover=sh && pc_menu_mouse_position(pointer,(float)sw/sh,
        (float)WORD_AT(0x4e8cf0u),(float)WORD_AT(0x4e8cf4u),&px,&py) && px>=x && px<=x+w && py>=y-3.f && py<=y+h+3.f;
    return mp_ui.copy_hover;
}
/* Menu ticks stop during gameplay. The Arcade frame hook keeps display
 * metadata fresh without opening menus or altering native engine progress. */
static void native_multiplayer_match_metadata(void)
{
    if(!mp_ui.storage || !mp_native_passthrough || !pc_online_menu_active())return;
    GuestRegisters saved;save_registers(&saved);
    mp_publish_metadata();restore_registers(&saved);
}
static void native_multiplayer_open(uint32_t source)
{
    if(!guest_writable(source,0x3cu) || !mp_prepare())return;
    mp_ui.parent=WORD_AT(source+0x2cu);mp_native_passthrough=0;mp_show(MP_NETWORK);
}
#ifdef _WIN32
static void native_multiplayer_story_open(uint32_t menu)
{
    native_multiplayer_open(menu);mp_ui.mode=PC_ONLINE_STORY;mp_ui.source=0;
    /* Both Online entry points choose Community / Private / LAN first. */
}
#endif
static void mp_edit(unsigned field)
{
    mp_ui.editing=field;
    char port[16];snprintf(port,sizeof port,"%u",mp_ui.port);
    const char *value=field==1u?mp_ui.name:field==2u?mp_ui.code:field==4u?port:mp_ui.address;
    /* Editing must not overwrite slot30, which owns the panel heading. */
    const uint32_t buffer=mp_text(20,value);
    guest_call(sub_0007D090,8,mp_ui.menu,buffer,field==1u?63u:255u,bits(420.f),0u,0u,MP_TEXT_DONE,MP_TEXT_CANCEL);
}
static void mp_text_done(void)
{
    GuestRegisters saved;save_registers(&saved);
    const char *value=(const char *)((uintptr_t)mp_buffer(20)+g_xbox_mem_offset);
    if(mp_ui.editing==1u){snprintf(mp_ui.name,sizeof mp_ui.name,"%.63s",value);mp_ui.name_custom=!!*mp_ui.name;mp_default_lobby_name();}
    else if(mp_ui.editing==2u)snprintf(mp_ui.code,sizeof mp_ui.code,"%s",value);
    else if(mp_ui.editing==4u) {
        char *end;unsigned long port=strtoul(value,&end,10);
        if(*value && !*end && port>=1024u && port<=65535u)mp_ui.port=(unsigned)port;
        else snprintf(mp_ui.notice,sizeof mp_ui.notice,"Choose a UDP match port from 1024 to 65535");
    } else snprintf(mp_ui.address,sizeof mp_ui.address,"%s",value);
    mp_ui.editing=0;mp_rebuild();restore_registers(&saved);g_esp+=4u;
}
static void mp_text_cancel(void){mp_ui.editing=0;g_esp+=4u;}
static void mp_paste(void)
{
    char text[2048];int result=pc_online_menu_paste(text,sizeof text);
    if(result<0){if(!mp_ui.pasting)mp_ui.paste_page=mp_ui.page;mp_ui.pasting=1;return;}
    if(result && (!mp_ui.pasting || mp_ui.paste_page==mp_ui.page)) {
        snprintf(mp_ui.code,sizeof mp_ui.code,"%s",text);mp_ui.notice[0]=0;
    }
    else if(!result)snprintf(mp_ui.notice,sizeof mp_ui.notice,"No invitation or lobby code is available on the clipboard");
    mp_ui.pasting=0;mp_rebuild();
}
static void mp_select(void)
{
    GuestRegisters saved;save_registers(&saved);const uint32_t menu=WORD_AT(g_esp+4u);
    if(!guest_writable(menu,0x190u)){restore_registers(&saved);g_esp+=4u;return;}
    unsigned id=WORD_AT(menu+0x18cu);if(mp_ui.pending || mp_ui.pasting){restore_registers(&saved);g_esp+=4u;return;}
    settings_tabs_sound(mp_ui.page==MP_CREATE && (id==2u || id==3u)?8u:0u);
    mp_ui.return_notice[0]=0;
    switch(mp_ui.page) {
    case MP_NETWORK:
        if(id==1u){mp_ui.source=0;mp_show(MP_BROWSER);mp_request(PC_OM_BROWSE);}
        else if(id==2u){mp_ui.source=1;mp_show(MP_PRIVATE);}
        else if(id==3u){
            /* LAN changes match transport, not the owned directory lifetime. */
            if(pc_online_menu_active())pc_online_menu_leave();else pc_network_set_online(NULL);
            pc_network_enable_lan();multiplayer_profiles_capture();
            mp_ui.source=2;mp_ui.pending=mp_ui.autojoin=0;mp_native_passthrough=1;
            guest_call(sub_001A0A60,0);
        }
        break;
    case MP_PRIVATE:
        if(id==1u){mp_show(MP_HOST_SERVER);mp_request(PC_OM_PRIVATE_HOST);}
        else if(id==2u){mp_ui.code[0]=0;mp_show(MP_JOIN_SERVER);}break;
    case MP_HOST_SERVER:
        if(id==1u || id==2u){if(mp_ui.state.private_owner) {
                snprintf(mp_ui.notice,sizeof mp_ui.notice,"%s",pc_online_menu_copy(mp_ui.state.invitation)?"Invite code copied":"Could not copy invite code");mp_rebuild();
            } else mp_request(PC_OM_PRIVATE_HOST);}
        else if(id==3u){if(mp_ui.state.private_owner){mp_show(MP_BROWSER);mp_request(PC_OM_BROWSE);}
            else {snprintf(mp_ui.notice,sizeof mp_ui.notice,"Start the private server first");mp_rebuild();}}break;
    case MP_JOIN_SERVER:
        if(id==1u)mp_paste();
        else if(id==2u)mp_paste();
        else if(id==3u)mp_request(PC_OM_PRIVATE_CONNECT);break;
    case MP_CREATE:
        if(id==1u)mp_edit(1);
        else if(id==4u)mp_request(PC_OM_CREATE);break;
    case MP_JOIN_CODE:
        if(id==1u)mp_edit(2);
        else if(id==2u)mp_paste();
        else if(id==3u)mp_request(PC_OM_JOIN);break;
    case MP_BROWSER:
        if(id==1u)mp_show(MP_CREATE);
        else if(id>=100u && id-100u<mp_ui.state.count) {
            const PcOnlineMenuLobby *lobby=&mp_ui.state.lobbies[id-100u];
            if(!lobby->joinable)snprintf(mp_ui.notice,sizeof mp_ui.notice,"No player slot is available in that match");
            else if(lobby->mode==PC_ONLINE_STORY && !mp_story_available())snprintf(mp_ui.notice,sizeof mp_ui.notice,"Online Story is unavailable on this host");
            else {
                mp_ui.mode=lobby->mode;
                snprintf(mp_ui.lobby_id,sizeof mp_ui.lobby_id,"%s",lobby->invite_only?lobby->code:"");
                snprintf(mp_ui.code,sizeof mp_ui.code,"%s",lobby->invite_only?"":lobby->code);
                if(lobby->invite_only)mp_show(MP_JOIN_CODE);
                else mp_request(PC_OM_JOIN);
            }
            mp_rebuild();
        }break;
    }
    restore_registers(&saved);g_esp+=4u;
}
static void mp_back(void)
{
    GuestRegisters saved;save_registers(&saved);uint32_t menu=WORD_AT(g_esp+4u);
    settings_tabs_sound(0);
    if(mp_native_passthrough && mp_ui.source==2u) {
        mp_ui.autojoin=0;guest_call(sub_001A4350,1,menu);restore_registers(&saved);g_esp+=4u;return;
    }
    /* Admission runs on the source page. Cancel there without also leaving
       the connected directory or discarding the code-entry page. */
    if(mp_ui.pending && mp_ui.action==PC_OM_JOIN) {
        mp_request(PC_OM_CANCEL);mp_ui.pending=0;
        snprintf(mp_ui.notice,sizeof mp_ui.notice,"Cancelled");mp_rebuild();
        restore_registers(&saved);g_esp+=4u;return;
    }
    if(mp_ui.pending){mp_request(PC_OM_CANCEL);mp_ui.pending=0;snprintf(mp_ui.notice,sizeof mp_ui.notice,"Cancelled");}
    if(mp_ui.page==MP_NETWORK){pc_online_disconnect();guest_call(sub_001A2720,1,menu);}
    else if(mp_ui.page==MP_PRIVATE || mp_ui.page==MP_BROWSER)mp_show(MP_NETWORK);
    else if(mp_ui.page==MP_CREATE || mp_ui.page==MP_JOIN_CODE || mp_ui.page==MP_JOINING){mp_show(MP_BROWSER);if(mp_ui.action==PC_OM_CANCEL)mp_request(PC_OM_BROWSE);}
    else mp_show(MP_PRIVATE);
    restore_registers(&saved);g_esp+=4u;
}
static uint32_t mp_leaf(void)
{
    uint32_t menu=WORD_AT(0x7ba26cu);
    for(unsigned i=0;i<16u && guest_writable(menu,0x174u);i++) {
        uint32_t child=WORD_AT(menu+0x170u);if(!child)break;menu=child;
    }
    return guest_writable(menu,0x23cu)?menu:0;
}
static void mp_connection_ended(const char *reason)
{
    GuestRegisters saved;save_registers(&saved);
    snprintf(mp_ui.return_notice,sizeof mp_ui.return_notice,"%s",reason);
    guest_call(sub_001CC360,0);pc_online_menu_leave();mp_native_passthrough=0;
    mp_ui.pending=mp_ui.autojoin=0;mp_show(MP_BROWSER);
    if(pc_online_menu_active())mp_request(PC_OM_LEAVE);
    restore_registers(&saved);
}
static void native_multiplayer_refresh_click(uint32_t menu)
{
    /* Footer mouse input is sampled after our tick and consumed by the stock
       dispatcher in the same frame. Latch it until the next safe UI tick. */
    if(!mp_native_passthrough && mp_ui.page==MP_BROWSER && mp_ui.menu==menu)
        mp_ui.refresh_queued=1;
}
static void native_multiplayer_tick(void)
{
    if(!mp_ui.storage)return;
    if(mp_native_passthrough && mp_ui.source==2u)return;
    static uint64_t trace_at;
    if(getenv("TSFP_NATIVE_MENU_TRACE") && GetTickCount64()>=trace_at) {
        trace_at=GetTickCount64()+1000u;
            fprintf(stderr,"[PC-ONLINE-UI] scene=%u phase=%u pass=%d pending=%u auto=%u process=%d page=%u menu=%x leaf=%x refresh=%u/%u\n",
            WORD_AT(0x79094cu),WORD_AT(0x76b1a4u),mp_native_passthrough,mp_ui.pending,mp_ui.autojoin,pc_online_menu_active(),mp_ui.page,mp_ui.menu,mp_leaf(),mp_ui.refresh_down,mp_ui.refresh_queued);
    }
    uint32_t leaf=mp_leaf();
    if(!mp_native_passthrough && !mp_ui.editing && leaf==mp_ui.menu && guest_writable(leaf,0x1b4u) && WORD_AT(leaf+0x1b0u)==3u) {
        GuestRegisters focus_saved;save_registers(&focus_saved);mp_focus(leaf);restore_registers(&focus_saved);
    }
    if(!mp_native_passthrough && !mp_ui.editing && !mp_ui.pending && leaf==mp_ui.menu &&
       (mp_ui.page==MP_JOIN_SERVER || mp_ui.page==MP_JOIN_CODE)) {
        unsigned down=!!pc_controls_key_down(0x11) && !!pc_controls_key_down(0x56);
        if(down && !mp_ui.paste_down)mp_paste();
        mp_ui.paste_down=down;
    } else mp_ui.paste_down=0;
    /* Native footer X/keyboard R refreshes the list without adding a row.
       The menu mouse adapter already maps footer clicks to the same edge. */
    if(!mp_native_passthrough && mp_ui.page==MP_BROWSER && leaf==mp_ui.menu &&
       WORD_AT(leaf+0x1b0u)==3u) {
        unsigned down=!!pc_controls_key_down(0x52) || !!(WORD_AT(leaf+0xc4u)&0x40u);
        if((mp_ui.refresh_queued || (down && !mp_ui.refresh_down)) && !mp_ui.pending) {
            mp_ui.refresh_queued=0;mp_request(PC_OM_BROWSE);
        }
        mp_ui.refresh_down=down;
    } else mp_ui.refresh_down=mp_ui.refresh_queued=0;
    if(native_multiplayer_lobby_notice(leaf)) {
        unsigned down=!!pc_controls_key_down(0x43);
        uint32_t pressed=WORD_AT(0x76b0fcu)?0x6b94d8u:0x6b93e0u;
        if(WORD_AT(pressed)&0x80u)down=1; /* Original Xbox right-stick press. */
        PcMenuPointer pointer=pc_controls_menu_hover_pointer();
        if(pointer.valid && pointer.left && mp_ui.copy_hover && GetTickCount64()-mp_ui.copy_at<100u)down=1;
        if(down && !mp_ui.copy_down && pc_online_menu_copy(mp_ui.state.code))mp_ui.copied_until=GetTickCount64()+1800u;
        mp_ui.copy_down=down;
    } else mp_ui.copy_down=0;
    if(GetTickCount64()<mp_ui.poll_at)return;
    mp_ui.poll_at=GetTickCount64()+100u;
    GuestRegisters saved;save_registers(&saved);
    if(mp_ui.pasting)mp_paste();
    if(mp_native_passthrough)mp_publish_metadata();
    if(!pc_online_menu_active()) {
        if(mp_native_passthrough && mp_ui.source!=2u && WORD_AT(0x79094cu)==101u)
            mp_connection_ended("The online connection stopped. You have returned to the match browser.");
        if(mp_ui.pending){mp_ui.pending=0;snprintf(mp_ui.notice,sizeof mp_ui.notice,"Online support stopped. Reopen Multiplayer to try again.");mp_rebuild();}
        restore_registers(&saved);return;
    }
    const PcOnlineMenuState *state=pc_online_menu_poll();
    if(getenv("TSFP_NATIVE_MENU_TRACE") && state->status==PC_OM_ERROR)
        fprintf(stderr,"[PC-ONLINE-UI] error seq=%u expected=%u pending=%u\n",state->sequence,mp_ui.request_sequence,mp_ui.pending);
    if(!mp_ui.pending && mp_native_passthrough && state->status==PC_OM_ERROR && WORD_AT(0x79094cu)==101u) {
        mp_connection_ended(state->message);restore_registers(&saved);return;
    }
    if(mp_ui.pending && state->sequence==mp_ui.request_sequence && state->status!=PC_OM_BUSY) {
        mp_ui.state=*state;mp_ui.pending=0;
        snprintf(mp_ui.notice,sizeof mp_ui.notice,"%s",(state->status==PC_OM_ERROR || (state->status==PC_OM_PRIVATE_READY && mp_ui.action==PC_OM_PRIVATE_HOST))?state->message:"");
        if(state->status==PC_OM_IDLE && mp_ui.action==PC_OM_LEAVE){mp_show(MP_BROWSER);mp_request(PC_OM_BROWSE);}
        else if(state->status==PC_OM_PRIVATE_READY && mp_ui.action==PC_OM_PRIVATE_CONNECT){mp_show(MP_BROWSER);mp_request(PC_OM_BROWSE);}
        else if(state->status==PC_OM_CONNECTED && pc_online_menu_accept()) {
            mp_native_passthrough=1;mp_ui.host=state->host;mp_ui.autojoin=1;mp_ui.connect_at=GetTickCount64();
#ifdef _WIN32
            if(state->mode==PC_ONLINE_STORY)story_rewards_profiles_capture();else
#endif
            multiplayer_profiles_capture();guest_call(sub_001A0A60,0);
        } else {
            if(state->status==PC_OM_ERROR && mp_ui.page==MP_JOINING) {
                mp_show(mp_ui.lobby_id[0]?MP_JOIN_CODE:MP_BROWSER);snprintf(mp_ui.notice,sizeof mp_ui.notice,"%s",state->message);
            }
            mp_rebuild();
        }
    }
    if(mp_ui.autojoin && GetTickCount64()>mp_ui.connect_at+500u) {
        uint32_t menu=mp_leaf();
        if(menu && WORD_AT(menu+0x1b0u)==3u) {
            if(mp_ui.host){mp_ui.autojoin=0;guest_call(sub_001CBA50,0);}
            else {
                if(mp_ui.autojoin==1u){guest_call(sub_00352980,0);mp_ui.autojoin=2;}
                unsigned count=guest_call(sub_00352990,1,0x790960u);
                if(count && count<=20u) {
                    uint32_t row=WORD_AT(menu+0x178u);
                    for(unsigned i=0;i<64u && guest_writable(row,0x3cu);i++,row=WORD_AT(row+4u))
                        if(WORD_AT(row+0x38u)==0x1cbd40u && WORD_AT(row+0x14u)==0u) {
                            WORD_AT(menu+0x198u)=row;WORD_AT(menu+0x18cu)=0;
                            mp_ui.autojoin=0;guest_call(sub_001CBD40,1,menu);break;
                        }
                }
            }
        }
        if(mp_ui.autojoin && GetTickCount64()>mp_ui.connect_at+20000u) {
            mp_ui.autojoin=0;pc_online_menu_leave();mp_native_passthrough=0;
            mp_show(MP_BROWSER);snprintf(mp_ui.notice,sizeof mp_ui.notice,"The match host did not answer. Refresh and try again.");mp_rebuild();
        }
    }
    restore_registers(&saved);
}
static void mp_native_back(void)
{
    GuestRegisters saved;save_registers(&saved);
    settings_tabs_sound(0);
    if(mp_ui.source==2u) {
        /* The stock closing menu waits for phase0 and releases the LAN
           interface before reopening the network chooser. Do not skip it. */
        mp_ui.autojoin=0;guest_call(sub_001A4350,1,WORD_AT(g_esp+4u));
        restore_registers(&saved);g_esp+=4u;return;
    }
    mp_native_passthrough=0;mp_ui.autojoin=0;
    /* The stock Back callbacks open a modal before committing native leave.
       This action already leaves our owned lobby: retire its native session,
       then wait for the directory leave ACK before browsing again. */
    guest_call(sub_001CC360,0);
    if(mp_ui.source==2u)mp_show(MP_NETWORK);
    else if(pc_online_menu_active()){pc_online_menu_leave();mp_ui.pending=0;mp_show(MP_BROWSER);mp_request(PC_OM_LEAVE);}
    restore_registers(&saved);g_esp+=4u;
}
static void mp_lobby_tick(void (*original)(void),uint32_t address)
{
    GuestRegisters saved;save_registers(&saved);native_multiplayer_tick();restore_registers(&saved);
    if(!mp_native_passthrough){g_esp+=4u;return;}
    void (*retained)(void)=multiplayer_exit_lookup(address);
    if(retained)retained();else original();
}
static void mp_host_tick(void){mp_lobby_tick(sub_001A8D30,0x1a8d30u);}
static void mp_guest_tick(void){mp_lobby_tick(sub_001A8F30,0x1a8f30u);}
static void mp_shared_tick(void)
{
    GuestRegisters saved;save_registers(&saved);native_multiplayer_tick();restore_registers(&saved);
    if(!mp_native_passthrough){g_esp+=4u;return;}
    multiplayer_lobby_tick(sub_001A4510);
}
static int native_multiplayer_return_menu(void (*original)(void))
{
    const uint32_t frame=g_esp;
    if(!mp_ui.storage || !mp_native_passthrough || mp_ui.source==2u || !pc_online_menu_active() ||
       WORD_AT(0x79094cu)!=101u || !guest_writable(frame,16u) ||
       WORD_AT(frame+4u)!=WORD_AT(0x7613e4u))return 0;
    const uint32_t descriptor=WORD_AT(frame+8u);
    if(descriptor!=0x4e99c4u && descriptor!=0x4e99d4u && descriptor!=0x4e9404u &&
       descriptor!=0x4e93f4u && descriptor!=0x4e9584u && descriptor!=0x4e9974u &&
       descriptor!=0x4e92c4u && descriptor!=0x4e9574u)return 0;
    GuestRegisters saved;save_registers(&saved);
    const PcOnlineMenuState *state=pc_online_menu_poll();
    snprintf(mp_ui.return_notice,sizeof mp_ui.return_notice,"%s",state->status==PC_OM_ERROR?state->message:
        "The host closed the lobby or the connection ended. You have returned to the match browser.");
    guest_call(sub_001CC360,0);pc_online_menu_leave();mp_native_passthrough=0;mp_ui.autojoin=0;
    mp_ui.page=MP_BROWSER;mp_ui.parent=WORD_AT(frame+4u);mp_ui.pending=0;
    restore_registers(&saved);WORD_AT(frame+8u)=mp_descriptor(MP_BROWSER);original();WORD_AT(frame+8u)=descriptor;
    save_registers(&saved);mp_request(PC_OM_LEAVE);restore_registers(&saved);return 1;
}
extern void sub_000318A0(void),sub_003800D1(void);
static void mp_transport_close(void)
{
    /* 318A0 unregisters the old system-link session and waits forever for
       callback31680. A removed relay room cannot deliver that reply. Once
       our owned controller confirms terminal closure, take the original
       already-detached branch; its ordinary tail still destroys the local
       session/library, closes its socket and resets all session flags.
       This is frontend teardown only, never gameplay/migration recovery. */
    if(WORD_AT(0x76b140u)==2u && WORD_AT(0x76b1a4u)==18u) {
        const uint64_t deadline=GetTickCount64()+2500u;
        while(pc_online_menu_active() && pc_online_menu_poll()->status!=PC_OM_ERROR && GetTickCount64()<deadline)
            guest_call(sub_003800D1,1,50u);
        if(!pc_online_menu_active() || pc_online_menu_poll()->status==PC_OM_ERROR) {
            WORD_AT(0x687ee0u)=WORD_AT(0x687edcu)=0;
            if(getenv("TSFP_NATIVE_MENU_TRACE"))fprintf(stderr,"[PC-ONLINE-UI] closed room: detach legacy session before local cleanup\n");
        }
    }
    sub_000318A0();
}
static void (*native_multiplayer_lookup(uint32_t address))(void)
{
    switch(address) {
    case 0x318a0u:if(mp_ui.storage && mp_native_passthrough && mp_ui.source!=2u && WORD_AT(0x79094cu)==101u)return mp_transport_close;break;
    case 0x1a43e0u:case 0x1a4780u:
        if(mp_native_passthrough && mp_ui.source!=2u && mp_ui.autojoin && !mp_ui.host)return mp_joining_browser_build;
        if(mp_native_passthrough && mp_ui.source==2u)return mp_lan_browser_build;break;
    case MP_CODE_DRAW:return mp_code_draw;
    case MP_CHOICE_CHANGE:return mp_choice_change;
    case MP_MATCH_DRAW:return mp_match_draw;
    case MP_BUILD:return mp_build;case MP_SELECT:return mp_select;case MP_BACK:return mp_back;
    case MP_TEXT_DONE:return mp_text_done;case MP_TEXT_CANCEL:return mp_text_cancel;
    case 0x1a2730u:if(!getenv("TSFP_ONLINE_TEST_COMMAND") && (!mp_native_passthrough || (mp_ui.source==2u && !WORD_AT(0x76b108u))))return mp_root_build;break;
    case 0x1a8d30u:case 0x1a8f30u:
        if(mp_ui.storage && mp_native_passthrough && WORD_AT(0x79094cu)==101u)return address==0x1a8d30u?mp_host_tick:mp_guest_tick;break;
    case 0x1a4510u:
        if(mp_ui.storage && mp_native_passthrough && WORD_AT(0x79094cu)==101u)return mp_shared_tick;break;
    case 0x1a4350u:case 0x1a8ce0u:case 0x1a8ee0u:
        if(mp_native_passthrough && (mp_ui.source==2u || pc_online_menu_active()) && WORD_AT(0x79094cu)==101u)return mp_native_back;break;
    }
    return NULL;
}
#else
static void native_multiplayer_tick(void){}
static void native_multiplayer_refresh_click(uint32_t menu){(void)menu;}
static void native_multiplayer_match_metadata(void){}
static void native_multiplayer_open(uint32_t source){(void)source;}
static const char *native_multiplayer_notice(uint32_t menu){(void)menu;return NULL;}
static int native_multiplayer_lobby_notice(uint32_t menu){(void)menu;return 0;}
static int native_multiplayer_browser_notice(uint32_t menu){(void)menu;return 0;}
static int native_multiplayer_notice_bounds(uint32_t menu,float x,float y,float w,float h){(void)menu;(void)x;(void)y;(void)w;(void)h;return 0;}
static int native_multiplayer_return_menu(void (*original)(void)){(void)original;return 0;}
static void (*native_multiplayer_lookup(uint32_t address))(void){(void)address;return NULL;}
#endif
