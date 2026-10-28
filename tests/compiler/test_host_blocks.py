"""Original native allocation ownership; controlled host allocation failures."""
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


def build(folder,mutation=None):
    for name in ('host_blocks.c','host_blocks.h','host_blocks_contracts.c'):
        path=HERE/name
        if not path.exists(): path=ROOT/'src/runtime'/name
        source=path.read_text(encoding='utf-8')
        if mutation and name=='host_blocks.c':
            old,new=mutation
            assert source.count(old)==1
            source=source.replace(old,new)
        (folder/name).write_text(source,encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    return _compile(folder,folder/'host_blocks_contracts.c',extra=[folder/'host_blocks.c',*flags])


@pytest.fixture(scope='module')
def executable(tmp_path_factory):
    return build(tmp_path_factory.mktemp('host-blocks'))


@pytest.mark.parametrize('mode',['allocate','failure','lifecycle','concurrent'])
def test_host_block_contracts(executable,mode):
    run=subprocess.run([str(executable),mode],capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('mode,mutation',[
    ('allocate',('result=(void *)aligned;','result=(void *)(uintptr_t)(uint32_t)aligned;')),
    ('allocate',('(kind && r->kind!=kind)','(kind && r->kind==UINT32_MAX)')),
    ('failure',('if (s->backend.unmap(s->backend.context,r->mapping,r->mapping_size)) {',
                'if ((s->backend.unmap(s->backend.context,r->mapping,r->mapping_size), 1)) {')),
    ('lifecycle',('        s->active || s->live) return 0;','        s->live) return 0;')),
])
def test_compiled_host_block_negative_controls(tmp_path,mode,mutation):
    exe=build(tmp_path,mutation)
    run=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=60)
    assert run.returncode!=0,'Mutated ownership engine incorrectly passed'
