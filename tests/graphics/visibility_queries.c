#include "graphics/visibility_queries.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct Fake {int active,ended,ready,failed;uint64_t count;} Fake;
static unsigned created,destroyed;
static void *create(void *host){(void)host;created++;return calloc(1,sizeof(Fake));}
static void destroy(void *host,void *q){(void)host;destroyed++;free(q);}
static int begin(void *host,void *q){(void)host;Fake *f=q;assert(!f->active);f->active=1;f->ended=0;f->ready=f->failed=0;return 1;}
static int end(void *host,void *q){(void)host;Fake *f=q;assert(f->active);f->active=0;f->ended=1;return 1;}
static int result(void *host,void *q,uint64_t *count){(void)host;Fake *f=q;assert(f->ended);if(f->failed)return -1;if(!f->ready)return 0;*count=f->count;return 1;}
static TsfpVisibilityQueries state;
static TsfpVisibilityOps ops={NULL,create,destroy,begin,end,result};
static Fake *pending(unsigned index){assert(state.entries[index].pending);return state.slots[state.entries[index].pending-1].query;}
int main(void) {
    uint32_t count=77;
    assert(!tsfp_visibility_begin(&state,&ops,NAN));
    assert(!tsfp_visibility_begin(&state,&ops,0));
    assert(tsfp_visibility_result(&state,&ops,0,&count)==-1 && count==77);
    assert(tsfp_visibility_begin(&state,&ops,4));
    assert(!tsfp_visibility_begin(&state,&ops,1));
    assert(tsfp_visibility_end(&state,&ops,0,1));
    assert(tsfp_visibility_result(&state,&ops,0,&count)==0 && count==77);
    pending(0)->ready=1;pending(0)->count=2048;
    assert(tsfp_visibility_result(&state,&ops,0,&count)==1 && count==512);
    assert(tsfp_visibility_result(&state,&ops,0,&count)==1 && count==512);
    assert(created==1); /* Native object is reused, while old ID stays cached. */
    assert(tsfp_visibility_begin(&state,&ops,1));assert(tsfp_visibility_end(&state,&ops,1,1));
    pending(1)->ready=1;pending(1)->count=0;
    assert(tsfp_visibility_result(&state,&ops,1,&count)==1 && count==0);
    assert(tsfp_visibility_result(&state,&ops,0,&count)==1 && count==512);
    assert(!tsfp_visibility_end(&state,&ops,0,1)); /* Failed new begin invalidates old answer. */
    count=77;assert(tsfp_visibility_result(&state,&ops,0,&count)==-1 && count==77);
    for(unsigned i=0;i<TSFP_VISIBILITY_SLOTS;i++) {
        assert(tsfp_visibility_begin(&state,&ops,1));assert(tsfp_visibility_end(&state,&ops,i,1));
    }
    assert(!tsfp_visibility_begin(&state,&ops,1));
    pending(3)->failed=1;assert(tsfp_visibility_result(&state,&ops,3,&count)==-1 && count==77);
    assert(tsfp_visibility_begin(&state,&ops,1));assert(tsfp_visibility_end(&state,&ops,3,1));
    pending(3)->count=UINT64_MAX;pending(3)->ready=1;
    assert(tsfp_visibility_result(&state,&ops,3,&count)==1 && count==UINT32_MAX);
    /* Replacing a pending reused ID retires its old object until GPU completion. */
    Fake *old=pending(4);
    assert(tsfp_visibility_begin(&state,&ops,1));assert(tsfp_visibility_end(&state,&ops,4,1));
    old->ready=1;pending(4)->ready=1;pending(4)->count=12;
    assert(tsfp_visibility_result(&state,&ops,4,&count)==1 && count==12);
    assert(tsfp_visibility_begin(&state,&ops,1));assert(!tsfp_visibility_end(&state,&ops,0,0));
    tsfp_visibility_reset(&state,&ops);assert(created==destroyed);
    assert(!state.active && tsfp_visibility_result(&state,&ops,4,&count)==-1);
    /* A title may abandon a pending ring entry. Reap completed native work
     * on a new begin even if that old ID is never polled again. */
    for(unsigned i=0;i<TSFP_VISIBILITY_SLOTS;i++) {
        assert(tsfp_visibility_begin(&state,&ops,1));assert(tsfp_visibility_end(&state,&ops,i,1));
    }
    for(unsigned i=0;i<TSFP_VISIBILITY_SLOTS;i++){pending(i)->ready=1;pending(i)->count=i+1;}
    assert(tsfp_visibility_begin(&state,&ops,1));assert(tsfp_visibility_end(&state,&ops,100,1));
    assert(tsfp_visibility_result(&state,&ops,0,&count)==1 && count==1);
    tsfp_visibility_reset(&state,&ops);assert(created==destroyed);
    puts("asynchronous visibility cache, zero/pending/error, bounded reuse and reset passed");return 0;
}
