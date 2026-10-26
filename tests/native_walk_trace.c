/* The actual diagnostic bridge, with controlled original collision services.
 * This tests observation/dispatch/ABI, not collision or gameplay equivalence. */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static unsigned char memory[0x800000];
ptrdiff_t g_xbox_mem_offset;
int g_tsfp_native_timing_active;
static uint32_t g_esp,g_eax,denied;
#define WORD(a) (*(uint32_t *)(memory+(a)))
static int read_guest(uint32_t a,void *out,size_t n)
{
    if(a==denied || a>=sizeof memory || n>sizeof memory-a)return 0;
    memcpy(out,memory+a,n);return 1;
}
#include "../src/pc_native_numeric.h"
#define PC_WALK_READ read_guest
#include "../src/pc_native_walk_trace_guest.inc"
enum { STACK=0x18000,CHILD=0x17000,CONTEXT=0x20000,OLD=0x21000,NEW=0x22000,OUT=0x23000 };
static unsigned roots,sweeps;
static void arguments(uint32_t sp)
{
    WORD(sp)=0x12345678;
    WORD(sp+4)=CONTEXT;WORD(sp+8)=OLD;WORD(sp+12)=NEW;
    for(unsigned i=4;i<=10;i++)WORD(sp+i*4)=0;
    WORD(sp+28)=OUT;
}
void sub_000C4700(void)
{
    sweeps++;assert(g_esp==CHILD);
    WORD(NEW)=0x3f800000;WORD(OUT+12)=0x3f000000;WORD(CONTEXT+24)=1;
    WORD(g_esp+12)=0; /* The real routine may reuse argument slots. */
    g_eax=6;g_esp+=4;
}
void sub_000C54A0(void)
{
    roots++;assert(g_esp==STACK);
    arguments(CHILD);g_esp=CHILD;
    RECOMP_DIRECT_CALLEE_AT(0xC55E6u,sub_000C4700)();
    assert(g_esp==CHILD+4 && g_eax==6);
    g_esp=STACK;WORD(STACK+12)=0;g_eax=0x123abc;g_esp+=4;
}
static void run(void)
{
    arguments(STACK);g_esp=STACK;g_eax=0;
    RECOMP_DIRECT_CALLEE_AT(0xC5AD6u,sub_000C54A0)();
    assert(g_esp==STACK+4 && g_eax==0x123abc);
    assert(!pc_walk_depth && !pc_walk_parent);
}
int main(void)
{
    g_xbox_mem_offset=(ptrdiff_t)(uintptr_t)memory;
    WORD(0x7497b8)=9;WORD(0x79094c)=46;WORD(0x7de318)=2100;
    WORD(0x4e7948)=WORD(0x4e794c)=0x3e800000;
    PcWalkTrace options={0};
    assert(pc_walk_options(&options,NULL,NULL) && options.scene==46 && options.first_tick==2000);
    assert(pc_walk_options(&options,"49","0,4294967295"));
    const char *invalid[]={"-1"," 46","46x","4294967296",""};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++)assert(!pc_walk_options(&options,invalid[i],NULL));
    const char *ranges[]={"2,1","1,","1,2,3",",2","-1,2","0,4294967296"};
    for(unsigned i=0;i<sizeof ranges/sizeof ranges[0];i++)assert(!pc_walk_options(&options,NULL,ranges[i]));
    assert(!pc_native_walk_trace_requested());
    assert(RECOMP_DIRECT_CALLEE_AT(0xC5AD6u,sub_000C54A0)==sub_000C54A0);
    run();assert(roots==1 && sweeps==1 && !pc_walk_trace.rows);
    pc_walk_trace.file=tmpfile();assert(pc_walk_trace.file);
    assert(pc_walk_options(&pc_walk_trace,NULL,NULL));
    assert(RECOMP_DIRECT_CALLEE_AT(0xC5AD6u,sub_000C54A0)==pc_native_walk_collision);
    assert(RECOMP_DIRECT_CALLEE_AT(0xC55E6u,sub_000C4700)==pc_native_walk_sweep);
    assert(RECOMP_DIRECT_CALLEE_AT(0xC5AD5u,sub_000C54A0)==sub_000C54A0);
    assert(RECOMP_DIRECT_CALLEE_AT(0xC55E5u,sub_000C4700)==sub_000C4700);
    WORD(NEW)=0;run();assert(roots==2 && sweeps==2 && pc_walk_trace.rows==2);
    assert(pc_walk_trace.bytes==(unsigned)ftell(pc_walk_trace.file));
    rewind(pc_walk_trace.file);char line[4096];
    assert(fgets(line,sizeof line,pc_walk_trace.file));
    assert(strstr(line,"2,1,1,000c4700") && strstr(line,"3f000000;"));
    assert(fgets(line,sizeof line,pc_walk_trace.file));assert(strstr(line,"1,0,0,000c54a0"));
    fseek(pc_walk_trace.file,0,SEEK_END);
    WORD(0x7de318)=2301;run();assert(pc_walk_trace.rows==2 && roots==3 && sweeps==3);
    WORD(0x7de318)=2100;WORD(0x79094c)=49;run();assert(pc_walk_trace.rows==2);
    WORD(0x79094c)=46;
    WORD(CONTEXT)=1;run();assert(pc_walk_trace.rows==2);WORD(CONTEXT)=0;
    PcWalkSnapshot before,after;arguments(STACK);
    assert(pc_walk_collect(read_guest,STACK,10,NULL,&before));
    WORD(STACK+12)=0;
    assert(!pc_walk_collect(read_guest,STACK,10,NULL,&after));
    assert(pc_walk_collect(read_guest,STACK,10,&before,&after));
    assert(after.arguments[2]==NEW);
    denied=NEW;assert(!pc_walk_collect(read_guest,STACK,10,&before,&after));denied=0;
    assert(!pc_walk_collect(read_guest,0xfffffff0u,10,NULL,&after));
    assert(!pc_walk_collect(read_guest,STACK,11,NULL,&after));
    after=before;after.frame++;pc_walk_record(&pc_walk_trace,3,0,0,0,STACK,STACK+4,0,&before,&after);
    assert(pc_walk_trace.rows==2);
    after=before;pc_walk_record(&pc_walk_trace,3,0,0,0,STACK,STACK,0,&before,&after);
    assert(pc_walk_trace.rows==2);
    pc_walk_trace.bytes=PC_WALK_BYTES-1;
    pc_walk_record(&pc_walk_trace,3,0,0,0,STACK,STACK+4,0,&before,&after);
    assert(!pc_walk_trace.file && pc_walk_trace.rows==2);
    pc_walk_trace.file=tmpfile();assert(pc_walk_trace.file);pc_walk_trace.rows=PC_WALK_ROWS;
    assert(!pc_native_walk_trace_requested() && !pc_walk_trace.file);
    puts("PASS: bounded synchronous walk trace, nested pairs, pointer anchors, guards and disabled dispatch");
    return 0;
}
