/* Shared file IPC. Platform adapters supply process lifetime and atomic rename. */
#include "pc_online_roster.h"
int pc_online_menu_active(void){return online_menu_started && PC_ONLINE_MENU_RUNNING();}
static int online_menu_write(const char *suffix,const void *data,size_t bytes)
{
    char path[sizeof ready_path+16],temporary[sizeof ready_path+20];
    snprintf(path,sizeof path,"%s%s",ready_path,suffix);snprintf(temporary,sizeof temporary,"%s.tmp",path);
    FILE *file=fopen(temporary,"wb");if(!file)return 0;
    int ok=fwrite(data,1,bytes,file)==bytes;if(fclose(file))ok=0;
    if(ok)ok=PC_ONLINE_MENU_REPLACE(temporary,path);
    if(!ok)remove(temporary);return ok;
}
int pc_online_menu_start(void)
{
    if(pc_online_menu_active())return 1;
    online_menu_launching=1;
    int started=pc_online_connect_mode(PC_ONLINE_ARCADE);
    online_menu_launching=0;
    return started;
}
int pc_online_menu_send(PcOnlineMenuRequest *request)
{
    if(!request || !pc_online_menu_active())return 0;
    request->magic=PC_ONLINE_MENU_MAGIC;request->version=PC_ONLINE_MENU_VERSION;
    request->sequence=++online_menu_sequence;
    return online_menu_write(".request",request,sizeof *request);
}
const PcOnlineMenuState *pc_online_menu_poll(void)
{
    if(!pc_online_menu_active()) {
        online_menu_state.status=PC_OM_ERROR;
        snprintf(online_menu_state.message,sizeof online_menu_state.message,"Online connection process stopped");
        return &online_menu_state;
    }
    char path[sizeof ready_path+16];snprintf(path,sizeof path,"%s.state",ready_path);
    FILE *file=fopen(path,"rb");if(!file)return &online_menu_state;
    PcOnlineMenuState next;
    int ok=fread(&next,1,sizeof next,file)==sizeof next && fgetc(file)==EOF;
    fclose(file);
    if(!ok || next.magic!=PC_ONLINE_MENU_MAGIC || next.version!=PC_ONLINE_MENU_VERSION ||
       next.sequence>online_menu_sequence || next.sequence<online_menu_state.sequence ||
       next.status>PC_OM_ERROR || next.mode>PC_ONLINE_STORY || next.count>PC_ONLINE_MENU_MAX ||
       next.host>1u || next.private_owner>1u)return &online_menu_state;
    next.message[255]=next.code[31]=next.invitation[2047]=next.bridge[255]=next.address[255]=next.fingerprint[67]=0;
    for(unsigned i=0;i<next.count;i++) {
        PcOnlineMenuLobby *row=&next.lobbies[i];
        if(row->players>256u || !row->capacity || row->capacity>16u || row->joinable>1u ||
           row->mode<PC_ONLINE_ARCADE || row->mode>PC_ONLINE_STORY ||
           (row->map_scene>100u && row->map_scene!=UINT32_MAX) ||
           (row->game_type_id>=32u && row->game_type_id!=UINT32_MAX))return &online_menu_state;
        row->code[31]=row->name[63]=row->map[63]=row->game_type[31]=row->phase[23]=0;
    }
    online_menu_state=next;return &online_menu_state;
}
int pc_online_menu_accept(void)
{
    const PcOnlineMenuState *state=pc_online_menu_poll();
    if(state->status!=PC_OM_CONNECTED || state->sequence!=online_menu_sequence ||
       !pc_network_set_online(state->bridge))return 0;
    active_mode=(PcOnlineMode)state->mode;return 1;
}
void pc_online_menu_leave(void)
{
    active_mode=PC_ONLINE_NONE;pc_network_set_online(NULL);
    PcOnlineMenuRequest request={0};request.action=PC_OM_LEAVE;request.mode=PC_ONLINE_ARCADE;request.port=41820;
    pc_online_menu_send(&request);
}
int pc_online_menu_publish(PcOnlineMatchInfo *info)
{
    if(!info || !pc_online_menu_active() || active_mode==PC_ONLINE_NONE)return 0;
    info->magic=PC_ONLINE_MENU_MAGIC;info->version=PC_ONLINE_MENU_VERSION;
    return online_menu_write(".match",info,sizeof *info);
}
int pc_online_menu_publish_roster(struct PcOnlineRoster *info)
{
    static uint32_t serial;
    if(!info || !pc_online_menu_active() || active_mode!=PC_ONLINE_ARCADE)return 0;
    info->serial=++serial;
    return online_menu_write(".players",info,sizeof *info);
}
