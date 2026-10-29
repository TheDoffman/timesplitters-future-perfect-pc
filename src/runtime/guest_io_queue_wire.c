#include "guest_io_queue_wire.h"
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));}
static tsfp_io_owner *waiting(const tsfp_io_queue *q,size_t offset) {
    size_t remaining=q->capacity-q->head;
    return q->entries[offset<remaining?q->head+offset:offset-remaining];
}
static void changed(void *context,const tsfp_io_queue *q) {
    tsfp_io_queue_wire *wire=context;unsigned char *p=wire->device;
    uint32_t sentinel=wire->address+0x2c;size_t i;
    put(p+0x10,q->current?q->current->address:0);p[0x2b]=(unsigned char)(q->current || q->count);
    put(p+0x2c,q->count?waiting(q,0)->address+0x3c:sentinel);
    put(p+0x30,q->count?waiting(q,q->count-1)->address+0x3c:sentinel);
    for(i=0;i<q->count;++i) {
        tsfp_io_owner *owner=waiting(q,i);
        put(owner->bytes+0x3c,i+1<q->count?waiting(q,i+1)->address+0x3c:sentinel);
        put(owner->bytes+0x40,i?waiting(q,i-1)->address+0x3c:sentinel);
        owner->bytes[0x48]=1;
    }
}
int tsfp_io_queue_wire_init(tsfp_io_queue_wire *wire,tsfp_io_queue *q,void *device,size_t span,uint32_t address) {
    unsigned char *p=device;tsfp_io_queue_wire value;uint32_t bytes;
    if(!wire || !q || !p || span<72 || !address || address>UINT32_MAX-71)return 0;
    if(q->closed || q->current || q->count || q->publish || !q->entries || !q->capacity)return 0;
    bytes=(uint32_t)p[2]|((uint32_t)p[3]<<8);
    if(p[0]!=3 || p[1] || bytes<72 || span<bytes || address>UINT32_MAX-(bytes-1) || p[0x28]!=0x14 || p[0x29] || p[0x2a]!=12)return 0;
    value.device=p;value.address=address;*wire=value;
    q->publish_context=wire;q->publish=changed;changed(wire,q);return 1;
}
