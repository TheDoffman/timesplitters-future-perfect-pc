#ifndef TSFP_VISIBILITY_DRAW_SCOPE_H
#define TSFP_VISIBILITY_DRAW_SCOPE_H
#include "visibility_queries.h"
#include <stdlib.h>

/* A title query counts its submitted geometry, not host feedback/resolve draws
 * needed to prepare it. Each guest draw owns one native segment; internal host
 * work between draws stays outside all segments. Preserve asynchronous results. */
#define TSFP_VISIBILITY_DRAW_PARTS 64u
typedef struct TsfpVisibilityDrawQuery {
    void *parts[TSFP_VISIBILITY_DRAW_PARTS];
    unsigned count;
    int drawing,ended,failed;
} TsfpVisibilityDrawQuery;
typedef struct TsfpVisibilityDrawScope {
    TsfpVisibilityDrawQuery *active;
    const TsfpVisibilityOps *native;
} TsfpVisibilityDrawScope;
static inline void *tsfp_visibility_draw_create(void *host) {
    (void)host;return calloc(1,sizeof(TsfpVisibilityDrawQuery));
}
static inline void tsfp_visibility_draw_destroy(void *host,void *query) {
    TsfpVisibilityDrawScope *s=host;TsfpVisibilityDrawQuery *q=query;
    if(q->drawing)s->native->end(s->native->host,q->parts[q->count-1]);
    if(s->active==q)s->active=NULL;
    for(unsigned i=0;i<TSFP_VISIBILITY_DRAW_PARTS;i++)
        if(q->parts[i])s->native->destroy(s->native->host,q->parts[i]);
    free(q);
}
static inline int tsfp_visibility_draw_query_begin(void *host,void *query) {
    TsfpVisibilityDrawScope *s=host;TsfpVisibilityDrawQuery *q=query;
    if(s->active)return 0;
    q->count=0;q->drawing=q->ended=q->failed=0;s->active=q;return 1;
}
static inline void tsfp_visibility_draw_begin(TsfpVisibilityDrawScope *s) {
    TsfpVisibilityDrawQuery *q=s->active;
    if(!q)return;
    if(q->drawing || q->count==TSFP_VISIBILITY_DRAW_PARTS){q->failed=1;return;}
    void **part=&q->parts[q->count];
    if(!*part)*part=s->native->create(s->native->host);
    if(!*part || !s->native->begin(s->native->host,*part)){q->failed=1;return;}
    q->count++;q->drawing=1;
}
static inline void tsfp_visibility_draw_end(TsfpVisibilityDrawScope *s) {
    TsfpVisibilityDrawQuery *q=s->active;
    if(!q || !q->drawing)return;
    if(!s->native->end(s->native->host,q->parts[q->count-1]))q->failed=1;
    q->drawing=0;
}
static inline int tsfp_visibility_draw_query_end(void *host,void *query) {
    TsfpVisibilityDrawScope *s=host;TsfpVisibilityDrawQuery *q=query;
    if(s->active!=q)return 0;
    if(q->drawing){tsfp_visibility_draw_end(s);q->failed=1;}
    s->active=NULL;q->ended=1;return !q->failed;
}
static inline int tsfp_visibility_draw_result(void *host,void *query,uint64_t *samples) {
    TsfpVisibilityDrawScope *s=host;TsfpVisibilityDrawQuery *q=query;uint64_t total=0;
    if(!q->ended || q->failed)return -1;
    for(unsigned i=0;i<q->count;i++) {
        uint64_t value=0;int ready=s->native->result(s->native->host,q->parts[i],&value);
        if(ready!=1)return ready;
        if(UINT64_MAX-total<value)return -1;
        total+=value;
    }
    *samples=total;return 1;
}
#endif
