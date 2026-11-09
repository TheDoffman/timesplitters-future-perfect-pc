#ifndef TSFP_TEXTURE_MODE_H
#define TSFP_TEXTURE_MODE_H
/* Host or guest menu requests; renderer reads. GPU resources stay on render thread. */
int tsfp_texture_is_4k(void);
/* Set a live choice only. Persistent UI changes use save_4k. */
void tsfp_texture_set_4k(int enabled);
/* Save first; publish only on success. Shared with the launcher. */
int tsfp_texture_save_4k(int enabled);
/* F10/menu-bar action; failed saves leave the live mode unchanged. */
int tsfp_texture_toggle(void);
#endif
