#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PC_PROMPTS_H
#define TSFP_PC_GUEST_MEMORY_H
#define TSFP_PC_GUEST_SPAN_H
#define pc_guest_query_span(address,minimum) pc_guest_query_region(address)
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);}} while(0)
static unsigned char memory[16384];
static uint32_t g_esp,g_eax;
static uintptr_t g_xbox_mem_offset;
static unsigned queries,draws,writes;
static int readable=1,translate;
typedef struct PcGuestRegion {uintptr_t base;size_t size;int writable;} PcGuestRegion;
static inline PcGuestRegion pc_guest_query_region(uintptr_t address)
{
    PcGuestRegion r={0,0,0};++queries;
    if(readable && address>=(uintptr_t)memory && address-(uintptr_t)memory<sizeof memory) {
        r.base=(uintptr_t)memory;r.size=sizeof memory;r.writable=1;
    }
    return r;
}
static inline size_t pc_guest_region_remaining(const PcGuestRegion *r,uintptr_t a,int writing)
{return (!writing || r->writable) && a>=r->base && a-r->base<r->size?r->size-(a-r->base):0;}
static int pc_guest_memory(uintptr_t a,size_t n,int writing)
{PcGuestRegion r=pc_guest_query_region(a);CHECK(writing);++writes;return n<=pc_guest_region_remaining(&r,a,writing);}
#include "pc_text_read_scope.h"
static PcTextReadScope scope;
void pc_prompts_layout_begin(void){pc_text_read_begin(&scope);}
void pc_prompts_layout_end(void){pc_text_read_end(&scope);}
size_t pc_prompts_read_region(uintptr_t a){return pc_text_read_region(&scope,a);}
size_t pc_prompts_read_span(uintptr_t a,size_t n){return pc_text_read_span(&scope,a,n);}
int pc_prompts_rewrite(const char *source,char *out,size_t capacity)
{CHECK(!strcmp(source,"label"));if(!translate)return 0;CHECK(capacity>1);strcpy(out,"E");return 1;}
#include "pc_prompt_guest.inc"
static void draw(void)
{
    uint32_t text=((uint32_t *)(memory+g_esp))[4];
    CHECK(!strcmp((char *)memory+text,readable && translate?"E":"label"));
    ++draws;g_eax=0xabcdef01;g_esp+=4;
}
void sub_00041E30_gen(void)
{
    uint32_t saved=g_esp;
    /* US 0x41E30 draws four outline offsets and the foreground, using two
       alternating stack frames. The caller owns all original arguments. */
    for(unsigned pass=0;pass<5;pass++) {
        g_esp=saved-128-(pass%2)*32;
        ((uint32_t *)(memory+g_esp))[4]=1024;
        pc_prompt_guest(draw,4,6);
    }
    g_esp=saved+4;
}
/* PRODUCTION_OUTLINE_WRAPPER */
#ifdef TEST_PANEL_SCOPE
static unsigned measurements,placements;
static void measure(void)
{
    uint32_t text=((uint32_t *)(memory+g_esp))[3];
    CHECK(!strcmp((char *)memory+text,readable && translate?"E":"label"));
    ++measurements;g_esp+=4;
}
void sub_0012E3F0_gen(void)
{
    uint32_t saved=g_esp;
    /* Shared panel: width, fit, width, then foreground/outline labels.
       Font metrics and graphics are spies; mapping checks are production. */
    for(unsigned pass=0;pass<3;pass++) {
        g_esp=saved-64-pass*32;
        ((uint32_t *)(memory+g_esp))[3]=1024;
        pc_prompt_guest(measure,3,4);
    }
    for(unsigned label=0;label<2;label++) {
        g_esp=saved-256-label*64;sub_00041E30();
    }
    g_esp=saved+4;
}
static void pc_display_ui(void (*original)(void),uint32_t tag)
{
    CHECK(tag==0x0012e3f0 && !scope.depth);++placements;
    original();
    /* Placement flushes remain outside the reused mapping lifetime. */
    CHECK(!scope.depth);++placements;
}
/* PRODUCTION_PANEL_WRAPPER */
#endif
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;strcpy((char *)memory+1024,"label");
#ifdef TEST_PANEL_SCOPE
    g_esp=8192;sub_0012E3F0();
    printf("panel read queries=%u measurements=%u draws=%u\n",queries,measurements,draws);fflush(stdout);
    CHECK(queries==1);
    CHECK(measurements==3 && draws==10 && placements==2 && g_esp==8196 && !scope.depth);
    /* New operations must reject changed read protection. */
    readable=0;g_esp=8192;sub_0012E3F0();
    CHECK(queries==14 && measurements==6 && draws==20 && placements==4 && !scope.depth);
    readable=1;translate=1;g_esp=8192;sub_0012E3F0();
    CHECK(writes==13 && measurements==9 && draws==30 && placements==6 && g_esp==8196 && !scope.depth);
    puts("PASS: panel read reuse, live scratch writes, scope expiry, placement boundaries and all text passes");
#else
    g_esp=8192;sub_00041E30();
    CHECK(queries==1);CHECK(draws==5 && g_esp==8196 && g_eax==0xabcdef01 && !scope.depth);
    /* A later operation must recheck protections, including the fallback. */
    readable=0;g_esp=8192;sub_00041E30();
    CHECK(queries==6 && draws==10 && !scope.depth);
    readable=1;translate=1;g_esp=8192;sub_00041E30();
    CHECK(writes==5 && draws==15 && g_esp==8196 && g_eax==0xabcdef01 && !scope.depth);
    puts("PASS: outlined text batches, nested reads, fresh writes, expiry, passes and ABI");
#endif
    return 0;
}
