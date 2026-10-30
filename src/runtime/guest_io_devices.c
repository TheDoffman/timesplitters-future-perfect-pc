#include "guest_io_devices.h"
#include <string.h>
static void finish_devices(void *context) {
    tsfp_io_devices *b=(tsfp_io_devices *)context;size_t i;
    void (*retired)(void *,tsfp_io_devices *,int)=b->retired;void *host=b->context;
    b->closed=1;b->owner=NULL;
    for(i=b->count;i>0;--i) {
        tsfp_io_device_entry *e=&b->entries[i-1];
        if(!tsfp_guest_objects_release(&e->lease))++b->uncertain;
    }
    retired(host,b,b->uncertain==0);
}
int tsfp_io_devices_init(tsfp_io_devices *b,tsfp_io_owner *owner,tsfp_device_factory *device_factory,
    tsfp_io_device_entry *entry_storage,size_t capacity,void (*retired)(void *,tsfp_io_devices *,int),void *context) {
    if(!b || !owner || !device_factory || !entry_storage || !capacity || capacity>TSFP_IO_MAX_STACKS || !retired)return 0;
    memset(b,0,sizeof(*b));memset(entry_storage,0,capacity*sizeof(*entry_storage));
    b->owner=owner;b->factory=device_factory;b->entries=entry_storage;b->capacity=capacity;b->retired=retired;b->context=context;
    if(!tsfp_io_owner_attach(owner,b,finish_devices)) {b->owner=NULL;b->closed=1;return 0;}
    return 1;
}
int tsfp_io_devices_pin(tsfp_io_devices *b,uint32_t body,void **mapping,uint32_t *bytes) {
    size_t i;tsfp_io_device_entry *e;tsfp_guest_object_lease lease={0};void *value;uint32_t span;
    if(!b || b->closed || !b->owner || !body || !mapping || !bytes)return 0;
    for(i=0;i<b->count;++i) {
        e=&b->entries[i];
        if(e->body==body) {*mapping=e->mapping;*bytes=e->bytes;return 1;}
    }
    if(b->count>=b->capacity)return 0;
    if(!tsfp_device_factory_pin(b->factory,body,&lease,&value,&span))return 0;
    e=&b->entries[b->count];e->body=body;e->bytes=span;e->mapping=value;e->lease=lease;
    ++b->count;*mapping=value;*bytes=span;return 1;
}
