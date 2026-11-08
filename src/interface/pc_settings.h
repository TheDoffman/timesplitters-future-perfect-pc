#ifndef TSFP_PC_SETTINGS_H
#define TSFP_PC_SETTINGS_H
#ifdef _WIN32
#include <windows.h>
#else
typedef void *HWND;
#endif
#include <stddef.h>
#include "pc_weapon_slots.h"
#define PC_PAD_ACTION_COUNT 24
#define PC_DEVELOPER_ACTION (PC_PAD_ACTION_COUNT + PC_WEAPON_SLOT_COUNT)
#define PC_NOCLIP_ACTION (PC_DEVELOPER_ACTION + 1)
#define PC_FREECAM_ACTION (PC_DEVELOPER_ACTION + 2)
#define PC_ACTION_COUNT (PC_DEVELOPER_ACTION + 3)
#define PC_BINDING_SLOTS 2
#define PC_SETTINGS_COMMAND 0x7e20
#define PC_FULLSCREEN_COMMAND 0x7e22
#define PC_MENU_BAR_COMMAND 0x7e23
#define PC_TEXTURE_MODE_COMMAND 0x7e24
typedef struct PcSettings {
    int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS];
    double sensitivity;
    int enabled, invert_y;
    int preset; /* 0 retains custom bindings, 1 overlays Modern PC. */
} PcSettings;
extern const char *const pc_action_labels[PC_ACTION_COUNT];
/* Optional look keys and developer shortcuts stay editable outside Modern PC. */
static inline int pc_settings_action_editable(int preset,int action)
{ return !preset || (action>=19 && action<=22) || action>=PC_DEVELOPER_ACTION; }

/* Shared in-game binding capture; raw keys never pass through menu aliases. */
int pc_controls_binding_keys(unsigned char keys[256]);
void pc_settings_capture_begin(void);
void pc_settings_capture_end(void);
void pc_settings_capture_key(int key);
int pc_settings_capture_event(void);
int pc_settings_capture_blocked(void);
int pc_settings_capture_pump(void);
void pc_settings_defaults(PcSettings *settings);
void pc_settings_modern(PcSettings *settings);
int pc_settings_custom_read(PcSettings *settings,char *error,size_t bytes);
int pc_settings_controller_preset(unsigned port);
/* New-setup semantic Custom bindings; absent in existing configurations. */
int pc_settings_custom_semantics(void);
int pc_settings_controller_preset_save(unsigned port,int modern,char *error,size_t bytes);
/* Native gameplay roles for this local port; identity outside gameplay. */
typedef int (*PcGameplayMasks)(unsigned port,unsigned masks[24],int *mounted);
void pc_settings_gameplay_reader(PcGameplayMasks reader);
int pc_settings_gameplay_masks(unsigned port,unsigned masks[24],int *mounted);
int pc_settings_prompt_keys(int keys[PC_ACTION_COUNT]);
int pc_settings_read(PcSettings *settings, char *error, size_t bytes);
int pc_settings_save(const PcSettings *settings, char *error, size_t bytes);
void pc_settings_key_name(int key, char *out, size_t bytes);
int pc_settings_conflict(const PcSettings *settings, int action, int key);
int pc_settings_key_allowed(int key);
int pc_settings_developer_enabled(void);
int pc_settings_developer_save(int enabled,char *error,size_t bytes);
void pc_settings_developer_keys(int keys[PC_BINDING_SLOTS]);
void pc_settings_action_keys(int action,int keys[PC_BINDING_SLOTS]);
/* 1 applied, 0 invalid, -1 conflict requiring explicit confirmation. */
int pc_settings_assign(PcSettings *settings, int action, int slot, int key, int move);
/* Per-input-profile Windows menu-bar preference; absent means hidden. */
int pc_settings_menu_visible(void);
int pc_settings_save_menu_visible(int visible);
int pc_settings_keyboard_player(void);
void pc_settings_players_read(int devices[4]);
int pc_settings_players_save(const int devices[4],char *error,size_t bytes);
int pc_settings_pad_status(int slot,char *name,size_t bytes,int *pressed);
int pc_settings_players_join(unsigned used_ports);
void pc_settings_attach(HWND game);
void pc_settings_toggle_menu(HWND game);
void pc_settings_detach(HWND game);
void pc_settings_show(HWND game);

/* Raw Xbox stick range; legacy deadzone is inherited until edited. */
int pc_settings_deadzone(unsigned port,int aim);
int pc_settings_deadzone_save(unsigned port,int aim,int value,char *error,size_t bytes);

#endif
