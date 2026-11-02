#include "guest_x86_context.h"
#include <string.h>
typedef struct {unsigned group,guest;size_t state;} context_word;
#define WORD(group,guest,field) {group,guest,offsetof(tsfp_x86_context_state,field)}
static const context_word words[]={
 WORD(16,4,debug[0]),WORD(16,8,debug[1]),WORD(16,12,debug[2]),
 WORD(16,16,debug[3]),WORD(16,20,debug[4]),WORD(16,24,debug[5]),
 WORD(4,140,gs),WORD(4,144,fs),WORD(4,148,es),WORD(4,152,ds),
 WORD(2,156,edi),WORD(2,160,esi),WORD(2,164,ebx),
 WORD(2,168,edx),WORD(2,172,ecx),WORD(2,176,eax),
 WORD(1,180,ebp),WORD(1,184,eip),WORD(1,188,cs),
 WORD(1,192,eflags),WORD(1,196,esp),WORD(1,200,ss)};
#undef WORD
static uint32_t get(const unsigned char *p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put(unsigned char *p,uint32_t v) {
 unsigned i;for(i=0;i<4;++i)p[i]=(unsigned char)(v>>(i*8));
}
static int flags_valid(uint32_t flags) {
 return (flags&~UINT32_C(63))==TSFP_CONTEXT_I386;
}
int tsfp_x86_context_encode(void *output,size_t bytes,const tsfp_x86_context_state *state,uint32_t flags) {
 tsfp_x86_context_state saved;unsigned char *p=output;size_t i;
 if(!p || !state || bytes<TSFP_CONTEXT_BYTES || !flags_valid(flags))return 0;
 saved=*state;
 for(i=0;i<sizeof words/sizeof words[0];++i)if(flags&words[i].group) {
  uint32_t v;memcpy(&v,(const unsigned char *)&saved+words[i].state,4);put(p+words[i].guest,v);
 }
 if(flags&TSFP_CONTEXT_FLOAT)memcpy(p+28,saved.x87,sizeof saved.x87);
 if(flags&TSFP_CONTEXT_EXTENDED)memcpy(p+204,saved.extended,sizeof saved.extended);
 put(p,flags);return 1;
}
uint32_t tsfp_x86_context_decode(tsfp_x86_context_state *state,const void *input,size_t bytes) {
 const unsigned char *p=input;tsfp_x86_context_state saved;uint32_t flags;size_t i;
 if(!state || !p || bytes<TSFP_CONTEXT_BYTES)return 0;
 flags=get(p);if(!flags_valid(flags))return 0;saved=*state;
 for(i=0;i<sizeof words/sizeof words[0];++i)if(flags&words[i].group) {
  uint32_t v=get(p+words[i].guest);memcpy((unsigned char *)&saved+words[i].state,&v,4);
 }
 if(flags&TSFP_CONTEXT_FLOAT)memcpy(saved.x87,p+28,sizeof saved.x87);
 if(flags&TSFP_CONTEXT_EXTENDED)memcpy(saved.extended,p+204,sizeof saved.extended);
 *state=saved;return flags;
}
