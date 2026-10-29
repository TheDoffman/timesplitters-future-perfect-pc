#include "guest_symlinks.h"
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
struct tsfp_symlink_node {struct tsfp_symlink_node *next;size_t name_bytes,target_bytes;};
static void enter(tsfp_guest_symlinks *s) {
#if defined(_MSC_VER)
    while(_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while(__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_guest_symlinks *s) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int valid(const void *p,size_t n) {return p && n && n<=UINT16_MAX && (uintptr_t)p<=UINTPTR_MAX-(n-1);}
static int matches(const tsfp_symlink_node *p,const void *name,size_t n) {
    return p->name_bytes==n && !memcmp(p+1,name,n);
}
int tsfp_symlinks_init(tsfp_guest_symlinks *s,size_t limit,
    void *(*allocate)(void *,size_t),void (*dispose)(void *,void *),void *context) {
    if(!s || !limit || !allocate || !dispose)return 0;
    memset(s,0,sizeof(*s));s->limit=limit;s->allocate=allocate;s->dispose=dispose;s->context=context;return 1;
}
int tsfp_symlinks_create(tsfp_guest_symlinks *s,const void *name,size_t n,const void *target,size_t bytes) {
    tsfp_symlink_node *node,*p;int result=TSFP_SYMLINK_OK;
    if(!s || !valid(name,n) || !valid(target,bytes))return TSFP_SYMLINK_INVALID;
    node=s->allocate(s->context,sizeof(*node)+n+bytes);
    if(!node)return TSFP_SYMLINK_RESOURCE;
    node->name_bytes=n;node->target_bytes=bytes;
    memcpy(node+1,name,n);memcpy((unsigned char *)(node+1)+n,target,bytes);
    enter(s);
    if(s->closed)result=TSFP_SYMLINK_CLOSED;
    else {
        for(p=s->head;p;p=p->next)if(matches(p,node+1,n)) {result=TSFP_SYMLINK_COLLISION;break;}
        if(result==TSFP_SYMLINK_OK) {
            if(s->count>=s->limit)result=TSFP_SYMLINK_RESOURCE;
            else {node->next=s->head;s->head=node;++s->count;}
        }
    }
    leave(s);if(result!=TSFP_SYMLINK_OK)s->dispose(s->context,node);return result;
}
int tsfp_symlinks_remove(tsfp_guest_symlinks *s,const void *name,size_t n) {
    tsfp_symlink_node **link,*node=NULL;int result=TSFP_SYMLINK_MISSING;
    if(!s || !valid(name,n))return TSFP_SYMLINK_INVALID;
    enter(s);
    if(s->closed)result=TSFP_SYMLINK_CLOSED;
    else for(link=&s->head;*link;link=&(*link)->next)if(matches(*link,name,n)) {
        node=*link;*link=node->next;--s->count;result=TSFP_SYMLINK_OK;break;
    }
    leave(s);if(node)s->dispose(s->context,node);return result;
}
int tsfp_symlinks_copy(tsfp_guest_symlinks *s,const void *name,size_t n,void *output,size_t capacity,size_t *length) {
    tsfp_symlink_node *p;int result=TSFP_SYMLINK_MISSING;
    if(!s || !valid(name,n) || !output || !length)return TSFP_SYMLINK_INVALID;
    enter(s);
    if(s->closed)result=TSFP_SYMLINK_CLOSED;
    else for(p=s->head;p;p=p->next)if(matches(p,name,n)) {
        if(capacity<p->target_bytes)result=TSFP_SYMLINK_RESOURCE;
        else {memcpy(output,(const unsigned char *)(p+1)+n,p->target_bytes);*length=p->target_bytes;result=TSFP_SYMLINK_OK;}
        break;
    }
    leave(s);return result;
}
size_t tsfp_symlinks_stop(tsfp_guest_symlinks *s) {
    tsfp_symlink_node *p,*next;size_t count;if(!s)return 0;
    enter(s);s->closed=1;p=s->head;count=s->count;s->head=NULL;s->count=0;leave(s);
    for(;p;p=next) {next=p->next;s->dispose(s->context,p);}
    return count;
}
