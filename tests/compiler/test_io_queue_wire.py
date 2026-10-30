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
 ('guest_io_queue_wire','put(p+0x10,q->current','put(p+0x14,q->current'),
 ('guest_io_queue_wire','(q->current || q->count)','(q->count!=0)'),
 ('guest_io_queue_wire','wire->address+0x2c','wire->address+0x30'),
 ('guest_io_owner','if(o->waiting_queue==queue) {o->bytes[0x48]=0;','if(o->waiting_queue==queue) {o->bytes[0x48]=1;'),
 ('guest_io_queue_wire','i?waiting(q,i-1)','i?waiting(q,i)'),
 ('guest_io_queue_wire','q->publish=changed;','q->publish=NULL;'),
 ('guest_io_owner','!o->waiting_queue &&','(!o->waiting_queue || queue!=NULL) &&'),
 ('guest_io_owner','o->clients+=references;','o->clients+=1;'),
])
def test_original_queue_guest_mirrors_and_membership(tmp_path,mutation):
    providers=('guest_io_queue_wire','guest_io_queue','guest_io_owner','guest_io_request','guest_device_body')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_queue_contract.c','io_queue_wire_contract.c']:
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        provider,old,new=mutation;p=tmp_path/(provider+'.c');s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'io_queue_wire_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken queue mirror/membership passed'
    else:assert run.returncode==0,run.stdout+run.stderr
