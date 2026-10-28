"""Native-width VM policy, separate from the guest's seven-word ABI."""
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
    for name in ('host_virtual_memory.h','host_virtual_memory.c','host_virtual_memory_contracts.c'):
        path=HERE/name
        if not path.exists(): path=ROOT/'src/runtime'/name
        text=path.read_text(encoding='utf-8')
        if mutation and name=='host_virtual_memory.c':
            old,new=mutation
            assert text.count(old)==1
            text=text.replace(old,new)
        (folder/name).write_text(text,encoding='utf-8')
    return _compile(folder,folder/'host_virtual_memory_contracts.c',
                    extra=[folder/'host_virtual_memory.c',*(['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])])


@pytest.fixture(scope='module')
def executable(tmp_path_factory):
    return build(tmp_path_factory.mktemp('host-virtual-memory'))


@pytest.mark.parametrize('mode',['allocate','free','query'])
def test_host_virtual_contracts(executable,mode):
    run=subprocess.run([str(executable),mode],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('mode,mutation',[
    ('allocate',('*base = result;','*base = (void *)(uintptr_t)(uint32_t)(uintptr_t)result;')),
    ('free',('if (type & 0x8000u) *base = NULL;','*base = NULL;')),
    ('query',('memset(scratch,0,scratch_size);','memset(scratch,0xcc,scratch_size);')),
    ('query',('if (returned) *returned=(uint32_t)copy;','if (returned) *returned=(uint32_t)scratch_size;')),
])
def test_native_vm_negative_controls(tmp_path,mode,mutation):
    exe=build(tmp_path,mutation)
    run=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=30)
    assert run.returncode!=0,'Mutated provider incorrectly passed'
