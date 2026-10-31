#include "guest_event.h"
#include "recomp_types.h"
#include <string.h>
static void put(unsigned char *p,uint32_t v) {unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(8*i));}
int tsfp_event_initialize(void *target,size_t span,uint32_t address,uint32_t type,int initial) {
    unsigned char value[16]={0};
    if(!target || span<16 || !address || (address&3) || address>UINT32_MAX-15 || type>1)return 0;
    value[0]=(unsigned char)type;value[2]=4;put(value+4,initial?1:0);
    put(value+8,address+8);put(value+12,address+8);memcpy(target,value,sizeof(value));return 1;
}
static int valid(const tsfp_event_view *view) {
    return view && view->signal && !((uintptr_t)view->signal&3) && view->address &&
        !(view->address&3) && view->address<=UINT32_MAX-15 && view->synchronization<=1;
}
int tsfp_event_state(const tsfp_event_view *view) {
    uint32_t state;if(!valid(view))return -1;
    state=tsfp_atomic_cas32(view->signal,0,0);return state<=1?(int)state:-1;
}
int tsfp_event_bind(tsfp_event_view *out,void *mapping,size_t span,uint32_t address) {
    unsigned char *p=mapping;tsfp_event_view value;
    if(!out || !p || span<16 || p[0]>1 || p[2]!=4)return 0;
    value.signal=p+4;value.address=address;value.synchronization=p[0];
    if(tsfp_event_state(&value)<0)return 0;
    *out=value;return 1;
}
static int change(const tsfp_event_view *view,uint32_t desired) {
    uint32_t previous,observed;if(!valid(view))return -1;
    previous=tsfp_atomic_cas32(view->signal,0,0);
    for(;;) {
        if(previous>1)return -1;
        observed=tsfp_atomic_cas32(view->signal,previous,desired);
        if(observed==previous)return (int)previous;
        previous=observed;
    }
}
int tsfp_event_set(const tsfp_event_view *view) {return change(view,1);}
int tsfp_event_reset(const tsfp_event_view *view) {return change(view,0);}
int tsfp_event_try_wait(const tsfp_event_view *view) {
    uint32_t previous;if(!valid(view))return -1;
    if(!view->synchronization)return tsfp_event_state(view);
    previous=tsfp_atomic_cas32(view->signal,1,0);return previous==1?1:previous==0?0:-1;
}
