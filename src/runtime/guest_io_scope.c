#include "guest_io_scope.h"
#include <string.h>
static int finish(void *context,int abandoned) {
    tsfp_io_scope *frame=(tsfp_io_scope *)context;unsigned kind;
    if(!frame->active)return 1;
    frame->active=0;kind=frame->ticket.kind;
    return (kind==1 || kind==3 || kind==4 || kind==5)?tsfp_io_owner_dispatch_end(&frame->ticket,abandoned):tsfp_io_owner_complete_end(&frame->ticket,abandoned);
}
static int begin(tsfp_guest_object_scopes *stack,tsfp_io_scope *frame,tsfp_guest_objects *store,
    tsfp_io_owner *owner,int complete,uint32_t status,uint32_t information) {
    int ok;
    if(!frame || !owner || !tsfp_guest_scope_enter(stack,&frame->scope,store))return 0;
    frame->active=0;memset(&frame->ticket,0,sizeof(frame->ticket));
    if(!tsfp_guest_scope_on_leave(stack,&frame->scope,finish,frame))goto failed;
    if(complete==1)ok=tsfp_io_owner_complete_begin(owner,&frame->ticket,status,information);
    else if(complete==2)ok=tsfp_io_owner_forward_begin(owner,&frame->ticket);
    else ok=tsfp_io_owner_dispatch_begin(owner,&frame->ticket);
    if(ok) {frame->active=1;return 1;}
failed:
    (void)tsfp_guest_scope_leave(stack,&frame->scope);return 0;
}
int tsfp_io_dispatch_scope_begin(tsfp_guest_object_scopes *s,tsfp_io_scope *f,tsfp_guest_objects *store,tsfp_io_owner *o) {return begin(s,f,store,o,0,0,0);}
int tsfp_io_forward_scope_begin(tsfp_guest_object_scopes *s,tsfp_io_scope *f,tsfp_guest_objects *store,tsfp_io_owner *o) {return begin(s,f,store,o,2,0,0);}
int tsfp_io_completion_scope_begin(tsfp_guest_object_scopes *s,tsfp_io_scope *f,tsfp_guest_objects *store,tsfp_io_owner *o,uint32_t status,uint32_t info) {return begin(s,f,store,o,1,status,info);}
int tsfp_io_scope_end(tsfp_guest_object_scopes *s,tsfp_io_scope *f) {return f && tsfp_guest_scope_leave(s,&f->scope);}

int tsfp_io_dispatch_scope_publish(tsfp_guest_object_scopes *s,tsfp_io_scope *f) {
    return s && f && s->current==&f->scope && f->active && tsfp_io_owner_dispatch_publish(&f->ticket);
}
int tsfp_io_dispatch_scope_cancel(tsfp_guest_object_scopes *s,tsfp_io_scope *f) {
    int ok;
    if(!s || !f || s->current!=&f->scope || !f->active || (f->ticket.kind!=1 && f->ticket.kind!=4))return 0;
    ok=tsfp_io_owner_dispatch_rollback(&f->ticket);
    if(ok)f->active=0;else f->scope.abandoned=1;
    return tsfp_guest_scope_leave(s,&f->scope) && ok;
}
