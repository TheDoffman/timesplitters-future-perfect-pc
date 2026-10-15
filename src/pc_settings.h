#ifndef TSFP_PC_SETTINGS_H
#define TSFP_PC_SETTINGS_H
#ifdef _WIN32
#include <windows.h>
#else
typedef void *HWND;
#endif
#include <stddef.h>
#define PC_ACTION_COUNT 24
#define PC_BINDING_SLOTS 4
#define PC_SETTINGS_COMMAND 0x7e20
#define PC_FULLSCREEN_COMMAND 0x7e22
#define PC_MENU_BAR_COMMAND 0x7e23
#define PC_TEXTURE_MODE_COMMAND 0x7e24
typedef struct PcSettings {
    int keys[PC_ACTION_COUNT][PC_BINDING_SLOTS];
    double sensitivity;
    int enabled, invert_y;
} PcSettings;
extern const char *const pc_action_labels[PC_ACTION_COUNT];
void pc_settings_defaults(PcSettings *settings);
int pc_settings_prompt_keys(int keys[PC_ACTION_COUNT]);
int pc_settings_read(PcSettings *settings, char *error, size_t bytes);
int pc_settings_save(const PcSettings *settings, char *error, size_t bytes);
void pc_settings_key_name(int key, char *out, size_t bytes);
int pc_settings_conflict(const PcSettings *settings, int action, int key);
void pc_settings_attach(HWND game);
void pc_settings_toggle_menu(HWND game);
void pc_settings_detach(HWND game);
void pc_settings_show(HWND game);
#endif
