/* Original SDK-shaped fixture: padding deliberately prevents offset parsing. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(2);}}while(0)
#define WINAPI
typedef int BOOL;
typedef uint32_t DWORD,ULONG,*PULONG;
typedef uintptr_t DWORD_PTR;
typedef void *HANDLE;
enum {CpuSetInformation=0};
typedef struct {
    DWORD Size,Type;
    unsigned char reserved_test_padding[23];
    struct {unsigned Group,LogicalProcessorIndex,EfficiencyClass;} CpuSet;
} SYSTEM_CPU_SET_INFORMATION,*PSYSTEM_CPU_SET_INFORMATION;
static int query_present=1,query_ok=1,affinity_ok=1,oversized;
static DWORD_PTR test_allowed=255;static unsigned char records[1024];static ULONG count;
static const char *test_request="1";
static const char *cpu_environment(const char *name){CHECK(!strcmp(name,"RECOMP_GUEST_ONE_CPU"));return test_request;}
static BOOL GetProcessAffinityMask(HANDLE h,DWORD_PTR *a,DWORD_PTR *b){CHECK(h);*a=test_allowed;*b=test_allowed;return affinity_ok;}
static HANDLE GetCurrentProcess(void){return (HANDLE)(uintptr_t)1;}
static HANDLE GetModuleHandleW(const void *p){CHECK(p);return GetCurrentProcess();}
static BOOL query(PSYSTEM_CPU_SET_INFORMATION buffer,ULONG capacity,PULONG size,HANDLE process,ULONG flags) {
    CHECK(process && !flags);*size=count;
    if(!buffer)return 0;
    CHECK(capacity>=count);memcpy(buffer,records,count);
    if(oversized)*size=capacity+1;return query_ok;
}
static void *GetProcAddress(HANDLE module,const char *name) {
    CHECK(module && !strcmp(name,"GetSystemCpuSetInformation"));return query_present?(void *)query:NULL;
}
#include "guest_cpu_choice.h"
#define getenv cpu_environment
#include "guest_execution_cpu.inc"
#undef getenv
static void add(unsigned group,unsigned index,unsigned rank) {
    SYSTEM_CPU_SET_INFORMATION item;memset(&item,0,sizeof item);item.Size=sizeof item;
    item.CpuSet.Group=group;item.CpuSet.LogicalProcessorIndex=index;item.CpuSet.EfficiencyClass=rank;
    CHECK(count+sizeof item<=sizeof records);memcpy(records+count,&item,sizeof item);count+=sizeof item;
}
int main(void) {
    unsigned i;char text[80];tsfp_cpu_choice best={0,0};
    const char *invalid[]={NULL,"","0","-1","66","999999999999999999999999999999999999","1x","0x","2 ","-0"};
    for(i=0;i<sizeof invalid/sizeof *invalid;++i)CHECK(!tsfp_cpu_request(invalid[i],UINT64_MAX,0));
    for(i=0;i<64;++i){snprintf(text,sizeof text,"%u",i+2);CHECK(tsfp_cpu_request(text,UINT64_MAX,0)==(UINT64_C(1)<<i));}
    CHECK(tsfp_cpu_request("1",UINT64_MAX,0)==(UINT64_C(1)<<63));
    CHECK(!tsfp_cpu_request("1",0,0));CHECK(!tsfp_cpu_request("4",3,0));
    CHECK(tsfp_cpu_request("1",10,2)==2);CHECK(tsfp_cpu_request("1",10,3)==8);
    tsfp_cpu_consider(&best,255,0,6,5);tsfp_cpu_consider(&best,255,0,2,5);
    tsfp_cpu_consider(&best,255,1,7,255);tsfp_cpu_consider(&best,255,0,90,255);
    CHECK(best.mask==64);tsfp_cpu_consider(&best,255,0,1,9);CHECK(best.mask==2);
    add(0,6,5);add(0,2,5);add(1,7,255);add(0,70,255);add(0,1,9);
    CHECK(execution_cpu_configure()==2);
    test_allowed=253;CHECK(execution_cpu_configure()==64);test_allowed=255;
    test_request="5";CHECK(execution_cpu_configure()==8);test_request="1";
    query_present=0;CHECK(execution_cpu_configure()==128);query_present=1;
    query_ok=0;CHECK(execution_cpu_configure()==128);query_ok=1;
    oversized=1;CHECK(execution_cpu_configure()==128);oversized=0;
    count=8;memset(records,0,sizeof records);CHECK(execution_cpu_configure()==128);
    {DWORD size=UINT32_MAX;memcpy(records,&size,sizeof size);CHECK(execution_cpu_configure()==128);}
    {DWORD size=4;memcpy(records,&size,sizeof size);CHECK(execution_cpu_configure()==128);}
    affinity_ok=0;CHECK(!execution_cpu_configure());affinity_ok=1;
    test_request=NULL;CHECK(!execution_cpu_configure());puts("PASS: original CPU affinity policy");return 0;
}
