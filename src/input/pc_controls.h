#ifndef TSFP_PC_CONTROLS_H
#define TSFP_PC_CONTROLS_H
#ifdef _WIN32
#include <windows.h>
#else
#include "platform/win32_compat.h"
#endif
#include <stdint.h>
#include <stddef.h>
#include "pc_input.h"
#include "pc_menu_mouse.h"
#include "pc_debug.h"
void pc_controls_attach(HWND window);
int pc_controls_message(HWND window, UINT message, WPARAM wp, LPARAM lp, LRESULT *result);
int pc_controls_key_down(int key);
int pc_controls_terminal_click(void);
void pc_controls_menu_mode(int active);
PcMenuPointer pc_controls_menu_pointer(void);
PcMenuPointer pc_controls_menu_hover_pointer(void);
int pc_controls_accept_pad(void);
void pc_controls_apply_mouse(uint32_t player, uint32_t fov_object, ptrdiff_t memory_offset);
void pc_controls_get_mouse(double *sensitivity, int *enabled, int *invert_y);
void pc_controls_set_mouse(double sensitivity, int enabled, int invert_y);
#endif
