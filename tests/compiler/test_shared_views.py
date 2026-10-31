from pathlib import Path
import os
import subprocess
import sys
import pytest

HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'tools/project_paths.py').exists())
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
sys.path.insert(0, str(ROOT / 'tests/support'))
sys.path.insert(0, str(ROOT / 'tests/compiler'))
from test_translation_product import _compile

def fixture_path(name):
    path=HERE/name
    if path.exists(): return path
    group='native' if name in ('shared_host.c','guest_map_host.c') else 'runtime'
    return ROOT/'src'/group/name

def run_contract(tmp_path, kind, mutation=None):
    source = {'posix':'shared_posix_model.c','arena':'owned_arena_contracts.c','mmio':'mmio_views_contracts.c'}.get(kind,f'shared_{kind}_contracts.c')
    for name in ('shared_views.h','shared_views.c','shared_host.c','owned_arena.c','owned_arena.h','mmio_views.c','mmio_views.h',
                 'guest_arena_service.c','guest_arena_service.h','guest_map_layout.c','guest_map_layout.h','guest_map_host.c',source):
        (tmp_path / name).write_bytes(fixture_path(name).read_bytes())
    if mutation:
        name, old, new = mutation
        path = tmp_path / name
        text = path.read_text(encoding='utf-8')
        assert text.count(old) == 1
        path.write_text(text.replace(old,new), encoding='utf-8')
    extra = [tmp_path/'shared_views.c',tmp_path/'owned_arena.c']
    if kind in ('host','mmio'): extra.append(tmp_path/'shared_host.c')
    if kind == 'mmio': extra.append(tmp_path/'mmio_views.c')
    if kind == 'host':
        extra += [tmp_path/name for name in ('mmio_views.c','guest_arena_service.c','guest_map_layout.c','guest_map_host.c')]
    if kind == 'posix':
        for name, text in {
            'unistd.h': '#define _SC_PAGESIZE 30\n',
            'sys/types.h': '',
            'sys/mman.h': '#define PROT_NONE 0\n#define PROT_READ 1\n#define PROT_WRITE 2\n#define PROT_EXEC 4\n#define MAP_SHARED 1\n#define MAP_PRIVATE 2\n#define MAP_FIXED 16\n#define MAP_ANON 4096\n#define MAP_FAILED ((void *)-1)\n',
        }.items():
            path=tmp_path/name
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text(text,encoding='utf-8')
        extra += [f'/I{tmp_path}'] if os.name=='nt' else [f'-I{tmp_path}']
    extra += ['/W4'] if os.name == 'nt' else ['-Wall','-Wextra','-pthread']
    exe = _compile(tmp_path,tmp_path/source,extra=extra)
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=45)

@pytest.mark.parametrize('kind',['views','host','posix','arena','mmio'])
def test_shared_backing_contracts(tmp_path,kind):
    p = run_contract(tmp_path,kind)
    assert p.returncode == 0, p.stdout+p.stderr
    assert 'PASS:' in p.stdout

@pytest.mark.parametrize('mutation',[
    ('shared_views.c','if(!s->backend.unmap(s->backend.context,v->base,v->bytes))return 0;',
     '(void)s->backend.unmap(s->backend.context,v->base,v->bytes);'),
    ('shared_views.c','if(hint && base!=hint) {','if(hint && base==hint) {'),
    ('shared_views.c','bytes>s->bytes-offset','bytes>s->bytes'),
    ('shared_views.c','if(result && s->backing) {','if(s->backing) {'),
    ('shared_views.c','if(v->base==base && v->published)', 'if(v->base==base)'),
])
def test_shared_owner_negative_controls(tmp_path,mutation):
    p = run_contract(tmp_path,'views',mutation)
    assert p.returncode != 0, 'mutated owner passed'

@pytest.mark.parametrize('mutation',[
    ('shared_host.c','(DWORD)(at>>32),(DWORD)at,bytes,hint','(DWORD)(at>>32),(DWORD)(at & 0),bytes,hint'),
    ('shared_host.c','FILE_MAP_READ:FILE_MAP_WRITE','FILE_MAP_READ:FILE_MAP_COPY'),
])
@pytest.mark.skipif(os.name != 'nt',reason='real Windows host negative controls')
def test_shared_windows_negative_controls(tmp_path,mutation):
    p = run_contract(tmp_path,'host',mutation)
    assert p.returncode != 0, 'mutated Windows shared mapper passed'

@pytest.mark.parametrize('mutation',[
    ('shared_host.c','PROT_WRITE:0),MAP_SHARED','PROT_WRITE:0),MAP_PRIVATE'),
    ('shared_host.c','PROT_WRITE:0),MAP_SHARED','PROT_WRITE:0),MAP_SHARED|MAP_FIXED'),
    ('shared_host.c','if(ftruncate(fileno(stream),length)) { fclose(stream);return 0; }',
     'if(ftruncate(fileno(stream),length)) { return 0; }'),
    ('shared_host.c','fileno((FILE *)token),at);','fileno((FILE *)token),(int32_t)at);'),
])
def test_shared_posix_negative_controls(tmp_path,mutation):
    p = run_contract(tmp_path,'posix',mutation)
    assert p.returncode != 0, 'mutated POSIX shared mapper passed'

@pytest.mark.parametrize('mutation',[
    ('owned_arena.c','bytes>s->bytes-(a-b)','bytes>s->bytes'),
    ('owned_arena.c','if(a<(uintptr_t)v->base+v->bytes && (uintptr_t)v->base<a+bytes)goto done;',
     'if(a==(uintptr_t)v->base)goto done;'),
    ('owned_arena.c','else if(result<0)s->poisoned=1;','else if(result<0)s->poisoned=0;'),
    ('owned_arena.c','if(s->views[i].state==PUBLISHED)goto done;','if(s->views[i].state==ABANDONED)goto done;'),
    ('owned_arena.c','!s->active || s->poisoned || !b','!s->active || !b'),
])
def test_owned_arena_negative_controls(tmp_path,mutation):
    p=run_contract(tmp_path,'arena',mutation)
    assert p.returncode != 0, 'mutated arena owner passed'

@pytest.mark.parametrize('mutation',[
    ('mmio_views.c','bytes<=v->bytes-(a-b)','bytes<=v->bytes'),
    ('mmio_views.c','v->visible && a>=b','a>=b'),
    ('mmio_views.c','s->views[i].visible=0','s->views[i].visible=1'),
    ('mmio_views.c','slot->bytes=bytes;','slot->bytes=rounded;'),
    ('mmio_views.c','!s || !s->active || !bytes','!s || !bytes'),
])
def test_mmio_negative_controls(tmp_path,mutation):
    p=run_contract(tmp_path,'mmio',mutation)
    assert p.returncode != 0, 'mutated MMIO owner passed'
