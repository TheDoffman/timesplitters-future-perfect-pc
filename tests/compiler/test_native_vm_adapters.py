from pathlib import Path
import os,subprocess,sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
from test_virtual_memory_adapters import body

def build(tmp_path,host,mutation=None):
    path=HERE/'integration/src/kernel/kernel_memory.c'
    if not path.exists():path=ROOT/'external/ts3FPrecomp/src/kernel/kernel_memory.c'
    text=path.read_text(encoding='utf-8')
    adapters=text[text.index('/* Project native VM boundary:'):text.index('/* End project native VM boundary. */')]
    adapters+='\n'.join(body(text,'xbox_'+name) for name in
        ('NtAllocateVirtualMemory','NtFreeVirtualMemory','NtQueryVirtualMemory'))
    if mutation:
        old,new=mutation;assert adapters.count(old)==1;adapters=adapters.replace(old,new)
    (tmp_path/'native_vm_adapters.inc').write_text(adapters,encoding='utf-8')
    for name in ('native_vm_adapters_contracts.c','native_vm_host.h','native_reservations.h',
                 'host_pages.h','host_virtual_memory.h','host_virtual_memory.c'):
        p=HERE/name
        if not p.exists():p=ROOT/'src/runtime'/name
        text=p.read_text(encoding='utf-8')
        if name=='native_vm_adapters_contracts.c':
            text='#undef _WIN32\n'+('#define _WIN32 1\n' if host=='windows' else '')+text
        (tmp_path/name).write_text(text,encoding='utf-8')
    return _compile(tmp_path,tmp_path/'native_vm_adapters_contracts.c',extra=[
        tmp_path/'host_virtual_memory.c',*(['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])])

@pytest.mark.parametrize('host',['windows','posix'])
def test_native_nt_adapters(tmp_path,host):
    exe=build(tmp_path,host)
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert p.returncode==0,p.stdout+p.stderr
    assert 'preserve widths, records and failure outputs' in p.stdout

@pytest.mark.parametrize('mutation',[
    ('tsfp_native_vm_allocate(hint,bytes,type,protection)','tsfp_native_vm_allocate(hint,(uint32_t)bytes,type,protection)'),
    ('native.AllocationBase=(PVOID)region.allocation_base;','native.AllocationBase=(PVOID)(uintptr_t)(uint32_t)region.allocation_base;'),
    ('native.PartitionId=region.partition_id;','native.PartitionId=0;'),
    ('memcpy(record,&native,sizeof(native));return sizeof(native);','memcpy(record,&native,sizeof(native));return sizeof(native)-1;'),
])
def test_native_nt_adapter_negative_controls(tmp_path,mutation):
    exe=build(tmp_path,'windows',mutation)
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert p.returncode!=0,'mutated native Nt adapter passed'
