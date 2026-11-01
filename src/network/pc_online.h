#ifndef TSFP_PC_ONLINE_H
#define TSFP_PC_ONLINE_H
typedef enum PcOnlineMode {
    PC_ONLINE_NONE = 0,
    PC_ONLINE_ARCADE,
    PC_ONLINE_STORY
} PcOnlineMode;
int pc_online_connect(void);
int pc_online_connect_mode(PcOnlineMode mode);
PcOnlineMode pc_online_mode(void);
void pc_online_disconnect(void);
#endif
