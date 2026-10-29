#include "guest_object_names.h"
#include <string.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
struct tsfp_guest_name_node {
    struct tsfp_guest_name_node *next;
    uint64_t key;
    size_t length;
};
static void enter(tsfp_guest_names *s) {
#if defined(_MSC_VER)
    while(_InterlockedCompareExchange(&s->lock,1,0)) { }
#else
    while(__atomic_test_and_set(&s->lock,__ATOMIC_ACQUIRE)) { }
#endif
}
static void leave(tsfp_guest_names *s) {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(&s->lock,0);
#else
    __atomic_clear(&s->lock,__ATOMIC_RELEASE);
#endif
}
static int valid(const void *name,size_t length,unsigned flags) {
    return name && length && length<=UINT16_MAX && !(flags&~1u) &&
        (uintptr_t)name<=UINTPTR_MAX-(length-1);
}
static unsigned char fold(unsigned char c) {
    return c>='a' && c<='z'?(unsigned char)(c-('a'-'A')):c;
}
static int equal(const tsfp_guest_name_node *node,const void *name,size_t length,unsigned flags) {
    const unsigned char *left=(const unsigned char *)(node+1),*right=name;
    size_t i;
    if(node->length!=length) return 0;
    for(i=0;i<length;++i) {
        unsigned char a=left[i],b=right[i];
        if(flags&TSFP_NAME_IGNORE_ASCII_CASE) { a=fold(a);b=fold(b); }
        if(a!=b) return 0;
    }
    return 1;
}
int tsfp_guest_names_init(tsfp_guest_names *s,tsfp_guest_objects *objects,size_t limit,
    void *(*allocate)(void *,size_t),void (*dispose)(void *,void *),void *context) {
    if(!s || !objects || !limit || !allocate || !dispose) return 0;
    memset(s,0,sizeof(*s));s->objects=objects;s->limit=limit;
    s->allocate=allocate;s->dispose=dispose;s->context=context;return 1;
}
int tsfp_guest_names_bind(tsfp_guest_names *s,const void *name,size_t length,unsigned flags,uint64_t key) {
    tsfp_guest_name_node *node,*other;int status=TSFP_NAME_OK;
    if(!s || !valid(name,length,flags)) return TSFP_NAME_INVALID;
    enter(s);
    if(s->closed) { leave(s);return TSFP_NAME_CLOSED; }
    leave(s);
    node=s->allocate(s->context,sizeof(*node)+length);
    if(!node) return TSFP_NAME_ALLOCATION;
    node->key=key;node->length=length;memcpy(node+1,name,length);
    enter(s);
    if(s->closed) status=TSFP_NAME_CLOSED;
    else {
        for(other=s->head;other;other=other->next)
            if(equal(other,node+1,length,flags)) { status=TSFP_NAME_COLLISION;break; }
        if(status==TSFP_NAME_OK && s->count==s->limit) status=TSFP_NAME_CAPACITY;
        /* Retain takes only the object metadata lock and invokes no callbacks. */
        if(status==TSFP_NAME_OK && !tsfp_guest_objects_retain(s->objects,key)) status=TSFP_NAME_OBJECT;
        if(status==TSFP_NAME_OK) { node->next=s->head;s->head=node;++s->count; }
    }
    leave(s);
    if(status!=TSFP_NAME_OK) s->dispose(s->context,node);
    return status;
}
int tsfp_guest_names_lookup(tsfp_guest_names *s,const void *name,size_t length,unsigned flags,uint64_t *key) {
    tsfp_guest_name_node *node;int status=TSFP_NAME_MISSING;
    if(!s || !key || !valid(name,length,flags)) return TSFP_NAME_INVALID;
    enter(s);
    if(s->closed) status=TSFP_NAME_CLOSED;
    else for(node=s->head;node;node=node->next) if(equal(node,name,length,flags)) {
        if(tsfp_guest_objects_retain(s->objects,node->key)) { *key=node->key;status=TSFP_NAME_OK; }
        else status=TSFP_NAME_OBJECT;
        break;
    }
    leave(s);return status;
}
static void release_node(tsfp_guest_names *s,tsfp_guest_name_node *node) {
    uint64_t key=node->key;
    s->dispose(s->context,node);
    (void)tsfp_guest_objects_dereference(s->objects,key);
}
int tsfp_guest_names_remove(tsfp_guest_names *s,const void *name,size_t length,unsigned flags,uint64_t expected_key) {
    tsfp_guest_name_node **link,*node=NULL;int status=TSFP_NAME_MISSING;
    if(!s || !valid(name,length,flags)) return TSFP_NAME_INVALID;
    enter(s);
    if(s->closed) status=TSFP_NAME_CLOSED;
    else for(link=&s->head;*link;link=&(*link)->next) if(equal(*link,name,length,flags)) {
        if(expected_key && expected_key!=(*link)->key) break;
        node=*link;*link=node->next;--s->count;status=TSFP_NAME_OK;break;
    }
    leave(s);
    if(node) release_node(s,node);
    return status;
}
size_t tsfp_guest_names_stop(tsfp_guest_names *s) {
    tsfp_guest_name_node *nodes;size_t count;
    if(!s) return 0;
    enter(s);s->closed=1;nodes=s->head;count=s->count;s->head=NULL;s->count=0;leave(s);
    while(nodes) { tsfp_guest_name_node *next=nodes->next;release_node(s,nodes);nodes=next; }
    return count;
}
