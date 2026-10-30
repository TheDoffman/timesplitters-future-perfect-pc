"""Compile all proposed guest Mm bridge bodies and the real context adapter."""
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
from test_virtual_memory_adapters import body


def build(tmp_path,mutation=None):
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    layout=(kernel/'xbox_memory_layout.c').read_text(encoding='utf-8')
    native=(kernel/'kernel_memory.c').read_text(encoding='utf-8')
    bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    adapters=layout[layout.index('/* Project guest Mm boundary:'):layout.index('/* End project guest Mm boundary. */')]
    adapters+=native[native.index('/* Project native memory statistics and'):native.index('/* End project native memory statistics adapter. */')]
    adapters+='\n'.join(body(native,name) for name in ('xbox_MmQueryStatistics', 'xbox_MmGetPhysicalAddress', 'xbox_MmPersistContiguousMemory', 'xbox_MmLockUnlockBufferPages', 'xbox_MmLockUnlockPhysicalPage'))
    adapters+='\n'.join(body(bridge,'bridge_'+name) for name in ('MmAllocateContiguousMemory', 'MmAllocateSystemMemory', 'MmAllocateContiguousMemoryEx', 'MmFreeContiguousMemory', 'MmQueryStatistics', 'MmQueryAddressProtect', 'MmGetPhysicalAddress', 'MmSetAddressProtect', 'MmClaimGpuInstanceMemory', 'MmMapIoSpace', 'MmQueryAllocationSize', 'MmUnmapIoSpace', 'MmFreeSystemMemory', 'MmCreateKernelStack', 'MmDeleteKernelStack', 'MmGlobalData', 'MmDbgAllocateMemory', 'MmDbgFreeMemory', 'MmPersistContiguousMemory', 'MmLockUnlockBufferPages', 'MmLockUnlockPhysicalPage', 'MmIsAddressValid', 'MmDbgQueryAvailablePages', 'MmDbgReleaseAddress', 'MmDbgWriteCheck'))
    if mutation:
        old,new=mutation
        assert adapters.count(old)==1
        adapters=adapters.replace(old,new)
    (tmp_path/'guest_mm_adapters.inc').write_text(adapters,encoding='utf-8')
    for name in ('guest_mm_adapters_contracts.c','guest_memory_services.c','guest_memory_services.h',
                 'memory_statistics.c','memory_statistics.h','host_memory_stats.h',
                 'guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','host_pages.h',
                 'host_virtual_memory.c','host_virtual_memory.h'):
        path=HERE/name
        if not path.exists():path=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(path.read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'guest_mm_adapters_contracts.c',extra=[
        tmp_path/'guest_memory_services.c',tmp_path/'memory_statistics.c',tmp_path/'guest_allocation.c',
        tmp_path/'host_virtual_memory.c',*flags])


def test_actual_guest_mm_adapters(tmp_path):
    exe=build(tmp_path)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'actual guest Mm bridges' in run.stdout


@pytest.mark.parametrize('mutation',[
    ('TSFP_MM_IO,STACK_ARG(1)','TSFP_MM_IO,STACK_ARG(0)'),
    ('TSFP_MM_STACK,STACK_ARG(1)','TSFP_MM_STACK,STACK_ARG(0)'),
    ('memory->physical_base=XBOX_CONTIG_BASE;','memory->physical_base=0;'),
    ('if(!status)memcpy(output,words,sizeof(words));',
     'if(!status)memcpy(output,words,sizeof(words)); else memset(output,0,sizeof(words));'),
    ('return (ULONG_PTR)address;','return (ULONG_PTR)(uint32_t)(uintptr_t)address;'),
])
def test_guest_mm_adapter_negative_controls(tmp_path,mutation):
    exe=build(tmp_path,mutation)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode!=0,'Mutated guest Mm adapter incorrectly passed'
