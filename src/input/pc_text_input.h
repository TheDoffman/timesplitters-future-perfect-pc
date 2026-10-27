#ifndef TSFP_PC_TEXT_INPUT_H
#define TSFP_PC_TEXT_INPUT_H
#include <stdint.h>
/* Host adapters serialize this bounded queue with their existing input lock.
 * A new dialog/focus loss discards pending text, never replaying it elsewhere. */
enum { PC_TEXT_BACKSPACE=0x100, PC_TEXT_ACCEPT, PC_TEXT_CANCEL,
       PC_TEXT_LEFT, PC_TEXT_RIGHT, PC_TEXT_HOME, PC_TEXT_END, PC_TEXT_DELETE,
       PC_TEXT_SELECT_ALL };
typedef struct PcTextInput {
    uint32_t owner;
    unsigned head,count;
    unsigned events[64];
} PcTextInput;
static inline int pc_text_key(unsigned key) {
    return key>=8 && key<256 && !(key>=0x70 && key<=0x87);
}
static inline void pc_text_session(PcTextInput *s,uint32_t owner) {
    if(s->owner!=owner) { s->owner=owner;s->head=s->count=0; }
}
static inline void pc_text_put(PcTextInput *s,unsigned event) {
    if(s->owner && s->count<64) s->events[(s->head+s->count++)%64]=event;
}
static inline unsigned pc_text_get(PcTextInput *s) {
    if(!s->count)return 0;
    unsigned event=s->events[s->head];s->head=(s->head+1)%64;--s->count;
    return event;
}
static inline void pc_text_character(PcTextInput *s,unsigned codepoint) {
    /* Original font/keyboard uses Latin-1. Unsupported Unicode is ignored,
     * rather than inserting broken UTF-8 bytes or guest formatting controls. */
    if(codepoint>=32 && codepoint<=255 && !(codepoint>=127 && codepoint<160))
        pc_text_put(s,codepoint);
}
static inline void pc_text_utf8(PcTextInput *s,const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    while(*p) {
        unsigned c=*p++;
        if(c<128)pc_text_character(s,c);
        else if((c==0xc2 || c==0xc3) && *p>=0x80 && *p<=0xbf)
            pc_text_character(s,((c&31)<<6)|(*p++&63));
        /* Other UTF-8 bytes cannot become printable ASCII. */
    }
}
static inline void pc_text_command(PcTextInput *s,unsigned key,int repeat,int control) {
    unsigned event=0;
    switch(key) {
    case 8:event=PC_TEXT_BACKSPACE;break;
    case 13:if(!repeat)event=PC_TEXT_ACCEPT;break;
    case 27:if(!repeat)event=PC_TEXT_CANCEL;break;
    case 0x25:event=PC_TEXT_LEFT;break;case 0x27:event=PC_TEXT_RIGHT;break;
    case 0x24:event=PC_TEXT_HOME;break;case 0x23:event=PC_TEXT_END;break;
    case 0x2e:event=PC_TEXT_DELETE;break;
    case 'A':if(control && !repeat)event=PC_TEXT_SELECT_ALL;break;
    }
    if(event)pc_text_put(s,event);
}
/* Implemented by the Windows/SDL adapter, used only by the guest menu thread. */
void pc_controls_text_mode(uint32_t owner);
unsigned pc_controls_text_event(uint32_t owner);
#endif
