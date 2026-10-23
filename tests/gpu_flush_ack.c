/* Actual runtime acknowledgement plus actual lifted guest store. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_gpu_ack.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); return 1; } } while (0)
typedef int32_t LONG;
#define XBOX_NV2A_BASE 0xFD000000u
static void *g_nv2a_memory, *g_nv2a_ack_thread;
static volatile LONG g_nv2a_ack_stop;
static volatile LONG *inject_word;
static unsigned writes;
static int xbox_EnvSwitch(const char *name,int fallback)
{ const char *value=getenv(name); return value ? strcmp(value,"0")!=0 : fallback; }
/* Inject another flag changing between the load and compare/exchange. */
static LONG InterlockedCompareExchange(volatile LONG *word,LONG value,LONG expected)
{
    if (word==inject_word && expected!=0) {
        *word |= 0x200; inject_word=NULL;
    }
    LONG old=*word;
    if (old==expected) {
        *word=value;
        if(value!=old && word==(volatile LONG *)((char *)g_nv2a_memory+0x100410)) ++writes;
    }
    return old;
}
/* ACTUAL_ACK_FUNCTION */
static uint32_t eax,ecx;
#define MEM32(address) (*(uint32_t *)((char *)g_nv2a_memory + (uint32_t)(address) - XBOX_NV2A_BASE))
static void guest_store(void) { /* ACTUAL_GUEST_STORE */ }
static void neighboring_store(void) { /* NEIGHBOR_GUEST_STORE */ }
int main(void)
{
    void *memory=calloc(1,0x101000); CHECK(memory);
    g_nv2a_memory=memory;g_nv2a_ack_thread=(void *)(uintptr_t)1;
    volatile LONG *word=(volatile LONG *)((char *)memory+0x100410);
    eax=XBOX_NV2A_BASE;ecx=0xa5010041u;
    guest_store();
    if (!xbox_EnvSwitch("RECOMP_PFB_SYNC_ACK",1)) {
        CHECK((uint32_t)*word==ecx && writes==0);
        free(memory);puts("PASS: explicit diagnostic fallback retains polling handshake");return 0;
    }
    CHECK((uint32_t)*word==0xa5000041u); CHECK(writes==1);
    CHECK(eax==XBOX_NV2A_BASE && ecx==0xa5010041u);
    /* No request: preserve the value without an acknowledgement write. */
    ecx=0x44000020;guest_store();CHECK((uint32_t)*word==ecx && writes==1);
    /* A neighboring instruction and another register retain normal stores. */
    ecx=0x10001;neighboring_store();CHECK((uint32_t)*word==ecx);
    eax+=4;guest_store();CHECK(MEM32(eax+0x100410)==ecx && (uint32_t)*word==ecx);
    eax=XBOX_NV2A_BASE;
    /* Initialization, teardown and unmapped storage do not acknowledge. */
    g_nv2a_ack_thread=NULL;guest_store();CHECK((uint32_t)*word==ecx);
    g_nv2a_ack_thread=(void *)(uintptr_t)1;g_nv2a_ack_stop=1;
    guest_store();CHECK((uint32_t)*word==ecx);
    g_nv2a_ack_stop=0;g_nv2a_memory=NULL;
    xbox_Nv2aAcknowledgePfbFlush(XBOX_NV2A_BASE+0x100410);CHECK((uint32_t)*word==ecx);
    g_nv2a_memory=memory;
    /* Preserve a flag concurrently changed before the acknowledgement CAS. */
    inject_word=word;guest_store();CHECK((uint32_t)*word==0x201 && inject_word==NULL);
    free(memory);
    puts("PASS: post-store PFB acknowledgement, untouched registers/flags, guard paths and CAS retry");
    return 0;
}
