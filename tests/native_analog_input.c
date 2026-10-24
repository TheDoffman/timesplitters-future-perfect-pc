/* Original US pad decoder and real wrapper; only XInput devices are controlled. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RECOMP_GENERATED_CODE
/* NUMERIC_POLICY */
#include "recomp_types.h"
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
RECOMP_TLS uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_seh_ebp;
RECOMP_TLS RecompXmm g_xmm0,g_xmm1,g_xmm2,g_xmm3,g_xmm4,g_xmm5,g_xmm6,g_xmm7;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top,g_fp_cmp,g_df;
enum { STACK=0x7000,RECORD=0x2000 };
static unsigned polls[4],feedback[4];
static uint16_t buttons;
static uint8_t triggers;
static int sign=1;
static uint32_t feedback_value[4];
static unsigned feedback_seen[4];
void sub_0046E36D(void)
{
    unsigned port=MEM32(g_esp+4)-0x100u;uint32_t out=MEM32(g_esp+8);assert(port<4);
    MEM32(out)=++polls[port];MEM16(out+4)=buttons;
    for(unsigned i=0;i<8;i++)MEM8(out+6+i)=0;
    MEM8(out+12)=MEM8(out+13)=triggers;
    for(unsigned i=0;i<4;i++)MEM16(out+14+i*2)=(uint16_t)(int16_t)(sign*(18000+1000*i+port*500));
    g_eax=0;g_esp+=12;
}
void sub_0046E3E0(void)
{
    unsigned port=MEM32(g_esp+4)-0x100u;assert(port<4);feedback[port]++;
    uint32_t out=MEM32(g_esp+8),value=MEM16(out+0x42)|((uint32_t)MEM16(out+0x44)<<16);
    if(feedback_seen[port])assert(value==feedback_value[port]);
    else {feedback_value[port]=value;feedback_seen[port]=1;assert(value!=0);}
    g_eax=0;g_esp+=12;
}
/* GENERATED_ROUTINES */
#define NT_WORD(address) MEM32(address)
/* INPUT_WRAPPER */
static void sample(unsigned port,unsigned whole)
{
    MEM32(0x4e7940)=whole;g_esp=STACK;MEM32(STACK)=0x12345678;
    MEM32(STACK+4)=RECORD+port*0x3c;MEM32(STACK+8)=port;
    g_esi=0xdead;g_ebx=0xbeef;g_edi=0xabcd;g_ebp=0xfeed;
    pc_native_input_guest(sub_0018FF70);
    assert(g_esp==STACK+4 && MEM32(STACK)==0x12345678 && g_fp_top==0);
    assert(g_esi==0xdead && g_ebx==0xbeef && g_edi==0xabcd && g_ebp==0xfeed);
}
int main(void)
{
    unsigned char *memory=calloc(1,0x800000);assert(memory);
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    /* ORIGINAL_CONSTANTS */
    MEM32(0x74d0fc)=15;
    for(unsigned port=0;port<4;port++) {
        MEM32(0x74d100+port*4)=0x100+port;
        for(unsigned axis=0;axis<4;axis++)MEM16(0x74d098+port*0x19+0xd+axis*2)=65535;
        MEM8(0x74d098+port*0x19+0xb)=MEM8(0x74d098+port*0x19+0xc)=255;
        MEM16(0x74d098+port*0x19+0x15)=65535;MEM16(0x74d098+port*0x19+0x17)=32767;
    }
    const unsigned rates[]={60,60,120,144,180,240};
    for(unsigned mode=0;mode<sizeof rates/sizeof rates[0];mode++) {
        unsigned fps=rates[mode],edges[4]={0},releases[4]={0},changes[4]={0};
        memset(memory+RECORD,0,4*0x3c);memset(polls,0,sizeof polls);memset(feedback,0,sizeof feedback);
        for(unsigned port=0;port<4;port++)MEM32(RECORD+port*0x3c+0x34)=37+port;
        g_tsfp_native_timing_active=mode!=0;
        for(unsigned frame=0;frame<fps/2;frame++) {
            unsigned whole=(frame+1)*60/fps-frame*60/fps;
            buttons=frame>=5 && frame<17?(uint16_t)MEM32(0x47df00):0;
            sign=frame%2?-1:1;
            for(unsigned port=0;port<4;port++) {
                uint32_t record=RECORD+port*0x3c,before=MEM32(record+0x1c),held=MEM32(record+12);
                uint32_t previous_axes[4];for(unsigned axis=0;axis<4;axis++)previous_axes[axis]=MEM32(record+0x1c+axis*4);
                sample(port,whole);
                assert(MEM32(record+4)==1);
                assert(sign>0?MEM32(record+0x1c)>128:MEM32(record+0x1c)<128);
                if(frame)assert(MEM32(record+0x1c)!=before);
                if(frame)for(unsigned axis=0;axis<4;axis++)assert(MEM32(record+0x1c+axis*4)!=previous_axes[axis]);
                changes[port]+=MEM32(record+0x1c)!=before;
                if(mode && !whole)assert(MEM32(record+12)==held && !MEM32(record+16) && !MEM32(record+20));
                edges[port]+=MEM32(record+16)!=0;releases[port]+=MEM32(record+20)!=0;
            }
        }
        for(unsigned port=0;port<4;port++) {
            assert(polls[port]==fps/2 && feedback[port]==fps/2);
            assert(edges[port]==1 && releases[port]==1 && changes[port]==fps/2);
        }
        printf("analog native=%d fps=%u ports=4 fresh_samples=%u edges=1 releases=1\n",mode!=0,fps,fps/2);fflush(stdout);
    }
    /* A fractional paused interval still refreshes sticks; catch-up polls once. */
    MEMF(0x4e794c)=0;buttons=0;sign=1;sample(0,0);sign=-1;sample(0,0);
    unsigned before=polls[0];sample(0,3);assert(polls[0]==before+1);
    /* Trigger axes refresh immediately while their digital fire edges retain
     * the published tick cadence. Rumble is an unchanged absolute command. */
    triggers=255;sample(0,0);
    assert(MEM32(RECORD+0x2c)>128 && MEM32(RECORD+0x30)>128 && MEM32(RECORD+16)==0);
    sample(0,1);assert(MEM32(RECORD+16)!=0);
    triggers=0;sample(0,0);
    assert(MEM32(RECORD+0x2c)==128 && MEM32(RECORD+0x30)==128 && MEM32(RECORD+20)==0);
    sample(0,1);assert(MEM32(RECORD+20)!=0);
    /* Device disappearance refreshes its connection/neutral-axis state. */
    MEM32(0x74d0fc)=0;sample(0,0);
    assert(MEM32(RECORD+4)==0 && MEM32(RECORD+0x1c)==128 && MEM32(RECORD+0x28)==128);
    /* Disabled/native-off path retains the original zero-tick button behavior. */
    MEM32(0x74d0fc)=15;g_tsfp_native_timing_active=0;buttons=(uint16_t)MEM32(0x47df00);
    sample(0,0);assert(MEM32(RECORD+16)==MEM32(0x47df04));
    free(memory);puts("PASS: original analogue decoding is fresh at native cadence with one-shot digital edges");return 0;
}
