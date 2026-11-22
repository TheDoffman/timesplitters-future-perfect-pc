#ifndef TSFP_OPENGL_HOST_H
#define TSFP_OPENGL_HOST_H
/* The graphics thread owns a context; the window adapter retains the window.
 * Destroy GPU objects before this context. No window/saved-setting ownership. */
typedef struct TsfpGlHost TsfpGlHost;
#ifdef __cplusplus
extern "C" {
#endif
/* Installed by a native window owner before rendering. Returns its physical
 * GL window for a logical RHI window, preserving settings and host bindings. */
void tsfp_gl_host_set_window_adapter(void *(*prepare)(void *));
TsfpGlHost *tsfp_gl_host_create(void *window);
void *tsfp_gl_host_proc(TsfpGlHost *host,const char *name);
/* CPU-only state shares serialized ownership with GPU commands. Windows
 * retains its context on a dedicated worker; SDL GPU calls use the main event
 * thread. Dispatch is synchronous, so argument lifetimes include completion.
 * gpu:0 CPU-only,1 GPU,2 creation,3 destruction. Stop joins the Windows worker. */
int tsfp_gl_host_run(void (*operation)(void *),void *argument,int gpu);
#ifndef _WIN32
/* SDL owner-thread only. Unsigned progress serial wraps; it is not a clock. */
unsigned tsfp_gl_host_main_dispatch_serial(void);
#endif
void tsfp_gl_host_stop(void);
int tsfp_gl_host_make_current(TsfpGlHost *host);
void tsfp_gl_host_release(TsfpGlHost *host);
int tsfp_gl_host_present(TsfpGlHost *host,int interval);
void tsfp_gl_host_destroy(TsfpGlHost *host);
#ifdef __cplusplus
}
#endif
#endif
