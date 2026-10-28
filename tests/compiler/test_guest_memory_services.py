"""Original Mm policy on synthetic guest storage and controlled host access."""
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


def build(tmp_path,mutation=None):
    for name in ('guest_memory_services.c','guest_memory_services.h','memory_statistics.c',
                 'memory_statistics.h','guest_memory_services_contracts.c','guest_virtual_memory.h',
                 'guest_allocation.c','guest_allocation.h'):
        path=HERE/name
        if not path.exists():path=ROOT/'src/runtime'/name
        source=path.read_text(encoding='utf-8')
        if name=='guest_memory_services.c' and mutation:
            old,new=mutation
            assert source.count(old)==1
            source=source.replace(old,new)
        (tmp_path/name).write_text(source,encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'guest_memory_services_contracts.c',extra=[
        tmp_path/'guest_memory_services.c',tmp_path/'memory_statistics.c',tmp_path/'guest_allocation.c',*flags])


@pytest.fixture(scope='module')
def executable(tmp_path_factory):
    return build(tmp_path_factory.mktemp('guest-memory-services'))


@pytest.mark.parametrize('mode',['allocation','statistics','gpu','access'])
def test_guest_memory_services(executable,mode):
    run=subprocess.run([str(executable),mode],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('mode,mutation',[
    ('allocation',('base && kind==TSFP_MM_STACK?base+bytes:base','base && kind==TSFP_MM_STACK?base:base')),
    ('statistics',('output,TSFP_MEMORY_STATISTICS_BYTES,1','output,4,1')),
    ('gpu',('if(bytes>m->physical_size)return 0;','if(bytes>m->physical_size && !bytes)return 0;')),
    ('gpu',('if(!padding)return 0;','if(!padding)padding=NULL;')),
    ('access',('address,bytes,-1','address,bytes,0')),
])
def test_guest_memory_negative_controls(tmp_path,mode,mutation):
    exe=build(tmp_path,mutation)
    run=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=30)
    assert run.returncode!=0,'Mutated guest Mm provider incorrectly passed'
