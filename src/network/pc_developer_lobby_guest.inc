/* Original 353760 reads a network player's name at roster record +8.
 * The peer handle is +4; its numeric ID indexes the native XNet sockaddr table. */
#include "pc_network.h"
#include "pc_developer.h"
extern void sub_00353760(void);
static unsigned developer_lobby_slots;
static int developer_lobby_name_slot=-1;
void pc_developer_lobby_guest(void)
{
    PcDeveloperPeer roster[16];unsigned slots[16],count=0;developer_lobby_slots=0;
    if(!WORD_AT(0x76b108u) || !pc_network_enabled()) {pc_developer_lobby_notice("");return;}
    uint32_t local=WORD_AT(0x774028u);
    for(unsigned slot=0;slot<16;slot++) {
        uint32_t record=0x777240u+slot*0x124u;
        if(!(WORD_AT(record+0x4cu)&1u))continue;
        uint32_t peer=WORD_AT(record+4u),ip=0;
        if(!guest_writable(peer,4u))continue;
        uint32_t index=WORD_AT(peer);
        if(index<16u)ip=WORD_AT(0x66648cu+index*16u);
        slots[count]=slot;PcDeveloperPeer *entry=&roster[count++];memset(entry,0,sizeof *entry);
        entry->ip=ip;entry->local=peer==local;
        entry->host=WORD_AT(0x76b140u)==1u?entry->local:index==WORD_AT(0x774040u);
        uint32_t text=guest_call(sub_00353760,1,slot);
        if(!guest_writable(text,64u))text=record+8u;
        const unsigned char *name=(const unsigned char *)((uintptr_t)text+g_xbox_mem_offset);
        for(unsigned n=0;n<64u && name[n];n++)entry->name[n]=name[n]>=32 && name[n]<127?(char)name[n]:'?';
        if(!entry->name[0])snprintf(entry->name,sizeof entry->name,"Player %u",slot+1);
    }
    char names[1152];unsigned mask=pc_network_developer_lobby(roster,count,pc_developer_enabled(),names,sizeof names);
    for(unsigned i=0;i<count;i++)if(mask&(1u<<i))developer_lobby_slots|=1u<<slots[i];
    pc_developer_lobby_notice(names);
}

/* Capture only the native roster-name lookup in198E20; never alter a username. */
void pc_developer_lobby_name(void (*original)(void))
{
    uint32_t caller=WORD_AT(g_esp),slot=WORD_AT(g_esp+4u);
    original();
    if(caller==0x198fd6u) {
        developer_lobby_name_slot=slot<16u?(int)slot:-1;
    }
}
/* Small vector wrench in the native roster font's coordinate system. Keeping
 * it in the title's own UI renderer preserves scaling, aspect and menu fading. */
void pc_developer_lobby_text(void (*original)(void))
{
    uint32_t caller=WORD_AT(g_esp),x=WORD_AT(g_esp+4u),y=WORD_AT(g_esp+8u),text=WORD_AT(g_esp+12u),color=WORD_AT(g_esp+16u),slot=g_ebx;
    original();
    uint32_t phase=WORD_AT(0x76b1a4u);
    if((caller!=0x199583u || slot>=16u || developer_lobby_name_slot!=(int)slot) || !WORD_AT(0x76b108u) ||
       (phase!=11u&&phase!=12u) || developer_lobby_name_slot<0 ||
       !(developer_lobby_slots&(1u<<developer_lobby_name_slot)) ||
       !guest_writable(text,1u) || !*(const char *)((uintptr_t)text+g_xbox_mem_offset))return;
    extern void sub_0003FCF0(void),sub_000814B0(void);
    static uint32_t measure;
    if(!measure)measure=xbox_HeapAlloc(8u,4u);
    if(!measure)return;
    guest_call(sub_0003FCF0,4,measure,measure+4u,text,0u);
    float width,height;memcpy(&width,(const void *)((uintptr_t)measure+g_xbox_mem_offset),4);
    memcpy(&height,(const void *)((uintptr_t)(measure+4u)+g_xbox_mem_offset),4);
    if(!isfinite(width)||!isfinite(height)||width<0||width>1000||height<=0||height>100)return;
    static const char *const rows[]={
        "        ##  ##  ","       ###  ### ","      ####  ####","      ##########",
        "       ######## ","      ########  ","     #######    ","    ######      ",
        "   #####        ","  #####         "," #####          "," ####           ","  ##            "};
    float scale=fmaxf(.6f,fminf(1.5f,height/14.f));
    float left,top;memcpy(&left,&x,4);memcpy(&top,&y,4);
    if(!isfinite(left)||!isfinite(top))return;left+=width+3.f;
    for(unsigned row=0;row<sizeof rows/sizeof rows[0];row++)for(unsigned col=0;col<16;col++)if(rows[row][col]=='#') {
        unsigned end=col+1;while(end<16&&rows[row][end]=='#')end++;
        int l=(int)(left+col*scale),t=(int)(top+row*scale);
        int r=(int)ceilf(left+end*scale),b=(int)ceilf(top+(row+1)*scale);
        guest_call(sub_000814B0,9,bits(1.f),(uint32_t)l,(uint32_t)t,(uint32_t)(r-l),(uint32_t)(b-t),color,color,color,0u);
        col=end-1;
    }
}
