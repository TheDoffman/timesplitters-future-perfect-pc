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

def build(tmp_path,mutation=None):
    for name in ('shared_views.c','shared_views.h','owned_arena.h','guest_map_layout.c','guest_map_layout.h','guest_map_layout_contracts.c'):
        (tmp_path/name).write_bytes(fixture_path(name).read_bytes())
    if mutation:
        path=tmp_path/'guest_map_layout.c';text=path.read_text(encoding='utf-8');old,new=mutation
        assert text.count(old)==1
        path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_map_layout_contracts.c',extra=[tmp_path/'guest_map_layout.c',tmp_path/'shared_views.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=40)

def test_guest_layout_contracts(tmp_path):
    p=build(tmp_path);assert p.returncode==0,p.stdout+p.stderr
    assert 'borrowed kernel, rollback and published lifetime' in p.stdout

@pytest.mark.parametrize('mutation',[
    ('if(s->published)return 2;','if(!s->started)return 2;'),
    ('if(ok)memset(s,0,sizeof(*s));','memset(s,0,sizeof(*s));'),
    ('s->kernel=s->physical?address(s,0x80010000,4096):private_at(s,0x80010000,4096,TSFP_MAP_KERNEL);',
     's->kernel=private_at(s,0x80010000,4096,TSFP_MAP_KERNEL);'),
    ('if(at)s->tiled=tsfp_shared_views_map(&s->physical_store,at,0,s->tiled_bytes,TSFP_VIEW_WRITE);',
     'if(at)s->tiled=tsfp_shared_views_map(&s->ram_store,at,0,s->tiled_bytes,TSFP_VIEW_WRITE);'),
    ('if(!s->tiled)s->tiled_bytes=0;','if(!s->tiled)s->tiled_bytes=cfg.ram_bytes;'),
    ('if(!h.healthy(h.context))goto fail;','if(h.healthy(h.context)<0)goto fail;'),
])
def test_guest_layout_negative_controls(tmp_path,mutation):
    p=build(tmp_path,mutation);assert p.returncode!=0,'mutated guest layout passed'
