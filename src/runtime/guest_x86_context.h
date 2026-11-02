/* Original little-endian i386 CONTEXT interchange, independent of host ABI. */
#ifndef TSFP_GUEST_X86_CONTEXT_H
#define TSFP_GUEST_X86_CONTEXT_H
#include <stdint.h>
#include <stddef.h>
enum {TSFP_CONTEXT_I386=0x10000,TSFP_CONTEXT_CONTROL=1,TSFP_CONTEXT_INTEGER=2,
      TSFP_CONTEXT_SEGMENTS=4,TSFP_CONTEXT_FLOAT=8,TSFP_CONTEXT_DEBUG=16,
      TSFP_CONTEXT_EXTENDED=32,TSFP_CONTEXT_BYTES=716};
typedef struct tsfp_x86_context_state {
    uint32_t eax,ebx,ecx,edx,esi,edi,esp,ebp,eip,eflags;
    uint32_t cs,ss,ds,es,fs,gs,debug[6];
    unsigned char x87[112],extended[512];
} tsfp_x86_context_state;
/* Complete caller-owned, stable buffers. Encode writes only selected groups and
 * ContextFlags; decode preserves unselected state. Floating/extended bytes are
 * opaque architectural images, not host floating values. Source/destination
 * overlap is supported. Failure (NULL/short/unsupported flags) writes nothing.
 * No register capture, selector validation, CPU restoration, exception dispatch
 * or host CONTEXT/NtContinue call is performed by this codec. */
int tsfp_x86_context_encode(void *,size_t,const tsfp_x86_context_state *,uint32_t flags);
/* Returns the accepted ContextFlags, or0 on failure. Only i386 and the six
 * documented groups are accepted; a group-free i386 header is valid. */
uint32_t tsfp_x86_context_decode(tsfp_x86_context_state *,const void *,size_t);
#endif
