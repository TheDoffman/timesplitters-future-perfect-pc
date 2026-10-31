/* Actual proposed bridge/getter/native bodies with controlled host services. */
#include "guest_memory_services.h"
#include "host_pages.h"
#include "host_memory_stats.h"
#include "host_virtual_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __stdcall
typedef void *PVOID;
typedef void VOID;
typedef uint32_t ULONG;
typedef uintptr_t ULONG_PTR;
typedef unsigned char BOOLEAN;
typedef int32_t NTSTATUS;
typedef struct { uint32_t words[9]; } XBOX_MM_STATISTICS,*PXBOX_MM_STATISTICS;
#define XBOX_CONTIG_BASE 0x80000000u
#define XBOX_CONTIG_SIZE 0x40000u
#define XBOX_GPU_INSTANCE_DEFAULT 0x2000u
#define XBOX_KERNEL_DATA_BASE 0x1000u
#define KDATA_MMGLOBAL 0x510u
static unsigned char ram[0x20000],physical[XBOX_CONTIG_SIZE];
static tsfp_allocations owner;
static tsfp_allocation_record heaps[32],physicals[32];
static uint32_t args[5],g_eax,denied;
static unsigned stats_calls,queries,protects;
static int host_failure;
static void *protected_address;
static uint32_t protected_bytes,protected_flags;
#define STACK_ARG(n) args[n]
static void *resolve(void *host,uint32_t address,uint32_t bytes,int access) {
    assert(host==ram);
    if(address<0x1000 || !bytes || (uint64_t)address+bytes>sizeof(ram))return NULL;
    if(access==1 && denied && address<=denied && (uint64_t)address+bytes>denied)return NULL;
    return ram+address;
}
static void xbox_GetGuestVirtualMemory(tsfp_guest_vm *vm) {
    memset(vm,0,sizeof(*vm));vm->host=ram;vm->resolve=resolve;vm->allocations=&owner;
}
int tsfp_host_pages_query(const void *address,tsfp_host_page_region *out) {
    assert((uintptr_t)address>UINT32_MAX);++queries;
    if(host_failure)return 0;
    out->base=(uintptr_t)address;out->bytes=4096;out->protection=4;return 1;
}
int tsfp_host_pages_protect(void *address,size_t bytes,uint32_t protection) {
    assert((uintptr_t)address>UINT32_MAX);++protects;
    protected_address=address;protected_bytes=(uint32_t)bytes;protected_flags=protection;
    return !host_failure;
}
int tsfp_host_available_memory(uint64_t *bytes) {
    ++stats_calls;*bytes=UINT64_C(1)<<40;return !host_failure;
}
#include "guest_mm_adapters.inc"
static uint32_t get(uint32_t address) {
    uint32_t word=0;
    for(unsigned i=0;i<4;++i)word|=(uint32_t)ram[address+i]<<(8*i);
    return word;
}
int main(void) {
    tsfp_allocation_config config={0};
    uint32_t a,b;
    XBOX_MM_STATISTICS native,before;
    uintptr_t pointer=UINT64_C(0x123456780000);
    config.ram=ram;config.ram_size=sizeof(ram);config.heap_begin=0x4000;config.reserve_begin=0x18000;
    config.physical=physical;config.physical_base=XBOX_CONTIG_BASE;config.physical_size=XBOX_CONTIG_SIZE;
    config.physical_begin=XBOX_CONTIG_BASE+0x10000;config.physical_end=XBOX_CONTIG_BASE+XBOX_CONTIG_SIZE-XBOX_GPU_INSTANCE_DEFAULT;
    assert(tsfp_allocations_init(&owner,&config,heaps,32,physicals,32));
    memset(ram,0x5a,sizeof(ram));memset(physical,0x7a,sizeof(physical));
    args[0]=64;bridge_MmAllocateContiguousMemory();a=g_eax;
    assert(a==XBOX_CONTIG_BASE+0x10000);
    args[0]=128;args[1]=0x7001;args[2]=0x7080;args[3]=4096;args[4]=1;
    bridge_MmAllocateContiguousMemoryEx();b=g_eax;assert(b==XBOX_CONTIG_BASE+0x7001);
    args[0]=b;bridge_MmQueryAllocationSize();assert(g_eax==128);
    bridge_MmFreeContiguousMemory();assert(!g_eax);
    args[0]=a;bridge_MmFreeContiguousMemory();assert(!owner.physical.count);
    args[0]=23;args[1]=1;bridge_MmAllocateSystemMemory();a=g_eax;
    assert(a==0x4000 && owner.heap.count==1 && ram[a]==0);
    args[0]=a;args[1]=UINT32_MAX;bridge_MmFreeSystemMemory();assert(!owner.heap.count && !g_eax);
    args[0]=19;args[1]=0xabcdef;bridge_MmDbgAllocateMemory();a=g_eax;
    assert(a==0x4000 && owner.heap.count==1);
    args[0]=a;bridge_MmDbgFreeMemory();assert(!owner.heap.count && !g_eax);
    args[0]=UINT32_MAX;args[1]=15;args[2]=1;bridge_MmMapIoSpace();a=g_eax;
    assert(a==0x4000 && tsfp_allocation_remaining(&owner,a)==16 && !protects);
    args[0]=a;bridge_MmUnmapIoSpace();assert(!owner.heap.count && !g_eax);
    args[0]=33;args[1]=1;bridge_MmCreateKernelStack();a=g_eax;
    assert(a==0x4000+33 && tsfp_allocation_remaining(&owner,a-1)==1);
    args[0]=UINT32_MAX;args[1]=a-33;bridge_MmDeleteKernelStack();assert(!owner.heap.count && !g_eax);
    args[0]=31;bridge_MmAllocateSystemMemory();a=g_eax;
    args[0]=a;bridge_MmFreeContiguousMemory();assert(!owner.heap.count);

    args[0]=0x2001;denied=0x2001+35;bridge_MmQueryStatistics();
    assert(g_eax==UINT32_C(0xc000000d) && !stats_calls && ram[0x2001]==0x5a);
    denied=0;bridge_MmQueryStatistics();assert(!g_eax && get(0x2001)==36 && get(0x2009)==8192);
    assert(ram[0x2000]==0x5a && ram[0x2025]==0x5a);
    memset(&before,0x39,sizeof(before));native=before;
    assert((uint32_t)xbox_MmQueryStatistics(NULL)==UINT32_C(0xc000000d));
    host_failure=1;assert((uint32_t)xbox_MmQueryStatistics(&native)==UINT32_C(0xc0000001));
    assert(!memcmp(&native,&before,sizeof(native)));
    host_failure=0;assert(!xbox_MmQueryStatistics(&native));
    assert(native.words[0]==36 && native.words[1]==16384 && native.words[2]==8192);
    for(unsigned i=3;i<9;++i)assert(!native.words[i]);
    assert(xbox_MmGetPhysicalAddress((void *)pointer)==pointer);
    xbox_MmPersistContiguousMemory((void *)pointer,UINT32_MAX,1);
    xbox_MmLockUnlockBufferPages((void *)pointer,UINT32_MAX,1);
    xbox_MmLockUnlockPhysicalPage(pointer,1);

    args[0]=UINT32_MAX;args[1]=0x2101;bridge_MmClaimGpuInstanceMemory();
    assert(g_eax==XBOX_CONTIG_BASE+XBOX_CONTIG_SIZE-XBOX_GPU_INSTANCE_DEFAULT && !get(0x2101));
    args[1]=(uint32_t)sizeof(ram)-3;bridge_MmClaimGpuInstanceMemory();assert(!g_eax);
    bridge_MmGlobalData();assert(g_eax==0x1510 && get(0x1510)==0x1510);
    denied=0x1513;bridge_MmGlobalData();assert(!g_eax);denied=0;
    args[0]=XBOX_CONTIG_BASE+0x12345;bridge_MmGetPhysicalAddress();assert(g_eax==0x12345);
    args[0]=0x12345;bridge_MmGetPhysicalAddress();assert(g_eax==0x12345);
    args[0]=0;bridge_MmQueryAddressProtect();assert(!g_eax && !queries);
    args[0]=0x2111;bridge_MmQueryAddressProtect();assert(g_eax==4 && queries==1);
    args[0]=0x2211;args[1]=13;args[2]=0x120;bridge_MmSetAddressProtect();
    assert(!g_eax && protected_address==ram+0x2211 && protected_bytes==13 && protected_flags==0x20);
    host_failure=1;bridge_MmSetAddressProtect();assert(!g_eax);
    args[0]=UINT32_MAX;bridge_MmSetAddressProtect();assert(!g_eax && protects==2);host_failure=0;
    bridge_MmIsAddressValid();assert(g_eax==1);
    bridge_MmDbgQueryAvailablePages();assert(g_eax==0x10000);
    bridge_MmDbgWriteCheck();assert(g_eax==UINT32_MAX);
    bridge_MmDbgReleaseAddress();assert(!g_eax);
    bridge_MmPersistContiguousMemory();assert(!g_eax);
    bridge_MmLockUnlockBufferPages();assert(!g_eax);
    bridge_MmLockUnlockPhysicalPage();assert(!g_eax);
    assert(!owner.heap.count && !owner.physical.count);
    puts("actual guest Mm bridges, host adapters and native statistics passed");return 0;
}
