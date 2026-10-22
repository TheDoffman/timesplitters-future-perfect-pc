#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PC_PROMPTS_H
#define TSFP_PC_GUEST_MEMORY_H
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
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;strcpy((char *)memory+1024,"label");
    g_esp=8192;sub_00041E30();
    CHECK(queries==1);CHECK(draws==5 && g_esp==8196 && g_eax==0xabcdef01 && !scope.depth);
    /* A later operation must recheck protections, including the fallback. */
    readable=0;g_esp=8192;sub_00041E30();
    CHECK(queries==6 && draws==10 && !scope.depth);
    readable=1;translate=1;g_esp=8192;sub_00041E30();
    CHECK(writes==5 && draws==15 && g_esp==8196 && g_eax==0xabcdef01 && !scope.depth);
    puts("PASS: outlined text batches, nested reads, fresh writes, expiry, passes and ABI");
    return 0;
}
