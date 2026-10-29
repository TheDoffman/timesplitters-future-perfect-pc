from pathlib import Path
import os,sys,subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name

@pytest.mark.parametrize('mutation',[None,
 ('guest_io_target.c','table+12+4*candidate.major','table+8+4*candidate.major'),
 ('guest_io_target.c','candidate.major>=14','candidate.major>=13'),
 ('guest_io_target.c','word(d+8)!=address','word(d+8)==0'),
 ('guest_io_request.c','if(current!=expected)return 0;','if(current!=expected && current==0)return 0;'),
 ('guest_io_request.c','current-=TSFP_IO_STACK_BYTES;','current+=TSFP_IO_STACK_BYTES;'),
])
def test_readonly_driver_target(tmp_path,mutation):
    for name in ('guest_io_target.c','guest_io_target.h','guest_io_request.c','guest_io_request.h','guest_io_stack.h','io_target_contract.c'):(tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        name,old,new=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_target_contract.c',extra=[tmp_path/'guest_io_target.c',tmp_path/'guest_io_request.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken target selection passed'
    else:assert run.returncode==0,run.stdout+run.stderr
