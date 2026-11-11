#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "network/pc_online.h"
#define REQUIRE(v) do{if(!(v)){fprintf(stderr,"line %d: %s\n",__LINE__,#v);exit(1);}}while(0)
static char ready_path[2048];
static PcOnlineMode active_mode;
#include "network/pc_online_menu_process.h"
static int connected;
int pc_network_set_online(const char *value){connected=value && !strcmp(value,"fixture");return connected;}
int pc_online_connect_mode(PcOnlineMode mode){REQUIRE(mode==PC_ONLINE_ARCADE);online_menu_started=online_menu_launching;return online_menu_started;}
static int replace(const char *a,const char *b){remove(b);return rename(a,b)==0;}
#define PC_ONLINE_MENU_RUNNING() 1
#define PC_ONLINE_MENU_REPLACE(a,b) replace(a,b)
#include "network/pc_online_menu_host.inc"
static void publish(const PcOnlineMenuState *state){
    char path[4096];snprintf(path,sizeof path,"%s.state",ready_path);
    FILE *file=fopen(path,"wb");REQUIRE(file);REQUIRE(fwrite(state,1,sizeof *state,file)==sizeof *state);REQUIRE(!fclose(file));
}
int main(int argc,char **argv){
    REQUIRE(argc==2);snprintf(ready_path,sizeof ready_path,"%s",argv[1]);
    online_menu_clear();online_menu_files_clear(ready_path);REQUIRE(pc_online_menu_start());
    PcOnlineMenuRequest request={0};request.action=PC_OM_CREATE;request.mode=PC_ONLINE_ARCADE;request.port=41820;
    REQUIRE(pc_online_menu_send(&request) && request.sequence==1);
    PcOnlineMenuState state={0};state.magic=PC_ONLINE_MENU_MAGIC;state.version=PC_ONLINE_MENU_VERSION;
    state.sequence=1;state.mode=PC_ONLINE_ARCADE;state.status=PC_OM_CONNECTED;strcpy(state.bridge,"fixture");
    publish(&state);REQUIRE(pc_online_menu_accept() && connected && active_mode==PC_ONLINE_ARCADE);
    PcOnlineMatchInfo info={0};info.mode=PC_ONLINE_ARCADE;info.map_scene=48;
    REQUIRE(pc_online_menu_publish(&info) && info.version==PC_ONLINE_MENU_VERSION);
    state.count=21;publish(&state);REQUIRE(pc_online_menu_poll()->count==0);
    state.count=0;state.sequence=2;state.status=PC_OM_ERROR;publish(&state);
    REQUIRE(pc_online_menu_poll()->sequence==1); /* unsolicited future state */
    pc_online_menu_leave();REQUIRE(!connected && active_mode==PC_ONLINE_NONE && online_menu_sequence==2);
    state.sequence=1;state.status=PC_OM_CONNECTED;publish(&state);REQUIRE(!pc_online_menu_accept());
    state.sequence=2;state.status=PC_OM_LIST;state.count=1;
    state.lobbies[0].capacity=8;state.lobbies[0].mode=PC_ONLINE_ARCADE;state.lobbies[0].joinable=1;
    publish(&state);REQUIRE(pc_online_menu_poll()->count==1);
    state.sequence=1;state.count=0;publish(&state);REQUIRE(pc_online_menu_poll()->sequence==2);
    online_menu_files_clear(ready_path);puts("PASS: native Online IPC rejects stale, malformed and future state");return 0;
}
