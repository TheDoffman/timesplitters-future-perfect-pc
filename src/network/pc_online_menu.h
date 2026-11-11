#ifndef TSFP_PC_ONLINE_MENU_H
#define TSFP_PC_ONLINE_MENU_H
#include <stdint.h>
#include <stddef.h>
/* Local process IPC only. No service tokens or guest pointers enter this ABI. */
#define PC_ONLINE_MENU_MAGIC 0x314d4f54u
#define PC_ONLINE_MENU_VERSION 1u
#define PC_ONLINE_MENU_MAX 20u
enum PcOnlineMenuAction { PC_OM_BROWSE=1,PC_OM_CREATE,PC_OM_JOIN,PC_OM_LEAVE,
    PC_OM_PRIVATE_HOST,PC_OM_PRIVATE_CONNECT,PC_OM_CANCEL };
enum PcOnlineMenuStatus { PC_OM_IDLE,PC_OM_BUSY,PC_OM_LIST,PC_OM_CONNECTED,
    PC_OM_PRIVATE_READY,PC_OM_ERROR };
typedef struct PcOnlineMenuRequest {
    uint32_t magic,version,sequence,action,mode,source,public_lobby,port;
    char name[64],code[2048],address[256],fingerprint[68];
} PcOnlineMenuRequest;
typedef struct PcOnlineMenuLobby {
    uint32_t players,capacity,joinable,mode;
    char code[32],name[64],map[64],game_type[32],phase[24];
} PcOnlineMenuLobby;
typedef struct PcOnlineMenuState {
    uint32_t magic,version,sequence,status,mode,count,host,private_owner;
    char message[256],code[32],invitation[2048],bridge[256],address[256],fingerprint[68];
    PcOnlineMenuLobby lobbies[PC_ONLINE_MENU_MAX];
} PcOnlineMenuState;
_Static_assert(sizeof(PcOnlineMenuRequest)==2468,"Online request ABI");
_Static_assert(sizeof(PcOnlineMenuLobby)==232,"Online lobby ABI");
_Static_assert(sizeof(PcOnlineMenuState)==7588,"Online state ABI");
int pc_online_menu_start(void);
int pc_online_menu_send(PcOnlineMenuRequest *request);
const PcOnlineMenuState *pc_online_menu_poll(void);
int pc_online_menu_accept(void);
int pc_online_menu_active(void);
void pc_online_menu_leave(void);
int pc_online_menu_copy(const char *text);
int pc_online_menu_paste(char *text,size_t capacity);
#endif
