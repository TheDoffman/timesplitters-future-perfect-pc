from pathlib import Path
import os
import sys
import subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name

@pytest.mark.parametrize('mutation',[None,
    ('p->closed || p->live>=p->registry.count','p->closed'),
    ('tsfp_io_owner_release(&n->owner);return address;', '(void)n;return address;'),
    ('(*link)->cleanup!=0','(*link)->cleanup==1'),
    ('(void)cleanup(n);return 0;', 'drop_metadata(n);return 0;'),
])
def test_real_guest_request_allocations(tmp_path,mutation):
    providers=('guest_io_pool','guest_io_registry','guest_io_owner','guest_io_request')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_pool_contract.c']:
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_allocation.c','guest_allocation.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_pool.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_pool_contract.c',extra=[
        *[tmp_path/f'{p}.c' for p in providers],tmp_path/'guest_allocation.c',*flags])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert result.returncode!=0,'Broken guest request allocation passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout
