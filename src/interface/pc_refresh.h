#ifndef TSFP_PC_REFRESH_H
#define TSFP_PC_REFRESH_H
#include <windows.h>
/* Window-thread-only temporary monitor mode; output resolution is unchanged. */
void pc_refresh_fullscreen(HWND window, int fullscreen, int hz);
void pc_refresh_message(HWND window, UINT message, WPARAM wp);
void pc_refresh_shutdown(void);
#endif
