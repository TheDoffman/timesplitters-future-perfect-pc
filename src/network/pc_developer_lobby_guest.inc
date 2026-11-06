/* Original 353760 reads a network player's name at roster record +8.
 * The peer handle is +4; its numeric ID indexes the native XNet sockaddr table. */
#include "pc_network.h"
#include "pc_developer.h"
extern void sub_00353760(void);
void pc_developer_lobby_guest(void)
{
    PcDeveloperPeer roster[16];unsigned count=0;
    if(!WORD_AT(0x76b108u) || !pc_network_enabled()) {pc_network_tools_policy(NULL,0,0);pc_developer_match_policy(0,0);return;}
    uint32_t local=WORD_AT(0x774028u);
    for(unsigned slot=0;slot<16;slot++) {
        uint32_t record=0x777240u+slot*0x124u;
        if(!(WORD_AT(record+0x4cu)&1u))continue;
        uint32_t peer=WORD_AT(record+4u),ip=0;
        if(!guest_writable(peer,4u))continue;
        uint32_t index=WORD_AT(peer);
        if(index<16u)ip=WORD_AT(0x66648cu+index*16u);
        PcDeveloperPeer *entry=&roster[count++];memset(entry,0,sizeof *entry);
        entry->ip=ip;entry->local=peer==local;
        entry->host=WORD_AT(0x76b140u)==1u?entry->local:index==WORD_AT(0x774040u);
        uint32_t text=guest_call(sub_00353760,1,slot);
        if(!guest_writable(text,64u))text=record+8u;
        const unsigned char *name=(const unsigned char *)((uintptr_t)text+g_xbox_mem_offset);
        for(unsigned n=0;n<64u && name[n];n++)entry->name[n]=name[n]>=32 && name[n]<127?(char)name[n]:'?';
        if(!entry->name[0])snprintf(entry->name,sizeof entry->name,"Player %u",slot+1);
    }
    int allowed=pc_network_tools_policy(roster,count,developer_match_tools);
    pc_developer_match_policy(1,allowed);
}
/* Compatibility wrappers for the existing patch hooks. Never decorate names. */
void pc_developer_lobby_name(void (*original)(void)) {original();}
void pc_developer_lobby_text(void (*original)(void)) {original();}
