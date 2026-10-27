/* Compile the complete production survey, with observable host boundaries. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x); return 1; } } while (0)
static int scan_policy, exec_policy;
static unsigned env_calls, offset_calls, executed, reports;
static uint32_t memory[16], methods[4], parameters[4], channels[4];
static char *policy_getenv(const char *name)
{
    ++env_calls;
    /* Presence, including empty and "0", retains the existing semantics. */
    if (!strcmp(name,"RECOMP_PB_SCAN")) return scan_policy ? (scan_policy==1?"":"0") : NULL;
    if (!strcmp(name,"RECOMP_PB_EXEC")) return exec_policy ? (exec_policy==1?"":"0") : NULL;
    abort();
}
ptrdiff_t xbox_GetMemoryOffset(void) { ++offset_calls; return (ptrdiff_t)(uintptr_t)memory; }
void nv2a_pb_exec_method(uint32_t channel,uint32_t method,uint32_t parameter)
{
    if (executed>=4) abort();
    channels[executed]=channel;methods[executed]=method;parameters[executed++]=parameter;
}
void nv2a_pb_exec_report(void) { ++reports; }
#define getenv policy_getenv
/* PRODUCTION_SCAN */
#undef getenv
int main(int argc,char **argv)
{
    CHECK(argc==3);
    scan_policy=atoi(argv[1]);exec_policy=atoi(argv[2]);
    nv2a_pb_scan_report();CHECK(env_calls==0 && offset_calls==0 && reports==0);
    for(unsigned i=0;i<10000;i++) nv2a_pb_scan(12,4); /* invalid span */
    CHECK(env_calls==2 && offset_calls==0 && executed==0);
    if (!scan_policy && !exec_policy) {
        /* Disabled paths cannot dereference even an unusable guest interval. */
        for(unsigned i=0;i<10000;i++) nv2a_pb_scan(0xfffff000u,0xfffffff0u);
        nv2a_pb_scan_report();
        CHECK(env_calls==2 && offset_calls==0 && executed==0 && reports==0);
        CHECK(s_seen_count==0 && s_tot_segments==0);
        puts("PASS: disabled diagnostics do not scan environment or guest memory");return 0;
    }
    memory[1]=(2u<<18)|(3u<<13)|0x100u;memory[2]=11;memory[3]=22;
    memory[4]=0x40000000u|(2u<<18)|(1u<<13)|0x300u;memory[5]=33;memory[6]=44;
    memory[7]=1; /* jump terminates the segment before the next command */
    memory[8]=(1u<<18)|0x400u;memory[9]=55;
    nv2a_pb_scan(4,40);
    CHECK(env_calls==2 && offset_calls==1 && s_seen_count==3);
    CHECK(s_seen[0].method==0x100 && s_seen[0].subch==3 && s_seen[0].count==1);
    CHECK(s_seen[1].method==0x104 && s_seen[1].count==1);
    CHECK(s_seen[2].method==0x300 && s_seen[2].subch==1 && s_seen[2].count==2);
    CHECK(s_tot_words==7 && s_tot_jumps==1 && s_tot_unknown==0 && s_tot_segments==1);
    CHECK(executed==(exec_policy?4u:0u));
    if(exec_policy) {
        CHECK(methods[0]==0x100 && methods[1]==0x104 && methods[2]==0x300 && methods[3]==0x300);
        CHECK(parameters[0]==11 && parameters[1]==22 && parameters[2]==33 && parameters[3]==44);
        CHECK(channels[0]==3 && channels[1]==3 && channels[2]==1 && channels[3]==1);
    }
    nv2a_pb_scan_report();
    CHECK(reports==(exec_policy?1u:0u) && env_calls==2 && offset_calls==1);
    puts("PASS: scan and executor modes retain decoding and independent reports");return 0;
}
