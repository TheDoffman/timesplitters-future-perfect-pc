#include "guest_device_event.h"
#include "recomp_types.h"
int tsfp_device_event_pin(tsfp_device_factory *factory,uint32_t address,tsfp_device_event *out) {
    tsfp_guest_object_lease lease={0};void *mapping;uint32_t bytes;unsigned char *event;
    if(!out || out->lease.owner || out->final.factory || out->bytes || out->address || address<0x34 ||
       !tsfp_device_factory_pin(factory,address-0x34,&lease,&mapping,&bytes))return 0;
    if(bytes<0x44) {(void)tsfp_guest_objects_release(&lease);return 0;}
    event=(unsigned char *)mapping+0x34;
    if(event[0]!=1 || event[1] || event[2]!=4 || ((uintptr_t)(event+4)&3)) {
        (void)tsfp_guest_objects_release(&lease);return 0;
    }
    out->lease=lease;out->bytes=event;out->address=address;return 1;
}
int tsfp_device_event_pin_finalization(tsfp_device_factory *factory,const tsfp_device_finalization *ticket,
    uint32_t address,tsfp_device_event *out) {
    tsfp_device_final_pin pin={0};void *mapping;uint32_t bytes;unsigned char *event;
    if(!out || out->lease.owner || out->final.factory || out->bytes || out->address || !ticket ||
       ticket->body>UINT32_MAX-0x43 || address!=ticket->body+0x34 ||
       !tsfp_device_factory_pin_finalization(factory,ticket,&pin,&mapping,&bytes))return 0;
    if(bytes<0x44) {(void)tsfp_device_factory_release_finalization(&pin);return 0;}
    event=(unsigned char *)mapping+0x34;
    if(event[0]!=1 || event[1] || event[2]!=4 || ((uintptr_t)(event+4)&3)) {
        (void)tsfp_device_factory_release_finalization(&pin);return 0;
    }
    out->final=pin;out->bytes=event;out->address=address;return 1;
}
int tsfp_device_event_release(tsfp_device_event *event) {
    if(!event || !event->bytes || (!event->lease.owner && !event->final.factory) ||
       (event->lease.owner && event->final.factory))return 0;
    event->bytes=NULL;event->address=0;
    return event->lease.owner?tsfp_guest_objects_release(&event->lease):tsfp_device_factory_release_finalization(&event->final);
}
static void *signal(tsfp_device_event *event) {
    return event && (event->lease.owner || event->final.factory) && event->bytes?event->bytes+4:NULL;
}
int tsfp_device_event_try_wait(tsfp_device_event *event) {
    void *p=signal(event);uint32_t previous;
    if(!p)return -1;
    previous=tsfp_atomic_cas32(p,1,0);return previous==1?1:previous==0?0:-1;
}
int tsfp_device_event_set(tsfp_device_event *event) {
    void *p=signal(event);uint32_t previous;if(!p)return -1;
    previous=tsfp_atomic_exchange32(p,1);return previous<=1?(int)previous:-1;
}
int tsfp_device_event_reset(tsfp_device_event *event) {
    void *p=signal(event);uint32_t previous;if(!p)return -1;
    previous=tsfp_atomic_exchange32(p,0);return previous<=1?(int)previous:-1;
}
