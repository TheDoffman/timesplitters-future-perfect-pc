#include "guest_device_factory.h"
#include <string.h>
enum {BUILDING,LIVE,DELETING,DISPOSING,UNPUBLISHED_FAILED,PUBLISHED_FAILED,FINAL_WAITING,FINAL_ACTIVE,FINAL_READY,FINAL_UNCERTAIN};
struct tsfp_device_node {
    tsfp_device_node *next;uint64_t key;uint32_t body,bytes;void *mapping;
    uint16_t name_length;unsigned phase,creator,named,exposed;uint32_t driver;int cleanup;size_t final_pins;
};
static unsigned char *name_bytes(tsfp_device_node *n) {return (unsigned char *)(n+1);}
static void lock(tsfp_device_factory *f) {f->ops.lock(f->ops.context);}
static void unlock(tsfp_device_factory *f) {f->ops.unlock(f->ops.context);}
static uint32_t word(const unsigned char *p) {return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static int span(uint32_t a,uint32_t n) {return a && n && a<=UINT32_MAX-(n-1);}
static tsfp_device_node *find(tsfp_device_factory *f,uint32_t body) {
    tsfp_device_node *n;for(n=f->head;n;n=n->next)if(n->body==body)return n;return NULL;
}
static int closed(tsfp_device_factory *f) {int yes;lock(f);yes=f->closed;unlock(f);return yes;}
static void drop_metadata(tsfp_device_factory *f,tsfp_device_node *n) {
    tsfp_device_node **link;lock(f);
    for(link=&f->head;*link && *link!=n;link=&(*link)->next) { }
    if(*link)*link=n->next;
    --f->live;unlock(f);f->ops.metadata_free(f->ops.context,n);
}
static int cleanup(tsfp_device_factory *f,tsfp_device_node *n,int published) {
    int result=f->ops.deallocate(f->ops.context,n->body);
    if(result==1) {drop_metadata(f,n);return 1;}
    lock(f);n->phase=published?PUBLISHED_FAILED:UNPUBLISHED_FAILED;
    n->cleanup=result==0?0:-1;unlock(f);return result==0?0:-1;
}
int tsfp_device_factory_init(tsfp_device_factory *f,tsfp_guest_objects *objects,tsfp_guest_names *names,
    uint32_t prefix,size_t limit,const tsfp_device_factory_ops *ops) {
    tsfp_device_factory candidate;
    if(!f || !objects || !objects->free_dispatcher || !names || names->objects!=objects ||
       !prefix || !limit || limit>objects->capacity || !ops || !ops->lock || !ops->unlock ||
       !ops->metadata_allocate || !ops->metadata_free || !ops->allocate || !ops->deallocate ||
       !ops->input || !ops->output || !ops->initialize)return 0;
    memset(&candidate,0,sizeof(candidate));candidate.objects=objects;candidate.names=names;
    candidate.ops=*ops;candidate.prefix_bytes=prefix;candidate.limit=limit;*f=candidate;return 1;
}
static int retire(tsfp_device_factory *f,tsfp_device_node *n) {
    int named;uint64_t key;
    /* Caller owns a body lease, so callbacks/removal cannot reclaim n. */
    lock(f);
    if(n->phase!=LIVE || !n->creator) {unlock(f);return 0;}
    n->creator=0;n->phase=DELETING;named=(int)n->named;n->named=0;key=n->key;unlock(f);
    if(f->ops.mark_deleted)f->ops.mark_deleted(f->ops.context,n->body,n->mapping,n->bytes);
    if(named)(void)tsfp_guest_names_remove(f->names,name_bytes(n),n->name_length,TSFP_NAME_IGNORE_ASCII_CASE,key);
    (void)tsfp_guest_objects_dereference(f->objects,key);return 1;
}
int tsfp_device_factory_create(tsfp_device_factory *f,uint32_t driver,uint32_t extension,
    uint32_t descriptor,uint32_t type,int exclusive,uint32_t out_address) {
    unsigned char data[8],*target;const void *name=NULL,*source;
    uint32_t length=0,capacity,address,bytes,body;uint64_t key;tsfp_device_node *n;
    tsfp_guest_object_lease lease={0};int result,status;
    if(!f || extension>UINT32_MAX-f->prefix_bytes)return TSFP_DEVICE_BAD_INPUT;
    if(!span(out_address,4) || !(target=f->ops.output(f->ops.context,out_address,4)))return TSFP_DEVICE_BAD_ADDRESS;
    /* Public guest DRIVER_OBJECT table: three callbacks plus14 major entries. */
    if(!span(driver,68) || !f->ops.input(f->ops.context,driver,68))return TSFP_DEVICE_BAD_ADDRESS;
    if(descriptor) {
        if(!span(descriptor,8) || !(source=f->ops.input(f->ops.context,descriptor,8)))return TSFP_DEVICE_BAD_ADDRESS;
        memcpy(data,source,8);length=(uint32_t)data[0]|((uint32_t)data[1]<<8);
        capacity=(uint32_t)data[2]|((uint32_t)data[3]<<8);address=word(data+4);
        if(!length || length>capacity)return TSFP_DEVICE_BAD_INPUT;
        if(!span(address,length) || !(name=f->ops.input(f->ops.context,address,length)))return TSFP_DEVICE_BAD_ADDRESS;
    }
    lock(f);
    if(f->closed || f->live>=f->limit) {result=f->closed?TSFP_DEVICE_CLOSED:TSFP_DEVICE_NO_MEMORY;unlock(f);return result;}
    ++f->live;unlock(f);
    n=f->ops.metadata_allocate(f->ops.context,sizeof(*n)+length);
    if(!n) {lock(f);--f->live;unlock(f);return TSFP_DEVICE_NO_MEMORY;}
    memset(n,0,sizeof(*n));n->name_length=(uint16_t)length;
    if(length)memcpy(name_bytes(n),name,length);
    lock(f);n->next=f->head;f->head=n;unlock(f);
    if(closed(f)) {drop_metadata(f,n);return TSFP_DEVICE_CLOSED;}
    bytes=f->prefix_bytes+extension;body=f->ops.allocate(f->ops.context,bytes);
    if(!body) {drop_metadata(f,n);return TSFP_DEVICE_NO_MEMORY;}
    lock(f);n->body=body;n->bytes=bytes;n->driver=driver;unlock(f);
    if(!span(body,bytes) || !(n->mapping=f->ops.output(f->ops.context,body,bytes))) {
        (void)cleanup(f,n,0);return TSFP_DEVICE_BAD_ADDRESS;
    }
    memset(n->mapping,0,bytes);
    if(!f->ops.initialize(f->ops.context,body,n->mapping,bytes,driver,extension,type,length!=0,exclusive!=0)) {
        (void)cleanup(f,n,0);return TSFP_DEVICE_INITIALIZATION;
    }
    if(closed(f)) {(void)cleanup(f,n,0);return TSFP_DEVICE_CLOSED;}
    if(!tsfp_guest_objects_adopt_body(f->objects,body,TSFP_KIND_DEVICE,0,&key)) {
        (void)cleanup(f,n,0);return TSFP_DEVICE_NO_MEMORY;
    }
    lock(f);n->key=key;n->creator=1;unlock(f);
    if(!tsfp_guest_objects_acquire_key(f->objects,key,&lease)) {
        lock(f);n->creator=0;n->phase=DELETING;unlock(f);
        (void)tsfp_guest_objects_dereference(f->objects,key);return TSFP_DEVICE_NO_MEMORY;
    }
    status=length?tsfp_guest_names_bind(f->names,name_bytes(n),length,TSFP_NAME_IGNORE_ASCII_CASE,key):TSFP_NAME_OK;
    lock(f);n->named=status==TSFP_NAME_OK && length!=0;n->phase=LIVE;result=f->closed;unlock(f);
    if(status!=TSFP_NAME_OK || result) {
        result=result?TSFP_DEVICE_CLOSED:status==TSFP_NAME_COLLISION?TSFP_DEVICE_COLLISION:TSFP_DEVICE_NO_MEMORY;
        (void)retire(f,n);(void)tsfp_guest_objects_release(&lease);return result;
    }
    lock(f);n->exposed=1;unlock(f);
    put(target,body);(void)tsfp_guest_objects_release(&lease);return TSFP_DEVICE_OK;
}
int tsfp_device_factory_has_event(tsfp_device_factory *f,uint32_t address) {
    tsfp_device_node *n;int found;
    if(!f || address<=0x34)return 0;
    lock(f);n=find(f,address-0x34);found=n && n->mapping && n->bytes>=0x44;unlock(f);return found;
}
int tsfp_device_factory_pin(tsfp_device_factory *f,uint32_t body,tsfp_guest_object_lease *out,void **mapping,uint32_t *bytes) {
    uint64_t key;tsfp_device_node *n;tsfp_guest_object_lease lease={0};void *value=NULL;uint32_t span=0;
    if(!f || !out || out->owner || !mapping || !bytes || !body ||
       !tsfp_guest_objects_reference_dispatcher(f->objects,body,&key))return 0;
    if(!tsfp_guest_objects_acquire_key(f->objects,key,&lease)) {
        (void)tsfp_guest_objects_dereference(f->objects,key);return 0;
    }
    (void)tsfp_guest_objects_dereference(f->objects,key);
    if(lease.kind==TSFP_KIND_DEVICE) {
        lock(f);n=find(f,body);
        if(n && n->key==key && n->exposed && (n->phase==LIVE || n->phase==DELETING)) {value=n->mapping;span=n->bytes;}
        unlock(f);
    }
    if(!value) {(void)tsfp_guest_objects_release(&lease);return 0;}
    *out=lease;*mapping=value;*bytes=span;return 1;
}
int tsfp_device_factory_delete(tsfp_device_factory *f,uint32_t body) {
    uint64_t key;tsfp_guest_object_lease lease={0};tsfp_device_node *n;int acquired,result=0;
    if(!f || !tsfp_guest_objects_reference_dispatcher(f->objects,body,&key))return 0;
    acquired=tsfp_guest_objects_acquire_key(f->objects,key,&lease);
    (void)tsfp_guest_objects_dereference(f->objects,key);
    if(!acquired)return 0;
    lock(f);n=find(f,body);if(n && n->key!=key)n=NULL;unlock(f);
    if(n && lease.kind==TSFP_KIND_DEVICE)result=retire(f,n);
    (void)tsfp_guest_objects_release(&lease);return result;
}
int tsfp_device_factory_dispose(tsfp_device_factory *f,uint32_t body,uint32_t kind,uint32_t flags) {
    tsfp_device_node *n;
    if(!f || kind!=TSFP_KIND_DEVICE || flags)return -1;
    lock(f);n=find(f,body);
    if(!n || !n->key || n->phase==DISPOSING || n->phase==UNPUBLISHED_FAILED ||
       (n->phase==PUBLISHED_FAILED && n->cleanup<0)) {unlock(f);return -1;}
    if(n->phase==FINAL_READY && n->final_pins) {unlock(f);return 0;}
    if(n->phase==FINAL_UNCERTAIN) {unlock(f);return -1;}
    if(n->phase==FINAL_WAITING || n->phase==FINAL_ACTIVE) {unlock(f);return 0;}
    if(n->phase==DELETING && n->exposed && f->ops.defer_finalization) {
        n->phase=FINAL_WAITING;unlock(f);return 0;
    }
    n->phase=DISPOSING;unlock(f);return cleanup(f,n,1);
}
void tsfp_device_factory_stop(tsfp_device_factory *f) {
    size_t i;tsfp_device_node *n;uint64_t key;tsfp_guest_object_lease lease;
    if(!f)return;
    lock(f);f->closed=1;unlock(f);
    for(i=0;i<f->limit;++i) {
        key=0;lock(f);for(n=f->head;n;n=n->next)if(n->phase==LIVE && n->creator) {key=n->key;break;}unlock(f);
        if(!key)break;
        memset(&lease,0,sizeof(lease));
        if(!tsfp_guest_objects_acquire_key(f->objects,key,&lease))break;
        (void)tsfp_device_factory_delete(f,lease.dispatcher);(void)tsfp_guest_objects_release(&lease);
    }
}
int tsfp_device_factory_retry_one(tsfp_device_factory *f) {
    tsfp_device_node *n;if(!f)return 0;
    lock(f);for(n=f->head;n;n=n->next)if(n->phase==UNPUBLISHED_FAILED && n->cleanup==0)break;
    if(n)n->phase=DISPOSING;unlock(f);return n?cleanup(f,n,0)==1:0;
}
int tsfp_device_factory_pin_finalization(tsfp_device_factory *f,const tsfp_device_finalization *ticket,
    tsfp_device_final_pin *out,void **mapping,uint32_t *bytes) {
    tsfp_device_node *n;void *value;uint32_t span;
    if(!f || !ticket || !ticket->key || !ticket->body || !out || out->factory || !mapping || !bytes)return 0;
    lock(f);n=find(f,ticket->body);
    if(!n || n->key!=ticket->key || n->driver!=ticket->driver || n->phase!=FINAL_ACTIVE || n->final_pins==SIZE_MAX) {unlock(f);return 0;}
    ++n->final_pins;value=n->mapping;span=n->bytes;unlock(f);
    out->ticket=*ticket;out->factory=f;*mapping=value;*bytes=span;return 1;
}
int tsfp_device_factory_release_finalization(tsfp_device_final_pin *pin) {
    tsfp_device_factory *f;tsfp_device_node *n;
    if(!pin || !(f=pin->factory))return 0;
    lock(f);n=find(f,pin->ticket.body);
    if(!n || n->key!=pin->ticket.key || n->driver!=pin->ticket.driver || !n->final_pins) {unlock(f);return 0;}
    --n->final_pins;pin->factory=NULL;memset(&pin->ticket,0,sizeof(pin->ticket));unlock(f);return 1;
}
size_t tsfp_device_factory_final_pins(tsfp_device_factory *f,const tsfp_device_finalization *ticket) {
    tsfp_device_node *n;size_t count=0;if(!f || !ticket)return 0;
    lock(f);n=find(f,ticket->body);
    if(n && n->key==ticket->key && n->driver==ticket->driver)count=n->final_pins;
    unlock(f);return count;
}
int tsfp_device_factory_take_finalization(tsfp_device_factory *f,tsfp_device_finalization *ticket) {
    tsfp_device_node *n;tsfp_device_finalization value;
    if(!f || !ticket)return 0;
    lock(f);for(n=f->head;n;n=n->next)if(n->phase==FINAL_WAITING)break;
    if(!n) {unlock(f);return 0;}
    value.key=n->key;value.body=n->body;value.driver=n->driver;
    n->phase=FINAL_ACTIVE;unlock(f);*ticket=value;return 1;
}
int tsfp_device_factory_finish_finalization(tsfp_device_factory *f,const tsfp_device_finalization *ticket,int completed) {
    tsfp_device_node *n;
    if(!f || !ticket || !ticket->key || !ticket->body)return 0;
    lock(f);n=find(f,ticket->body);
    if(!n || n->key!=ticket->key || n->driver!=ticket->driver || n->phase!=FINAL_ACTIVE) {unlock(f);return 0;}
    n->phase=completed?FINAL_READY:FINAL_UNCERTAIN;unlock(f);return 1;
}
void tsfp_device_factory_counts(tsfp_device_factory *f,size_t *live,size_t *retained,size_t *uncertain) {
    tsfp_device_node *n;size_t a=0,b=0;if(!f || !live || !retained || !uncertain)return;
    lock(f);for(n=f->head;n;n=n->next)if(n->phase==UNPUBLISHED_FAILED || n->phase==PUBLISHED_FAILED || n->phase==FINAL_UNCERTAIN) {
        if(n->phase!=FINAL_UNCERTAIN && n->cleanup==0)++a;else ++b;
    }
    *live=f->live;*retained=a;*uncertain=b;unlock(f);
}
