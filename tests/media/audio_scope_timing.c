/* Exercise the production guest wrappers with only their original callees
 * doubled. These are ABI/category checks, not executed game dialogue. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define PC_AUDIO_MIX_IMPLEMENTATION
#include "gameplay/pc_audio_mix.h"
#undef PC_AUDIO_MIX_IMPLEMENTATION
#define TSFP_PC_CONTROLS
static uint8_t memory[4096];
static ptrdiff_t g_xbox_mem_offset;
static uint32_t g_eax,g_ecx,g_esp;
static int expected,original_calls;
static uint32_t seen_record,seen_sound;
static void word(uint32_t address,uint32_t value){memcpy(memory+address,&value,4);}
unsigned pc_audio_stream_scope(uint32_t record,uint32_t sound,uint32_t sp)
{
    seen_record=record;seen_sound=sound;
    return pc_audio_scope_enter(pc_audio_stream_category(record,0x210,pc_audio_scope_bus(sp)),sp);
}
void sub_00045820_gen(void)
{assert(pc_audio_scope_bus(g_esp)==expected);original_calls++;g_eax=123;g_esp+=4;}
void sub_000299C0_gen(void)
{assert(pc_audio_scope_bus(g_esp)==expected);original_calls++;g_eax=456;g_esp+=4;}
void sub_00045820(void);
void sub_00046D80_gen(void)
{
    assert(pc_audio_scope_bus(g_esp)==expected);original_calls++;
    /* The real common helper calls45820 from a non-speech-specific site;
       the outer speech scope must carry through it without tagging other calls. */
    g_esp-=64;word(g_esp,0x46f27);sub_00045820();g_esp+=64;
}
#include "gameplay/pc_audio_mix_guest.inc"
#include "audio_scope_wrappers.inc"
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)memory;
    for(unsigned i=0;i<3;i++) {
        const uint32_t callers[]={0x1643c2,0x164d20,0x123456};
        g_esp=2048;word(g_esp,callers[i]);expected=i<2?PC_AUDIO_DIALOGUE:-1;
        sub_00046D80();assert(g_esp==2052 && g_eax==123);
        assert(pc_audio_scope_bus(g_esp)==-1);
    }
    g_esp=2048;word(g_esp,0x262700);expected=PC_AUDIO_DIALOGUE;
    sub_00045820();assert(g_eax==123 && g_esp==2052 && pc_audio_scope_bus(g_esp)==-1);
    g_esp=2048;word(g_esp,0x123456);expected=-1;sub_00045820();
    assert(pc_audio_scope_bus(g_esp)==-1);
    unsigned outer=pc_audio_scope_enter(PC_AUDIO_DIALOGUE,2200);
    g_esp=2048;g_ecx=0x5836d0;word(g_esp+4,12);expected=PC_AUDIO_DIALOGUE;
    sub_000299C0();assert(seen_record==0x5836d0 && seen_sound==12 && g_eax==456);
    assert(pc_audio_scope_bus(g_esp)==PC_AUDIO_DIALOGUE);
    g_esp=2048;g_ecx=0x64bd98;expected=PC_AUDIO_MUSIC;sub_000299C0();
    assert(pc_audio_scope_bus(g_esp)==PC_AUDIO_DIALOGUE);pc_audio_scope_leave(outer);
    g_esp=2048;g_ecx=0x5836d0;expected=PC_AUDIO_BACKGROUND;sub_000299C0();
    g_esp=2048;g_ecx=0x123456;expected=PC_AUDIO_EFFECTS;sub_000299C0();
    assert(pc_audio_scope_bus(g_esp)==-1);
    /* An unwind discards the skipped inner scope while preserving a live parent. */
    outer=pc_audio_scope_enter(PC_AUDIO_DIALOGUE,2200);
    (void)pc_audio_scope_enter(PC_AUDIO_MUSIC,2000);
    assert(pc_audio_scope_bus(2100)==PC_AUDIO_DIALOGUE);
    assert(pc_audio_scope_bus(2300)==-1);pc_audio_scope_leave(outer);
    assert(pc_audio_stream_category(0x5836d0+9*0x14094,0x210,-1)==PC_AUDIO_BACKGROUND);
    assert(pc_audio_stream_category(0x5836d0+10*0x14094,0x210,-1)==PC_AUDIO_EFFECTS);
    assert(pc_audio_stream_category(0x5836d1,0x210,PC_AUDIO_DIALOGUE)==PC_AUDIO_EFFECTS);
    for(unsigned i=0;i<20;i++)(void)pc_audio_scope_enter(PC_AUDIO_DIALOGUE,2200-i*4);
    assert(pc_audio_scope_count==8 && pc_audio_scope_bus(1000)==PC_AUDIO_DIALOGUE);
    assert(pc_audio_scope_bus(3000)==-1);
    assert(original_calls==12);puts("PASS: speech wrappers, nested ownership, music precedence and unwind cleanup");
}
