/* Guest workers can render while the main guest thread waits for loading.
 * Move the single context under a recursive host lock, releasing it before
 * another guest thread enters. Window creation/event pumping stay on main. */
#include <pthread.h>
static pthread_mutex_t native_gl_mutex;
static pthread_once_t native_gl_once=PTHREAD_ONCE_INIT;
static _Thread_local unsigned native_gl_depth;
static void native_gl_lock_init(void) {
    pthread_mutexattr_t a;pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&native_gl_mutex,&a);pthread_mutexattr_destroy(&a);
}
static int native_gl_enter(void) {
    pthread_once(&native_gl_once,native_gl_lock_init);pthread_mutex_lock(&native_gl_mutex);
    if(native_gl_depth++==0 && g.glctx && !SDL_GL_MakeCurrent(g.window,g.glctx)) {
        fprintf(stderr,"[native-gl] cannot acquire context: %s\n",SDL_GetError());abort();
    }
    return 1;
}
static void native_gl_leave(int *unused) {
    if(--native_gl_depth==0 && g.glctx)SDL_GL_MakeCurrent(g.window,NULL);
    pthread_mutex_unlock(&native_gl_mutex);
}
#define NATIVE_GL_SCOPE int native_gl_scope __attribute__((cleanup(native_gl_leave)))=native_gl_enter()
