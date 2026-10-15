/* Included by the US Future Perfect manual wrappers. All string pointers and
 * call frames remain Xbox addresses; never truncate a host pointer to 32 bits.
 * Original ABI: 3FCF0(out width, out height, text, first line only),
 * 40FA0(position, baseline, wrap width, text, glyph callback, glyph state).
 */
#include "pc_prompts.h"

#include "pc_guest_memory.h"

static void pc_prompt_guest(void (*original)(void), unsigned text_arg, unsigned args)
{
    uint32_t saved=g_esp, text_va, scratch, frame;
    uintptr_t stack=(uintptr_t)saved+g_xbox_mem_offset, text;
    char source[2048], translated[2048];
    size_t length=0;
    if (!pc_guest_memory(stack,(args+1)*4,0)) {
        original(); return;
    }
    text_va=((uint32_t *)stack)[text_arg];
    text=(uintptr_t)text_va+g_xbox_mem_offset;
    if (!text_va) { original(); return; }
    /* Validate one region at a time, including strings straddling a page. */
    while (length<sizeof source) {
        size_t available=pc_guest_region(text+length,0);
        if(!available)break;
        if (available>sizeof source-length) available=sizeof source-length;
        const char *part=(const char *)(text+length);
        const char *end=memchr(part,0,available);
        size_t n=end ? (size_t)(end-part)+1 : available;
        memcpy(source+length,part,n); length+=n;
        if (end) break;
    }
    if (!length || source[length-1]) { original(); return; }
    if (!pc_prompts_rewrite(source,translated,sizeof translated)) { original(); return; }
    length=strlen(translated)+1;
    if (saved<length+128) { original(); return; }
    scratch=(saved-(uint32_t)length)&~15u;
    frame=scratch-(args+1)*4;
    if (!pc_guest_memory((uintptr_t)frame+g_xbox_mem_offset,saved-frame,1)) { original(); return; }
    memcpy((void *)((uintptr_t)scratch+g_xbox_mem_offset),translated,length);
    memcpy((void *)((uintptr_t)frame+g_xbox_mem_offset),(void *)stack,(args+1)*4);
    ((uint32_t *)((uintptr_t)frame+g_xbox_mem_offset))[text_arg]=scratch;
    g_esp=frame;
    original();
    g_esp=saved+4; /* cdecl ret; the caller still owns its original arguments. */
}
