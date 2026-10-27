#ifndef TSFP_TEXTURE_MODE_H
#define TSFP_TEXTURE_MODE_H
/* Host or guest menu requests; renderer reads. GPU resources stay on render thread. */
int tsfp_texture_is_4k(void);
/* Set an explicit choice without a read/toggle race. Session-only, like F10. */
void tsfp_texture_set_4k(int enabled);
int tsfp_texture_toggle(void);
#endif
