from pathlib import Path
import os
import subprocess
import sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/support'))
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def fixture_path(name):
    path=HERE/name
    if path.exists(): return path
    group='native' if name in ('shared_host.c','guest_map_host.c') else 'runtime'
    return ROOT/'src'/group/name
from test_virtual_memory_adapters import body

def build(tmp_path,mutation=None):
    source=HERE/'integration/src/platform/win32_compat.c'
    if not source.exists(): source=ROOT/'external/ts3FPrecomp/src/platform/win32_compat.c'
    text=source.read_text(encoding='utf-8')
    block=text[text.index('/* Project arena forwarding.'):text.index('/* End project arena forwarding. */')]
    block+='\n'+text[text.index('/* Project MMIO forwarding.'):text.index('/* End project MMIO forwarding. */')]
    block+='\n'.join(body(text,name) for name in ('VirtualAlloc','VirtualFree','MapViewOfFileEx','UnmapViewOfFile'))
    if mutation:
        old,new=mutation
        assert block.count(old)==1
        block=block.replace(old,new)
    (tmp_path/'guest_arena_adapters.inc').write_text(block,encoding='utf-8')
    names=('shared_views','owned_arena','mmio_views','guest_arena_service')
    for name in [f'{n}.{ext}' for n in names for ext in ('c','h')]+['guest_arena_adapters_contracts.c']:
        (tmp_path/name).write_bytes(fixture_path(name).read_bytes())
    extra=[tmp_path/f'{n}.c' for n in names]
    extra+=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_arena_adapters_contracts.c',extra=extra)
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=40)

def test_actual_guest_arena_adapters(tmp_path):
    p=build(tmp_path)
    assert p.returncode==0,p.stdout+p.stderr
    assert 'share ownership and retain failed cleanup' in p.stdout

@pytest.mark.parametrize('mutation',[
    ('return tsfp_guest_arena_unmap((void *)address);','(void)address;return TRUE;'),
    ('if (arena_contains(baseAddr, 0)) return arena_unmap(baseAddr);',
     'if (arena_contains(baseAddr, 0)) { (void)view_take(baseAddr); return arena_unmap(baseAddr); }'),
    ('(prot&PROT_EXEC)?TSFP_ARENA_EXECUTE:0','(prot&PROT_EXEC)?0:0'),
    ('return tsfp_guest_mmio_alias(address,size);','(void)size;return tsfp_guest_mmio_alias(address,1);'),
    ('return tsfp_guest_arena_reserve(size);','return tsfp_guest_arena_reserve((uint32_t)size);'),
])
def test_guest_arena_adapter_negative_controls(tmp_path,mutation):
    p=build(tmp_path,mutation)
    assert p.returncode!=0,'mutated runtime arena adapter passed'
