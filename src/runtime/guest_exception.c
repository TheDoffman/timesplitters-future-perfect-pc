#include "guest_exception.h"
#include <string.h>

typedef struct exception_walk {
    tsfp_exception_ops ops;
    tsfp_exception_request request;
    unsigned char *head,*record,*context,*guard,*dispatcher;
    uint32_t record_address,initial_head;
    uint64_t budget;
    tsfp_x86_context_state baseline;
} exception_walk;

static uint32_t word(const unsigned char *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put(unsigned char *p,uint32_t value) {
    unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(value>>(8*i));
}
static int span(uint32_t address,uint32_t bytes) {
    return address && (uint64_t)address+bytes<=UINT64_C(0x100000000);
}
static int overlap(uint32_t a,uint32_t an,uint32_t b,uint32_t bn) {
    return (uint64_t)a+an>b && (uint64_t)b+bn>a;
}
static int frame_bounds(const exception_walk *w,uint32_t address) {
    return !(address&3u) && address>=w->request.stack_low &&
        (uint64_t)address+8<=w->request.stack_high &&
        (uint64_t)w->request.guard+TSFP_EXCEPTION_GUARD_BYTES<=address;
}
static int workspace_overlap(const exception_walk *w,uint32_t address,uint32_t bytes) {
    return overlap(address,bytes,w->request.head,4) ||
        overlap(address,bytes,w->record_address,TSFP_EXCEPTION_RECORD_BYTES) ||
        overlap(address,bytes,w->request.context,TSFP_CONTEXT_BYTES) ||
        overlap(address,bytes,w->request.guard,TSFP_EXCEPTION_GUARD_BYTES) ||
        overlap(address,bytes,w->request.dispatcher,4);
}
static unsigned char *frame(const exception_walk *w,uint32_t address) {
    if(!frame_bounds(w,address) || workspace_overlap(w,address,8))return NULL;
    return w->ops.resolve(w->ops.opaque,address,8,0);
}
static int prepare(exception_walk *w,const tsfp_exception_ops *ops,
                   const tsfp_exception_request *request,int unwind,
                   tsfp_x86_context_state *out) {
    uint32_t addresses[5],sizes[5],flags;unsigned i,j;
    tsfp_x86_context_state initial;
    if(!ops || !ops->resolve || !ops->handler || !ops->invoke || !request || !request->baseline || !out)return 0;
    memset(w,0,sizeof *w);w->ops=*ops;w->request=*request;w->baseline=*request->baseline;
    w->record_address=request->record?request->record:unwind?request->fallback_record:0;
    if(request->stack_low>=request->stack_high || request->stack_high>UINT64_C(0x100000000) ||
       request->guard&3u || request->guard<request->stack_low ||
       (uint64_t)request->guard+TSFP_EXCEPTION_GUARD_BYTES>request->stack_high)return 0;
    addresses[0]=request->head;sizes[0]=4;
    addresses[1]=w->record_address;sizes[1]=TSFP_EXCEPTION_RECORD_BYTES;
    addresses[2]=request->context;sizes[2]=TSFP_CONTEXT_BYTES;
    addresses[3]=request->guard;sizes[3]=TSFP_EXCEPTION_GUARD_BYTES;
    addresses[4]=request->dispatcher;sizes[4]=4;
    for(i=0;i<5;++i) {
        if(!span(addresses[i],sizes[i]))return 0;
        for(j=0;j<i;++j)if(overlap(addresses[i],sizes[i],addresses[j],sizes[j]))return 0;
    }
    w->head=ops->resolve(ops->opaque,addresses[0],sizes[0],1);
    w->record=ops->resolve(ops->opaque,addresses[1],sizes[1],1);
    w->context=ops->resolve(ops->opaque,addresses[2],sizes[2],1);
    w->guard=ops->resolve(ops->opaque,addresses[3],sizes[3],1);
    w->dispatcher=ops->resolve(ops->opaque,addresses[4],sizes[4],1);
    if(!w->head || !w->record || !w->context || !w->guard || !w->dispatcher ||
       !request->guard_handler || !ops->handler(ops->opaque,request->guard_handler))return 0;
    initial=w->baseline;
    flags=tsfp_x86_context_decode(&initial,w->context,TSFP_CONTEXT_BYTES);
    if(!flags || (flags&3u)!=3u || !initial.r_eip || initial.r_esp<request->stack_low ||
       (uint64_t)initial.r_esp+4>request->stack_high)return 0;
    if(request->record && word(w->record+16)>15)return 0;
    w->baseline=initial;
    w->initial_head=word(w->head);w->budget=(request->stack_high-request->stack_low)/8+1;
    return 1;
}
static int finish(exception_walk *w,tsfp_x86_context_state *out,int result) {
    tsfp_x86_context_state state;uint32_t flags;
    state=w->baseline;
    flags=tsfp_x86_context_decode(&state,w->context,TSFP_CONTEXT_BYTES);
    if(!flags || !state.r_eip || state.r_esp<w->request.stack_low ||
       (uint64_t)state.r_esp+4>w->request.stack_high)return TSFP_EXCEPTION_BAD_CONTEXT;
    *out=state;return result;
}
static int invoke(exception_walk *w,uint32_t registration,uint32_t handler,int unwind,
                  uint32_t *disposition) {
    uint32_t saved=word(w->head);int called;
    if(!handler || !w->ops.handler(w->ops.opaque,handler))return TSFP_EXCEPTION_BAD_HANDLER;
    put(w->guard,saved);put(w->guard+4,w->request.guard_handler);
    put(w->guard+8,registration);put(w->guard+12,(uint32_t)unwind);
    put(w->dispatcher,0);put(w->head,w->request.guard);
    called=w->ops.invoke(w->ops.opaque,handler,w->record_address,registration,
                         w->request.context,w->request.dispatcher,disposition);
    /* Never resurrect an abandoned registration after an unexpected chain edit. */
    if(word(w->head)!=w->request.guard)return TSFP_EXCEPTION_BAD_CHAIN;
    put(w->head,saved);
    return called?1:TSFP_EXCEPTION_CALLBACK_FAILED;
}
int tsfp_exception_raise(const tsfp_exception_ops *ops,const tsfp_exception_request *request,
                         tsfp_x86_context_state *continuation) {
    exception_walk w;uint32_t current,nested=0;
    if(!prepare(&w,ops,request,0,continuation))return TSFP_EXCEPTION_BAD_ARGUMENT;
    put(w.record+12,word(w.context+184));current=w.initial_head;
    while(current!=UINT32_MAX) {
        unsigned char *node;uint32_t handler,next,disposition=UINT32_MAX;int result;
        if(!w.budget-- || !(node=frame(&w,current))) {
            put(w.record+4,word(w.record+4)|TSFP_EXCEPTION_STACK_INVALID);
            return TSFP_EXCEPTION_BAD_CHAIN;
        }
        handler=word(node+4);result=invoke(&w,current,handler,0,&disposition);
        if(result!=1)return result;
        if(current==nested){put(w.record+4,word(w.record+4)&~TSFP_EXCEPTION_NESTED_CALL);nested=0;}
        if(disposition==TSFP_EXCEPTION_CONTINUE) {
            if(word(w.record+4)&TSFP_EXCEPTION_NONCONTINUABLE)return TSFP_EXCEPTION_CANNOT_CONTINUE;
            return finish(&w,continuation,TSFP_EXCEPTION_CONTINUED);
        }
        if(disposition==TSFP_EXCEPTION_NESTED) {
            uint32_t boundary=word(w.dispatcher);
            if(boundary<current || !frame(&w,boundary))return TSFP_EXCEPTION_BAD_CHAIN;
            if(boundary>nested)nested=boundary;
            put(w.record+4,word(w.record+4)|TSFP_EXCEPTION_NESTED_CALL);
        } else if(disposition!=TSFP_EXCEPTION_SEARCH)return TSFP_EXCEPTION_BAD_DISPOSITION;
        next=word(node);
        if(next!=UINT32_MAX && ((uint64_t)current+8>next)) {
            put(w.record+4,word(w.record+4)|TSFP_EXCEPTION_STACK_INVALID);
            return TSFP_EXCEPTION_BAD_CHAIN;
        }
        current=next;
    }
    return TSFP_EXCEPTION_UNHANDLED;
}
int tsfp_exception_unwind(const tsfp_exception_ops *ops,const tsfp_exception_request *request,
                          uint32_t target,uint32_t value,tsfp_x86_context_state *continuation) {
    exception_walk w;uint32_t current;
    if(!prepare(&w,ops,request,1,continuation))return TSFP_EXCEPTION_BAD_ARGUMENT;
    if(target && target!=UINT32_MAX && !frame(&w,target))return TSFP_EXCEPTION_BAD_TARGET;
    if(!request->record) {
        memset(w.record,0,TSFP_EXCEPTION_RECORD_BYTES);put(w.record,UINT32_C(0xc0000027));
        put(w.record+12,word(w.context+184));
    }
    put(w.record+4,word(w.record+4)|TSFP_EXCEPTION_UNWINDING|(target?0:TSFP_EXCEPTION_EXIT_UNWIND));
    put(w.context+176,value);current=w.initial_head;
    while(current!=target && current!=UINT32_MAX) {
        unsigned char *node;uint32_t next,disposition=UINT32_MAX;int result;
        if(!w.budget-- || !(node=frame(&w,current)))return TSFP_EXCEPTION_BAD_CHAIN;
        if(target && target!=UINT32_MAX && current>target)return TSFP_EXCEPTION_BAD_TARGET;
        result=invoke(&w,current,word(node+4),1,&disposition);
        if(result!=1)return result;
        if(disposition==TSFP_EXCEPTION_COLLIDED) {
            uint32_t collision=word(w.dispatcher);
            if(collision<current || !(node=frame(&w,collision)))return TSFP_EXCEPTION_BAD_CHAIN;
            if(target && target!=UINT32_MAX && collision>=target)return TSFP_EXCEPTION_BAD_TARGET;
            current=collision;
        } else if(disposition!=TSFP_EXCEPTION_SEARCH)return TSFP_EXCEPTION_BAD_DISPOSITION;
        next=word(node);
        if(next!=UINT32_MAX && (uint64_t)current+8>next)return TSFP_EXCEPTION_BAD_CHAIN;
        put(w.head,next);current=next;
    }
    if(target && current!=target)return TSFP_EXCEPTION_BAD_TARGET;
    return finish(&w,continuation,TSFP_EXCEPTION_UNWOUND);
}
