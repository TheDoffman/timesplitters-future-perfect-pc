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

def run(tmp_path,mutation=None):
    for name in ('guest_io_stack.c','guest_io_stack.h','io_stack_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_stack.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_stack_contract.c',extra=[tmp_path/'guest_io_stack.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)

@pytest.mark.parametrize('mutation',[None,
    ('if(!p || !out || bytes<TSFP_IO_STACK_BYTES)return 0;','if(!p || !out || bytes<20)return 0;'),
    ('((uint64_t)get(p+16)<<32)','((uint64_t)get(p+16)<<16)'),
    ('put(p+20,value.device)','put(p+16,value.device)'),
])
def test_explicit_guest_stack_codec(tmp_path,mutation):
    result=run(tmp_path,mutation)
    if mutation:assert result.returncode!=0,'Broken stack codec passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout
