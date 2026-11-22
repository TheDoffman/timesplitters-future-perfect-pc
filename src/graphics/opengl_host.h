#ifndef TSFP_OPENGL_HOST_H
#define TSFP_OPENGL_HOST_H
/* The graphics thread owns a context; the window adapter retains the window.
 * Destroy GPU objects before this context. No window/saved-setting ownership. */
typedef struct TsfpGlHost TsfpGlHost;
#ifdef __cplusplus
extern "C" {
#endif
TsfpGlHost *tsfp_gl_host_create(void *window);
void *tsfp_gl_host_proc(TsfpGlHost *host,const char *name);
int tsfp_gl_host_present(TsfpGlHost *host,int interval);
void tsfp_gl_host_destroy(TsfpGlHost *host);
#ifdef __cplusplus
}
#endif
#endif
