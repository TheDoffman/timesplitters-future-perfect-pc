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
    ('value.length=read_word(p+4);','value.length=read_word(p+8);'),
    ('value.buffer_word=read_word(p+8);','value.buffer_word=read_word(p+4);'),
    ('((uint64_t)read_word(p+16)<<32)','((uint64_t)read_word(p+16)<<16)'),
    ('value.device=read_word(p+20);','value.device=read_word(p+16);'),
    ('write_word(p+20,value.device);','write_word(p+16,value.device);'),
    ('if(!(value->flags&0x80))','if(value->flags&0x80)'),
    ('address+value->length>UINT64_C(0x100000000)','address+value->length>UINT64_MAX'),
    ('(!address && value->length)','(!address && !value->length)'),
    ('major==2 || major==3','major==2 || major==4'),
])
def test_original_supported_us_transfer_view(tmp_path,mutation):
    for name in ('guest_io_transfer.c','guest_io_transfer.h','io_transfer_contract.c'):
        source=HERE/name if (HERE/name).is_file() else ROOT/"src/runtime"/name
        (tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'guest_io_stack.h').write_bytes((ROOT/'src/runtime/guest_io_stack.h').read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_transfer.c';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_transfer_contract.c',extra=[tmp_path/'guest_io_transfer.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken transfer view passed'
    else:assert run.returncode==0,run.stdout+run.stderr
