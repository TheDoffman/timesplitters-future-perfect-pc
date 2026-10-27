#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TSFP_PC_PROMPTS_H
#define TSFP_PC_GUEST_MEMORY_H
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}} while(0)
static unsigned char memory[16384];
static uint32_t g_esp;
static uintptr_t g_xbox_mem_offset;
static unsigned depth,max_depth,calls,writes;
static int translate,nest;
void pc_prompts_layout_begin(void){if(++depth>max_depth)max_depth=depth;}
void pc_prompts_layout_end(void){CHECK(depth);--depth;}
size_t pc_prompts_read_region(uintptr_t address)
{CHECK(depth);return address>=(uintptr_t)memory && address-(uintptr_t)memory<sizeof memory?
 sizeof memory-(address-(uintptr_t)memory):0;}
static int pc_guest_memory(uintptr_t address,size_t bytes,int writing)
{CHECK(writing);++writes;return bytes<=pc_prompts_read_region(address);}
size_t pc_prompts_read_span(uintptr_t address,size_t minimum)
{size_t n=pc_prompts_read_region(address);return n>=minimum?n:0;}
int pc_prompts_rewrite(const char *source,char *out,size_t capacity)
{(void)source;CHECK(capacity>=2);if(!translate)return 0;strcpy(out,"E");return 1;}
#include "pc_prompt_guest.inc"
static void leaf(void)
{CHECK(depth);++calls;g_esp+=4;}
static void draw(void)
{
    uint32_t frame=g_esp;
    CHECK(depth && ((uint32_t *)(memory+frame))[1]);
    if(translate)CHECK(!strcmp((char *)memory+((uint32_t *)(memory+frame))[1],"E"));
    if(nest){g_esp=frame-128;((uint32_t *)(memory+g_esp))[1]=1024;pc_prompt_guest(leaf,1,1);}
    g_esp=frame+4;++calls;
}
int main(void)
{
    g_xbox_mem_offset=(uintptr_t)memory;strcpy((char *)memory+1024,"label");
    for(unsigned mode=0;mode<4;mode++){
        translate=mode&1;nest=mode&2;g_esp=8192;((uint32_t *)(memory+g_esp))[1]=1024;
        pc_prompt_guest(draw,1,1);CHECK(g_esp==8196 && depth==0);
    }
    CHECK(max_depth==2 && calls==6 && writes==3);
    /* Invalid frame and null string fallback must close their scope too. */
    g_esp=sizeof memory+1;pc_prompt_guest(leaf,1,1);CHECK(depth==0);
    g_esp=8192;((uint32_t *)(memory+g_esp))[1]=0;
    pc_prompt_guest(leaf,1,1);CHECK(depth==0 && g_esp==8196);
    puts("PASS: nested prompt scopes, scratch translation, guest ABI and fallback expiry");
    return 0;
}
