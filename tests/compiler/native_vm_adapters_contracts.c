#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "native_vm_host.h"
#include "host_virtual_memory.h"
#define __stdcall
typedef void *PVOID;
typedef size_t SIZE_T,*PSIZE_T;
typedef uint32_t DWORD,ULONG,*PULONG;
typedef uintptr_t ULONG_PTR;
typedef int32_t NTSTATUS;
typedef struct {
    void *BaseAddress,*AllocationBase;
    DWORD AllocationProtect;
#if defined(_WIN32)
    uint16_t PartitionId;
#endif
    size_t RegionSize;
    DWORD State,Protect,Type;
} MEMORY_BASIC_INFORMATION;
static const uintptr_t pointer=UINT64_C(0x123450000),allocation=UINT64_C(0x123440000);
static const size_t extent=UINT64_C(0x200001000);
static int failing;
static unsigned allocates,releases,queries;
void *tsfp_native_vm_allocate(void *hint,size_t bytes,uint32_t type,uint32_t protection) {
    assert((uintptr_t)hint==allocation && bytes==extent && type==0x3000 && protection==32);
    ++allocates;return failing?NULL:(void *)pointer;
}
int tsfp_native_vm_release(void *base,size_t bytes,uint32_t type) {
    assert((uintptr_t)base==pointer && !bytes && type==0x8000);++releases;return !failing;
}
int tsfp_native_vm_query(const void *base,tsfp_native_vm_region *out) {
    assert((uintptr_t)base==pointer);++queries;
    memset(out,0xa5,sizeof(*out));if(failing)return 0;
    out->base=pointer;out->allocation_base=allocation;out->bytes=extent;
    out->allocation_protection=32;out->state=0x1000;out->protection=2;out->type=0x20000;
    out->partition_id=42;return 1;
}
#include "native_vm_adapters.inc"
int main(void) {
    void *base=(void *)allocation;
    size_t size=extent;
    uint32_t length;
    MEMORY_BASIC_INFORMATION expected;
    unsigned char output[sizeof(expected)+8],before[sizeof(output)];
    memset(&expected,0,sizeof(expected));
    expected.BaseAddress=(void *)pointer;expected.AllocationBase=(void *)allocation;
    expected.AllocationProtect=32;expected.RegionSize=extent;expected.State=0x1000;
    expected.Protect=2;expected.Type=0x20000;
#if defined(_WIN32)
    expected.PartitionId=42;
#endif
    assert(xbox_NtAllocateVirtualMemory(&base,19,&size,0x3000,0x120)==0);
    assert((uintptr_t)base==pointer && size==extent && allocates==1);
    for(uint32_t n=0;n<=sizeof(output);++n) {
        size_t count=n<sizeof(expected)?n:sizeof(expected);
        memset(output,0x5a,sizeof(output));length=UINT32_MAX;
        assert(xbox_NtQueryVirtualMemory(base,output,n,&length)==0 && length==count);
        assert(!memcmp(output,&expected,count));
        for(size_t j=count;j<sizeof(output);++j)assert(output[j]==0x5a);
    }
    memset(output,0x5a,sizeof(output));memcpy(before,output,sizeof(output));
    failing=1;length=77;
    assert((uint32_t)xbox_NtQueryVirtualMemory(base,output,sizeof(output),&length)==0xc000000du);
    assert(length==77 && !memcmp(output,before,sizeof(output)));
    assert((uint32_t)xbox_NtFreeVirtualMemory(&base,&size,0x8000)==0xc0000001u);
    assert((uintptr_t)base==pointer && size==extent);
    base=(void *)allocation;
    assert((uint32_t)xbox_NtAllocateVirtualMemory(&base,0,&size,0x3000,0x120)==0xc0000017u);
    assert((uintptr_t)base==allocation && size==extent);
    failing=0;base=(void *)pointer;
    assert(xbox_NtFreeVirtualMemory(&base,&size,0x8000)==0 && !base && size==extent && releases==2);
    assert(queries && allocates==2);
    puts("actual native Nt adapters preserve widths, records and failure outputs");return 0;
}
