#ifndef TSFP_PC_BINDING_CAPTURE_H
#define TSFP_PC_BINDING_CAPTURE_H
#include <stdint.h>
#include <string.h>
/* Wait for the opening click/key to be released, capture once, then suppress
 * that key until release. Event 0 = none, -1 = cancel, otherwise key+1. */
typedef struct PcBindingCapture { int phase,event; uint64_t deadline; } PcBindingCapture;
static inline void pc_binding_capture_begin(PcBindingCapture *s,uint64_t now)
{ s->phase=1;s->event=0;s->deadline=now+30000; }
/* Preserve quick taps that begin and end between game polling ticks. */
static inline void pc_binding_capture_key(PcBindingCapture *s,int key)
{
    if(key>=0xa0 && key<=0xa5)key=0x10+(key-0xa0)/2; /* Shared Shift/Ctrl/Alt keys. */
    if(s->phase==2 && key>0 && key<256) {
        s->event=key==27?-1:key+1;s->phase=3;
    }
}
static inline void pc_binding_capture_step(PcBindingCapture *s,const unsigned char keys[256],int focus,uint64_t now)
{
    int key=0;
    if(!s->phase)return;
    for(int i=1;i<256;i++)if(keys[i]) {key=i;break;}
    if((!focus || now>=s->deadline) && s->phase<3) {s->event=-1;s->phase=3;}
    if(s->phase==1) {if(!key)s->phase=2;}
    else if(s->phase==2 && key)pc_binding_capture_key(s,key);
    else if(s->phase==3 && !key)s->phase=0;
}
#endif
