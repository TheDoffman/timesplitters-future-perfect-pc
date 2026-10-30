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
@pytest.mark.parametrize('mutation',[None,
    ('put_word(p+0x50,value.thread)','put_word(p+0x4c,value.thread)'),
    ('put_word(p+0x1c,value.user_iosb)','put_word(p+0x20,value.user_iosb)'),
    ('(size_t)(stacks-1)','(size_t)stacks'),
    ('memcpy(p+offset,slot,sizeof(slot));','memcpy(p+offset,slot,sizeof(slot)-1);'),
    ('!stacks || stacks>TSFP_IO_MAX_STACKS','stacks>TSFP_IO_MAX_STACKS'),
    ('if(!tsfp_io_request_initialize(p,span,address,bytes,stacks,6))return 0;','(void)tsfp_io_request_initialize(p,span,address,bytes,stacks,6);'),
])
def test_original_transfer_packet_construction(tmp_path,mutation):
    providers=('guest_io_build','guest_io_transfer','guest_io_request')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_build_contract.c']:
        candidate=HERE/name;source=candidate if candidate.is_file() else ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_build.c';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_build_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken packet construction passed'
    else:assert run.returncode==0,run.stdout+run.stderr
