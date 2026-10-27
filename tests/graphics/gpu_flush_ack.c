/* Actual notifier plus actual lifted store; the caller never acknowledges. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_gpu_ack.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); return 1; } } while (0)
typedef int32_t LONG;
#define XBOX_NV2A_BASE 0xFD000000u
static void *g_nv2a_memory, *g_nv2a_ack_thread, *g_nv2a_ack_wake;
static volatile LONG g_nv2a_ack_stop, g_nv2a_ack_notifiers;
static unsigned signals;
static uint32_t seen_word;
static int xbox_EnvSwitch(const char *name,int fallback)
{ const char *value=getenv(name); return value ? strcmp(value,"0")!=0 : fallback; }
static LONG InterlockedCompareExchange(volatile LONG *word,LONG value,LONG expected)
{ LONG old=*word; if(old==expected)*word=value; return old; }
static LONG InterlockedIncrement(volatile LONG *word) {return ++*word;}
static LONG InterlockedDecrement(volatile LONG *word) {return --*word;}
static int SetEvent(void *event)
{
    if(event!=g_nv2a_ack_wake || !g_nv2a_ack_notifiers) abort();
    ++signals;
    seen_word=*(uint32_t *)((char *)g_nv2a_memory+0x100410);
    return 1;
}
/* ACTUAL_ACK_FUNCTION */
static uint32_t eax,ecx;
#define MEM32(address) (*(uint32_t *)((char *)g_nv2a_memory + (uint32_t)(address) - XBOX_NV2A_BASE))
static void guest_store(void) { /* ACTUAL_GUEST_STORE */ }
static void neighboring_store(void) { /* NEIGHBOR_GUEST_STORE */ }
int main(void)
{
    /* Keep the adapter referenced in the deliberate direct-clear mutation so
     * Clang reaches its behavioral failure instead of an unused-function error. */
    (void)SetEvent;
    void *memory=calloc(1,0x101000); CHECK(memory);
    g_nv2a_memory=memory;g_nv2a_ack_thread=(void *)(uintptr_t)1;
    g_nv2a_ack_wake=(void *)(uintptr_t)2;
    volatile uint32_t *word=(volatile uint32_t *)((char *)memory+0x100410);
    eax=XBOX_NV2A_BASE;ecx=0xa5010041u;
    guest_store();
    CHECK(*word==ecx && g_nv2a_ack_notifiers==0);
    if (!xbox_EnvSwitch("RECOMP_PFB_WAKE_ACK",1)) {
        CHECK(signals==0);
        free(memory);puts("PASS: explicit fallback retains polling handshake");return 0;
    }
    CHECK(signals==1 && seen_word==ecx);
    CHECK(eax==XBOX_NV2A_BASE && ecx==0xa5010041u);
    ecx=0x10001;neighboring_store();CHECK(*word==ecx && signals==1);
    eax+=4;guest_store();CHECK(MEM32(eax+0x100410)==ecx && signals==1);
    eax=XBOX_NV2A_BASE;
    g_nv2a_ack_thread=NULL;guest_store();CHECK(*word==ecx && signals==1);
    g_nv2a_ack_thread=(void *)(uintptr_t)1;g_nv2a_ack_stop=1;
    guest_store();CHECK(*word==ecx && signals==1);
    g_nv2a_ack_stop=0;g_nv2a_memory=NULL;
    xbox_Nv2aNotifyPfbFlush(XBOX_NV2A_BASE+0x100410);CHECK(signals==1);
    g_nv2a_memory=memory;g_nv2a_ack_wake=NULL;
    guest_store();CHECK(*word==ecx && signals==1);
    CHECK(g_nv2a_ack_notifiers==0);
    g_nv2a_ack_wake=(void *)(uintptr_t)2;guest_store();
    CHECK(signals==2 && seen_word==ecx && *word==ecx);
    free(memory);
    puts("PASS: post-store notification, untouched registers, fallback and guards");
    return 0;
}
