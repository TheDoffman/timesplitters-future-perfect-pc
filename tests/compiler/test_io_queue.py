from pathlib import Path
import os,sys,subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile
@pytest.mark.parametrize('mutation',[None,
    ('q->current==owner','0'),
    ('q->entries[index_at(q,i)]==owner','0'),
    ('q->current!=expected','q->current!=expected && !expected'),
    ('q->closed || q->count==q->capacity','q->count==q->capacity'),
    ('offset-remaining','0'),
    ('if(!tsfp_io_owner_retain(next))goto done;','if(!next)goto done;'),
    ('if(previous)(void)tsfp_io_owner_release(previous);','if(previous)(void)tsfp_io_owner_phase(previous);'),
])
def test_original_owned_device_fifo(tmp_path,mutation):
    providers=('guest_io_queue','guest_io_owner','guest_io_request')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_queue_contract.c']:
        path=HERE/name
        if not path.is_file():path=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(path.read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_queue.c';s=p.read_text(encoding='utf-8')
        assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'io_queue_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken queue passed'
    else:assert run.returncode==0,run.stdout+run.stderr
