/* Bounded production PC reporting with observable symbol/line services. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %d: %s\n",__LINE__,#x);exit(2); } } while(0)
typedef uint32_t DWORD;
typedef uint64_t DWORD64;
typedef struct { DWORD SizeOfStruct,LineNumber; char *FileName; } IMAGEHLP_LINE64;
typedef struct { uintptr_t addr;unsigned count; } SampleSlot;
typedef struct { SampleSlot *leaf; } SampleThread;
typedef struct { uintptr_t base;unsigned symbol_complete;char name[96]; } ResolvedAddr;
#define SAMPLE_SLOTS 1024u
static uintptr_t s_exe_lo=1000,s_exe_hi=10000;
static char s_pc_symbol[96];
static unsigned resolves,lines;
static const ResolvedAddr *resolve(uintptr_t addr) {
    static ResolvedAddr r;resolves++;
    if(addr==1200)return NULL;
    r.base=addr==1310?1280:addr;
    r.symbol_complete=addr!=1320;
    strcpy(r.name,addr==1100?"target_extra":"target");return &r;
}
static void *GetCurrentProcess(void) { return (void *)(uintptr_t)1; }
static int SymGetLineFromAddr64(void *process,DWORD64 addr,DWORD *displacement,IMAGEHLP_LINE64 *line) {
    CHECK(process==GetCurrentProcess());CHECK(line->SizeOfStruct==sizeof(*line));++lines;
    *displacement=0;
    if(addr==1300)return 0;
    line->FileName=addr%3==0?NULL:"C:\\private\\recomp_0004.c";
    line->LineNumber=77;return 1;
}
/* PRODUCTION_PC_REPORT */
static SampleSlot slots[SAMPLE_SLOTS];
static char output[100000];
static void read_report(FILE *out) {
    size_t n;CHECK(!fflush(out));rewind(out);n=fread(output,1,sizeof(output)-1,out);
    CHECK(!ferror(out));output[n]=0;
}
int main(void) {
    SampleThread thread={slots};FILE *out=tmpfile();CHECK(out);
    report_pc_locations(&thread,out);CHECK(ftell(out)==0 && resolves==0 && lines==0);
    report_pc_locations(&thread,NULL);CHECK(resolves==0 && lines==0);
    CHECK(!pc_symbol_setting(NULL));CHECK(!pc_symbol_setting(""));
    CHECK(!pc_symbol_setting("bad\tname"));CHECK(!pc_symbol_setting("bad\nname"));
    char boundary[97];memset(boundary,'x',96);boundary[96]=0;
    CHECK(!pc_symbol_setting(boundary));boundary[95]=0;CHECK(pc_symbol_setting(boundary));
    CHECK(!pc_symbol_setting("bad\rname") && !s_pc_symbol[0]);
    CHECK(pc_symbol_setting("target"));
    slots[0]=(SampleSlot){0,99};slots[1]=(SampleSlot){999,3};slots[2]=(SampleSlot){10000,5};
    slots[3]=(SampleSlot){1100,7};slots[4]=(SampleSlot){1200,9};
    slots[5]=(SampleSlot){1300,11};slots[6]=(SampleSlot){1310,13};
    slots[7]=(SampleSlot){1320,17}; /* incomplete symbol names must not match */
    report_pc_locations(&thread,out);read_report(out);
    CHECK(resolves==5 && lines==2);
    CHECK(strstr(output,"pc\t11\t0x12c\t0x0\t?\t0\n"));
    CHECK(strstr(output,"pc\t13\t0x136\t0x1e\trecomp_0004.c\t77\n"));
    CHECK(!strstr(output,"private"));
    CHECK(strstr(output,"symbol=target addresses=2 emitted=2 samples=24 omitted_samples=0\n"));
    fclose(out);out=tmpfile();CHECK(out);resolves=lines=0;memset(slots,0,sizeof(slots));
    for(unsigned i=0;i<517;i++)slots[i]=(SampleSlot){1500+8*i,i+1};
    report_pc_locations(&thread,out);read_report(out);
    CHECK(resolves==517 && lines==512);
    CHECK(strstr(output,"addresses=517 emitted=512 samples=133903 omitted_samples=2575\n"));
    unsigned rows=0;for(const char *s=output;(s=strstr(s,"\npc\t"));s+=4)rows++;
    CHECK(rows==512 && strlen(output)<40000);fclose(out);
    puts("PASS: opt-in PC locations, exact symbol, source fallback/privacy and bounded coverage");return 0;
}
