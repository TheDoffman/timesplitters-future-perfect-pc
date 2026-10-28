"""Run proposed native Mm bodies with real OS memory and concurrent first use."""
from pathlib import Path
import os
import subprocess
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
from test_virtual_memory_adapters import body


def test_native_mm_adapters(tmp_path):
    path=HERE/'integration/src/kernel/kernel_memory.c'
    if not path.exists():path=ROOT/'external/ts3FPrecomp/src/kernel/kernel_memory.c'
    source=path.read_text(encoding='utf-8')
    first=source.index('/* Project native Mm ownership.')
    end=source.index('/* End project native Mm ownership. */',first)
    adapters='#include "native_vm_host.h"\n'+source[first:end]
    names=('xbox_MmAllocateContiguousMemory', 'xbox_MmAllocateContiguousMemoryEx', 'xbox_MmFreeContiguousMemory', 'xbox_MmAllocateSystemMemory', 'xbox_MmFreeSystemMemory', 'xbox_MmMapIoSpace', 'xbox_MmUnmapIoSpace', 'xbox_MmQueryAddressProtect', 'xbox_MmSetAddressProtect', 'xbox_MmQueryAllocationSize', 'xbox_MmClaimGpuInstanceMemory', 'xbox_MmCreateKernelStack', 'xbox_MmDeleteKernelStack')
    adapters+='\n'.join(body(source,name) for name in names)
    (tmp_path/'native_mm_adapters.inc').write_text(adapters,encoding='utf-8')
    for name in ('native_mm_contracts.c','host_blocks.c','host_blocks.h','host_pages.c','host_pages.h',
                 'native_vm_host.c','native_vm_host.h','native_reservations.c','native_reservations.h'):
        path=HERE/name
        if not path.exists():
            path=ROOT/'src'/('native' if name in ('host_pages.c','native_vm_host.c') else 'runtime')/name
        (tmp_path/name).write_bytes(path.read_bytes())
    for name in ('host_virtual_memory.c','host_virtual_memory.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'native_mm_contracts.c',extra=[
        tmp_path/'host_blocks.c',tmp_path/'host_pages.c',tmp_path/'host_virtual_memory.c',
        tmp_path/'native_vm_host.c',tmp_path/'native_reservations.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout
