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
    ('o->internal+=2;', 'o->internal+=1;'),
    ('o->phase=TSFP_IO_COMPLETING;', 'o->phase=TSFP_IO_PENDING;'),
    ('if(!o->clients && !o->internal &&', 'if(!o->clients &&'),
    ('put(o->bytes+0x14,info);', 'put(o->bytes+0x14,info+1);'),
])
def test_request_lifetime_and_completion(tmp_path,mutation):
    for name in ('guest_io_owner.c','guest_io_owner.h','guest_io_request.c',
                 'guest_io_request.h','guest_io_stack.h','io_owner_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_owner.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_owner_contract.c',extra=[
        tmp_path/'guest_io_owner.c',tmp_path/'guest_io_request.c',*flags])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=45)
    if mutation:assert result.returncode!=0,'Broken request lifetime passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout
