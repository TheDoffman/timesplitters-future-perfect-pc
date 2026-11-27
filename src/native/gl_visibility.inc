#include "graphics/visibility_queries.h"
#include "graphics/visibility_draw_scope.h"
typedef struct NativeVisibility {GLuint id;int active,ended;} NativeVisibility;
static TsfpVisibilityQueries native_visibility;
static void *native_visibility_create(void *host) {
    (void)host;NativeVisibility *q=calloc(1,sizeof(*q));if(!q)return NULL;
    glGenQueries(1,&q->id);if(!q->id){free(q);return NULL;}return q;
}
static void native_visibility_destroy(void *host,void *query) {
    (void)host;NativeVisibility *q=query;
    if(q->active)glEndQuery(GL_SAMPLES_PASSED);
    glDeleteQueries(1,&q->id);free(q);
}
static int native_visibility_begin(void *host,void *query) {
    (void)host;NativeVisibility *q=query;if(q->active)return 0;
    glBeginQuery(GL_SAMPLES_PASSED,q->id);q->active=1;q->ended=0;return 1;
}
static int native_visibility_end(void *host,void *query) {
    (void)host;NativeVisibility *q=query;if(!q->active)return 0;
    glEndQuery(GL_SAMPLES_PASSED);q->active=0;q->ended=1;return 1;
}
static int native_visibility_result(void *host,void *query,uint64_t *pixels) {
    (void)host;NativeVisibility *q=query;GLuint ready=0;GLuint64 count=0;
    if(!q->ended)return -1;
    glGetQueryObjectuiv(q->id,GL_QUERY_RESULT_AVAILABLE,&ready);if(!ready)return 0;
    glGetQueryObjectui64v(q->id,GL_QUERY_RESULT,&count);*pixels=count;return 1;
}
static const TsfpVisibilityOps native_visibility_raw_ops={NULL,native_visibility_create,native_visibility_destroy,native_visibility_begin,native_visibility_end,native_visibility_result};
static TsfpVisibilityDrawScope native_visibility_draws={NULL,&native_visibility_raw_ops};
static const TsfpVisibilityOps native_visibility_ops={&native_visibility_draws,
    tsfp_visibility_draw_create,tsfp_visibility_draw_destroy,
    tsfp_visibility_draw_query_begin,tsfp_visibility_draw_query_end,tsfp_visibility_draw_result};
static void native_visibility_guest_draw_begin(void){tsfp_visibility_draw_begin(&native_visibility_draws);}
static void native_visibility_guest_draw_end(void){tsfp_visibility_draw_end(&native_visibility_draws);}
int xbox_D3D8VisibilityBegin(void) {
    NATIVE_GL_SCOPE;
    double scale=native_scene_active && g.backbuf_w && g.backbuf_h
        ?(double)native_scene_w/g.backbuf_w*native_scene_h/g.backbuf_h:1;
    return tsfp_visibility_begin(&native_visibility,&native_visibility_ops,scale);
}
void xbox_D3D8VisibilityEnd(uint32_t index,int accepted) {
    NATIVE_GL_SCOPE;(void)tsfp_visibility_end(&native_visibility,&native_visibility_ops,index,accepted);
}
int xbox_D3D8VisibilityResult(uint32_t index,uint32_t *pixels) {
    NATIVE_GL_SCOPE;return tsfp_visibility_result(&native_visibility,&native_visibility_ops,index,pixels);
}
static void native_visibility_reset(void) {
    tsfp_visibility_reset(&native_visibility,&native_visibility_ops);
}
