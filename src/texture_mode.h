#ifndef TSFP_TEXTURE_MODE_H
#define TSFP_TEXTURE_MODE_H
/* Window thread requests; renderer reads. GPU resources stay on render thread. */
int tsfp_texture_is_4k(void);
int tsfp_texture_toggle(void);
#endif
