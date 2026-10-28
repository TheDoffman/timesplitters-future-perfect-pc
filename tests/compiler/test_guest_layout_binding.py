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

def build(tmp_path,mode,mutation=None):
    source=HERE/'integration/src/kernel/xbox_memory_layout.c'
    if not source.exists(): source=ROOT/'external/ts3FPrecomp/src/kernel/xbox_memory_layout.c'
    text=source.read_text(encoding='utf-8')
    block=text[text.index('/* Project host-layout binding.'):text.index('/* End project host-layout binding. */')]
    block+='\n'+text[text.index('/* Project map shutdown binding:'):text.index('/* End project map shutdown binding. */')]
    block+='\n'+body(text,'xbox_TiledApertureSize')
    if mutation:
        old,new=mutation;assert block.count(old)==1;block=block.replace(old,new)
    (tmp_path/'guest_layout_binding.inc').write_text(block,encoding='utf-8')
    names=('shared_views','owned_arena','mmio_views','guest_arena_service','guest_map_layout')
    for name in [f'{n}.{ext}' for n in names for ext in ('c','h')]+['shared_host.c','guest_map_host.c','guest_layout_binding_contracts.c']:
        (tmp_path/name).write_bytes(fixture_path(name).read_bytes())
    extra=[tmp_path/f'{n}.c' for n in names]+[tmp_path/'shared_host.c',tmp_path/'guest_map_host.c']
    extra+=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_layout_binding_contracts.c',extra=extra)
    return subprocess.run([str(exe),str(mode)],capture_output=True,text=True,timeout=40)

@pytest.mark.parametrize('mode',[0,1,2])
def test_actual_layout_binding(tmp_path,mode):
    p=build(tmp_path,mode);assert p.returncode==0,p.stdout+p.stderr
    assert 'bounded worker wait and published map retention' in p.stdout

@pytest.mark.parametrize('mode,mutation',[
    (0,('g_kernel_memory=g_host_layout.kernel;','g_kernel_memory=g_host_layout.physical;')),
    (0,('same=*physical==0xa5c30f17u;','same=*physical!=0xa5c30f17u;')),
    (1,('WaitForSingleObject(g_nv2a_ack_thread,1000)!=WAIT_OBJECT_0',
        'WaitForSingleObject(g_nv2a_ack_thread,1000)==WAIT_OBJECT_0')),
    (1,('if(result==2) {','if(result==3) {')),
    (2,('if((DWORD)(GetTickCount()-started)>=1000)return 0;',
        'if((DWORD)(GetTickCount()-started)>=1)return 0;')),
])
def test_layout_binding_negative_controls(tmp_path,mode,mutation):
    p=build(tmp_path,mode,mutation);assert p.returncode!=0,'mutated layout binding passed'
