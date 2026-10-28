"""Execute the actual proposed runtime adapters with controlled host mappings."""
from pathlib import Path
import os
import subprocess
import sys
import pytest

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
from c_source_scan import mask_noncode


def body(text,name):
    clean=mask_noncode(text)
    at=clean.index(name+'(')
    begin=text.rfind('\n',0,at)+1
    end=clean.index('{',at)+1
    depth=1
    while depth:
        depth+=(clean[end]=='{')-(clean[end]=='}')
        end+=1
    return text[begin:end]+'\n'


@pytest.mark.parametrize('host_model', ['windows', 'posix'])
def test_actual_vm_adapters(tmp_path, host_model):
    kernel=HERE/'integration/src/kernel'
    if not kernel.exists(): kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    layout=(kernel/'xbox_memory_layout.c').read_text(encoding='utf-8')
    bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    native=(kernel/'kernel_memory.c').read_text(encoding='utf-8')
    guest=layout[layout.index('/* Project guest VM adapter:'):layout.index('/* End project guest VM adapter. */')]
    host=native[native.index('/* Project native VM boundary:'):native.index('/* End project native VM boundary. */')]
    host=body(native,'xbox_protect_to_win32')+host
    for name in ('NtAllocateVirtualMemory','NtFreeVirtualMemory','NtQueryVirtualMemory'):
        host+=body(native,'xbox_'+name)
    bridges=''.join(body(bridge,'bridge_'+name) for name in (
        'NtAllocateVirtualMemory','NtFreeVirtualMemory','NtQueryVirtualMemory','NtProtectVirtualMemory'))
    for name in ('guest_virtual_memory.c','guest_virtual_memory.h','guest_allocation.c',
                 'guest_allocation.h','host_virtual_memory.c','host_virtual_memory.h'):
        path=HERE/name
        if not path.exists(): path=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(path.read_bytes())
    # Native ABI typedefs use fixed-width LONG/ULONG and pointer-width SIZE_T
    # on both fixture hosts. No Win32 headers enter the portable contracts.
    harness=r'''
#include "guest_virtual_memory.h"
#include "host_virtual_memory.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define __stdcall
typedef void *PVOID;
typedef size_t SIZE_T,*PSIZE_T;
typedef uint32_t DWORD,ULONG,*PULONG;
typedef uintptr_t ULONG_PTR;
typedef int32_t NTSTATUS;
typedef struct { DWORD dwPageSize; } SYSTEM_INFO;
typedef struct { void *BaseAddress,*AllocationBase; DWORD AllocationProtect;
    size_t RegionSize; DWORD State,Protect,Type; } MEMORY_BASIC_INFORMATION;
#define XBOX_CONTIG_SIZE 0x10000u
#define XBOX_NUM_MIRRORS 2
#define XBOX_TOTAL_RAM 0x40000u
static unsigned char ram[0x80000];
static void *g_memory_base,*g_contig_memory,*g_tiled_view,*g_kernel_memory;
static void *g_mirror_views[XBOX_NUM_MIRRORS];
static size_t g_memory_size;
static ptrdiff_t g_memory_offset;
static uint32_t g_xbox_code_lo=0x12000;
static tsfp_allocations g_guest_allocations;
static tsfp_allocation_record records[64];
static uint32_t args[5],g_eax;
static int host_failure;
static unsigned host_allocations,host_queries,host_frees,host_protects;
static void *host_base;
static size_t host_size;
static uint32_t last_type,last_protection;
static uintptr_t ro_begin,ro_end;
#define STACK_ARG(n) args[n]
static size_t xbox_TiledApertureSize(void) { return XBOX_CONTIG_SIZE; }
#ifdef _WIN32
static void GetSystemInfo(SYSTEM_INFO *i) { i->dwPageSize=4096; }
#else
#define _SC_PAGESIZE 1
static long fixture_page_size(int selector) { assert(selector==1);return 16384; }
#define sysconf fixture_page_size
#endif
static size_t pc_guest_region(uintptr_t p,int writing) {
    uintptr_t start=(uintptr_t)ram,end=start+sizeof(ram);
    if(p<start || p>=end)return 0;
    if(p>=ro_begin && p<ro_end)return writing?0:ro_end-p;
    if(p<ro_begin && ro_begin<end)return ro_begin-p;
    return end-p;
}
static int VirtualProtect(void *p,size_t size,DWORD protection,DWORD *old) {
    ++host_protects;
    assert((uintptr_t)p>=(uintptr_t)ram && (uintptr_t)p+size<=(uintptr_t)ram+sizeof(ram));
    last_protection=protection;
    if(host_failure)return 0;
    *old=4;return 1;
}
static void *VirtualAlloc(void *hint,size_t size,DWORD type,DWORD protection) {
    assert(!hint); ++host_allocations; last_type=type; last_protection=protection;
    if(host_failure)return NULL;
    assert(!host_base); host_base=malloc(size); host_size=size; return host_base;
}
static int VirtualFree(void *address,size_t size,DWORD type) {
    ++host_frees; assert(address==host_base); last_type=type;
    if(host_failure)return 0;
    if(type & 0x8000) { assert(size==0); free(host_base); host_base=NULL; }
    return 1;
}
static size_t VirtualQuery(const void *address,MEMORY_BASIC_INFORMATION *out,size_t size) {
    ++host_queries; assert(address==host_base && size==sizeof(*out));
    if(host_failure)return 0;
    out->BaseAddress=host_base; out->AllocationBase=host_base;
    out->AllocationProtect=4; out->RegionSize=host_size;
    out->State=0x1000;out->Protect=4;out->Type=0x20000;
    return size;
}
''' + guest + host + bridges + r'''
static void put(uint32_t a,uint32_t n) { memcpy(ram+a,&n,4); }
static uint32_t get(uint32_t a) { uint32_t n;memcpy(&n,ram+a,4);return n; }
int main(void) {
    tsfp_allocation_config config={0};
    tsfp_guest_vm context;
    MEMORY_BASIC_INFORMATION record;
    PVOID native=NULL;
    SIZE_T size=12345;
    ULONG returned=0;
    memset(ram,0xa5,sizeof(ram));
    g_memory_base=ram; g_memory_offset=(ptrdiff_t)(uintptr_t)ram; g_memory_size=0x40000;
    config.ram=ram;config.ram_size=0x40000;config.heap_begin=0x10000;config.reserve_begin=0x40000;
    assert(tsfp_allocations_init(&g_guest_allocations,&config,records,64,NULL,0));
    xbox_GetGuestVirtualMemory(&context);
    assert(context.allocations==&g_guest_allocations && context.code_begin==0x12000);
    assert(context.ram_size==0x40000);
#ifdef _WIN32
    assert(context.host_page_size==4096);
#else
    assert(context.host_page_size==16384);
#endif
    assert(context.resolve(context.host,0x1000,4,1)==ram+0x1000);
    assert(!context.resolve(context.host,0x40000,4,1));
    assert(!context.resolve(context.host,0x3fffe,4,1));
    assert(!context.resolve(context.host,UINT32_MAX-2,4,0));
    g_kernel_memory=ram+0x48000;g_mirror_views[0]=ram+0x50000;
    g_contig_memory=ram+0x60000;g_tiled_view=ram+0x70000;
    assert(context.resolve(NULL,0x48000,4,1)==ram+0x48000);
    assert(context.resolve(NULL,0x50000,4,1)==ram+0x50000);
    assert(context.resolve(NULL,0x60000,4,1)==ram+0x60000);
    assert(context.resolve(NULL,0x70000,4,1)==ram+0x70000);
    ro_begin=(uintptr_t)ram+0x30000;ro_end=ro_begin+4096;
    assert(context.resolve(NULL,0x2fffe,4,0)==ram+0x2fffe);
    assert(!context.resolve(NULL,0x2fffe,4,1));
    assert(context.resolve(NULL,0x30000,4,-1)==ram+0x30000);
    put(0x2001,0);put(0x2013,32);
    args[0]=0x2001;args[1]=0;args[2]=0x2013;args[3]=0x3000;args[4]=4;
    bridge_NtAllocateVirtualMemory();assert(!g_eax && get(0x2001)==0x10000 && get(0x2013)==32);
    assert(host_allocations==0);
    args[0]=0x10003;args[1]=0x2101;
    bridge_NtQueryVirtualMemory();assert(!g_eax && get(0x2101)==0x10000 && get(0x210d)==4096);
    /* code begins at 0x12000: the earlier heap allocation is reported free. */
    assert(get(0x2111)==0x10000 && host_queries==0);
    args[0]=0x2001;args[1]=0x2013;args[2]=1;args[3]=0x2025;
    bridge_NtProtectVirtualMemory();assert(!g_eax && get(0x2025)==4 && host_protects==1 && last_protection==1);
    args[0]=0x2001;args[1]=0x2013;args[2]=0x8000;
    bridge_NtFreeVirtualMemory();assert(!g_eax && !get(0x2001) && !g_guest_allocations.heap.count);
    assert(host_frees==0);
    assert(xbox_protect_to_win32(0x120)==0x20);
    assert(xbox_NtAllocateVirtualMemory(&native,0,&size,0x3000,0x104)==0);
    assert(native==host_base && size==12345 && last_type==0x3000 && last_protection==4);
    assert(xbox_NtQueryVirtualMemory(native,&record,sizeof(record),&returned)==0);
    assert(record.BaseAddress==native && record.RegionSize==12345 && returned==sizeof(record));
    host_failure=1;returned=77;
    assert((uint32_t)xbox_NtQueryVirtualMemory(native,&record,sizeof(record),&returned)==0xc000000du && returned==77);
    host_failure=0;
    assert(xbox_NtFreeVirtualMemory(&native,&size,0x8000)==0 && !native && size==12345);
    puts("actual virtual-memory adapters passed"); return 0;
}
'''
    model = '#undef _WIN32\n' + ('#define _WIN32 1\n' if host_model == 'windows' else '')
    harness = harness.replace('#define __stdcall', model + '#define __stdcall', 1)
    path=tmp_path/'adapters.c'
    path.write_text(harness,encoding='utf-8')
    exe=_compile(tmp_path,path,extra=[tmp_path/'guest_virtual_memory.c',tmp_path/'guest_allocation.c',
         tmp_path/'host_virtual_memory.c',*(['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])])
    env=os.environ.copy();env['RECOMP_NTFREE_LEGACY']='0'
    run=subprocess.run([str(exe)],capture_output=True,text=True,env=env,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'adapters passed' in run.stdout


@pytest.mark.skipif(os.name!='nt',reason='Real Windows memory protections require Windows')
def test_windows_guest_mapping_access(tmp_path):
    kernel=HERE/'integration/src/kernel'
    if not kernel.exists(): kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    layout=(kernel/'xbox_memory_layout.c').read_text(encoding='utf-8')
    adapter=layout[layout.index('/* Project guest VM adapter:'):layout.index('/* End project guest VM adapter. */')]
    for name in ('guest_virtual_memory.h','guest_allocation.h'):
        path=HERE/name
        if not path.exists(): path=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(path.read_bytes())
    (tmp_path/'pc_guest_memory.h').write_bytes((ROOT/'src/runtime/pc_guest_memory.h').read_bytes())
    code=r'''
#include "guest_virtual_memory.h"
#include "pc_guest_memory.h"
#include <assert.h>
#include <stdio.h>
#define XBOX_CONTIG_SIZE 0x10000u
#define XBOX_NUM_MIRRORS 2
#define XBOX_TOTAL_RAM 0x40000u
static void *g_memory_base,*g_contig_memory,*g_tiled_view,*g_kernel_memory;
static void *g_mirror_views[XBOX_NUM_MIRRORS];
static size_t g_memory_size;
static ptrdiff_t g_memory_offset;
static uint32_t g_xbox_code_lo=0x12000;
static tsfp_allocations g_guest_allocations;
static size_t xbox_TiledApertureSize(void) { return XBOX_CONTIG_SIZE; }
''' + adapter + r'''
int main(void) {
    unsigned char *ram=VirtualAlloc(NULL,0x80000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    DWORD previous;
    tsfp_guest_vm context;
    uint32_t prior;
    assert(ram);
    g_memory_base=ram;g_memory_size=0x40000;g_memory_offset=(ptrdiff_t)(uintptr_t)ram;
    xbox_GetGuestVirtualMemory(&context);
    assert(context.resolve(NULL,0x2000,4,1)==ram+0x2000);
    assert(!context.resolve(NULL,0x40000,4,0));
    assert(VirtualProtect(ram+0x30000,4096,PAGE_READONLY,&previous));
    assert(context.resolve(NULL,0x2fffe,4,0)==ram+0x2fffe);
    assert(!context.resolve(NULL,0x2fffe,4,1));
    assert(VirtualProtect(ram+0x30000,4096,PAGE_NOACCESS,&previous));
    assert(!context.resolve(NULL,0x30000,4,0));
    assert(context.resolve(NULL,0x30000,4,-1)==ram+0x30000);
    assert(context.protect(NULL,0x30001,1,PAGE_READWRITE,&prior) && prior==PAGE_NOACCESS);
    assert(context.resolve(NULL,0x30000,4,1)==ram+0x30000);
    assert(VirtualFree(ram,0,MEM_RELEASE));
    puts("real Windows VM access passed");return 0;
}
'''
    path=tmp_path/'mapping.c';path.write_text(code,encoding='utf-8')
    exe=_compile(tmp_path,path,extra=['/W4'])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'Windows VM access passed' in run.stdout


@pytest.mark.skipif(os.name!='nt',reason='Real native VM calls require Windows')
def test_windows_native_vm_calls(tmp_path):
    kernel=HERE/'integration/src/kernel'
    if not kernel.exists(): kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    text=(kernel/'kernel_memory.c').read_text(encoding='utf-8')
    native=body(text,'xbox_protect_to_win32')+text[text.index('/* Project native VM boundary:'):text.index('/* End project native VM boundary. */')]
    for name in ('xbox_NtAllocateVirtualMemory','xbox_NtFreeVirtualMemory','xbox_NtQueryVirtualMemory'):
        native+=body(text,name)
    for name in ('host_virtual_memory.h','host_virtual_memory.c'):
        path=HERE/name
        if not path.exists(): path=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(path.read_bytes())
    code=r'''
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host_virtual_memory.h"
typedef LONG NTSTATUS;
''' + native + r'''
int main(void) {
    void *base=NULL,*original;
    SIZE_T bytes=8192;
    MEMORY_BASIC_INFORMATION info;
    ULONG length=99;
    unsigned char out[sizeof(info)+8];
    assert(xbox_protect_to_win32(0x140)==PAGE_EXECUTE_READWRITE);
    assert((uint32_t)xbox_NtAllocateVirtualMemory(&base,0,&bytes,0,4)==0xc0000017u);
    assert(!base && bytes==8192);
    assert(xbox_NtAllocateVirtualMemory(&base,0,&bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==0);
    assert(base && (uintptr_t)base>UINT32_MAX && bytes==8192); original=base;
    ((unsigned char *)base)[0]=31;((unsigned char *)base)[8191]=73;
    assert(xbox_NtQueryVirtualMemory(base,&info,sizeof(info),&length)==0);
    assert(info.BaseAddress==base && info.AllocationBase==base && info.RegionSize>=8192);
    assert(info.State==MEM_COMMIT && info.Protect==PAGE_READWRITE && length==sizeof(info));
    for(ULONG n=0;n<=sizeof(out);++n) {
        size_t copied=n<sizeof(info)?n:sizeof(info);
        memset(out,0xa5,sizeof(out));length=99;
        assert(xbox_NtQueryVirtualMemory(base,out,n,&length)==0 && length==copied);
        for(size_t i=copied;i<sizeof(out);++i)assert(out[i]==0xa5);
    }
    length=99;memset(out,0xa5,sizeof(out));
    assert((uint32_t)xbox_NtQueryVirtualMemory((void *)(UINTPTR_MAX-15),out,sizeof(out),&length)==0xc000000du);
    assert(length==99);
    for(size_t i=0;i<sizeof(out);++i)assert(out[i]==0xa5);
    assert((uint32_t)xbox_NtFreeVirtualMemory(&base,&bytes,0)==0xc0000001u);
    assert(base==original && bytes==8192);
    bytes=4096;
    assert(xbox_NtFreeVirtualMemory(&base,&bytes,MEM_DECOMMIT)==0 && base==original && bytes==4096);
    assert(xbox_NtFreeVirtualMemory(&base,&bytes,MEM_RELEASE)==0 && !base && bytes==4096);
    puts("real Windows native VM calls passed");return 0;
}
'''
    path=tmp_path/'host.c';path.write_text(code,encoding='utf-8')
    exe=_compile(tmp_path,path,extra=[tmp_path/'host_virtual_memory.c','/W4'])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'native VM calls passed' in run.stdout
