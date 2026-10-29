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
    ('if(current!=expected)return 0;','if(current==UINT32_MAX && expected==0)return 0;'),
    ('current-=TSFP_IO_STACK_BYTES','current+=TSFP_IO_STACK_BYTES'),
    ('memset(p,0,bytes);','memset(p,0,bytes-1);'),
    ('end<=UINT32_MAX','end<=UINT64_MAX'),
])
def test_original_request_byte_state(tmp_path,mutation):
    for name in ('guest_io_request.c','guest_io_request.h','guest_io_stack.h','io_request_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_request.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_request_contract.c',extra=[tmp_path/'guest_io_request.c',*flags])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert result.returncode!=0,'Broken request state passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout
