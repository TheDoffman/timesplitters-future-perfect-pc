"""Selected pool imports against real guest ownership, not a host-heap mock."""
from pathlib import Path
import os,sys,subprocess,re
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile

@pytest.mark.parametrize('mutation',[None,
 ('bytes,16','bytes,4'),
 ('bytes,16','(uint16_t)bytes,16'),
 ('bridge_pool_allocate(bytes);','bridge_pool_allocate(bytes?tag:bytes);'),
 ('tsfp_allocation_remaining(memory.allocations,address)','tsfp_allocation_remaining(memory.allocations,address+1)'),
 ('tsfp_heap_release(memory.allocations,address)','tsfp_virtual_release(memory.allocations,address)'),
 ('address);g_eax=0;','address);g_eax=address;'),
])
def test_original_pool_guest_ownership_and_abi(tmp_path,mutation):
    for name in ('guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','guest_kernel_pool.inc','kernel_pool_contract.c'):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        p=tmp_path/'guest_kernel_pool.inc';s=p.read_text(encoding='utf-8');a,b=mutation
        assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'kernel_pool_contract.c',extra=[tmp_path/'guest_allocation.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken pool import passed'
    else:assert run.returncode==0,run.stdout+run.stderr

def test_retired_native_pool_has_no_production_caller():
    runtime=ROOT/'external/ts3FPrecomp'
    cmake=(runtime/'src/kernel/CMakeLists.txt').read_text(encoding='utf-8')
    sources=re.search(r'set\(KERNEL_SOURCES(.*?)\)',cmake,re.S).group(1).split()
    assert 'kernel_pool.c' not in sources and 'kernel_thunks.c' not in sources
    bridge=(runtime/'src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_pool.inc"')==1
    for ordinal,bytes_,name in ((14,4,'ExAllocatePool'),(15,8,'ExAllocatePoolWithTag'),(17,4,'ExFreePool'),(23,4,'ExQueryPoolBlockSize')):
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+'+str(bytes_)+r';\s*/\*\s*'+name+r'\b',bridge)
    for name in ('ExAllocatePool','ExAllocatePoolWithTag','ExFreePool','ExQueryPoolBlockSize'):
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert 'xbox_'+name not in (runtime/'src/kernel/kernel.h').read_text(encoding='utf-8')
    retired=re.compile(r'\bxbox_Ex(?:AllocatePool(?:WithTag)?|FreePool|QueryPoolBlockSize)\b')
    for root in (runtime/'src',runtime/'titles/futureperfect/src',ROOT/'src'):
        for p in root.rglob('*'):
            if p.suffix not in ('.c','.h','.inc') or p.name in ('kernel_pool.c','kernel_thunks.c'):continue
            assert not retired.search(p.read_text(encoding='utf-8',errors='replace')),p
