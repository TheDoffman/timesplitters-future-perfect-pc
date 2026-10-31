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
 ('!r->closed || r->references || r->queue.current || r->queue.count','!r->closed || r->references || r->queue.count'),
 ('!r->closed || r->references || r->queue.current || r->queue.count','!r->closed || r->queue.current || r->queue.count'),
 ('r->generation==t->generation &&','r->generation!=0 &&'),
 ('r->closed=1;r->queue.closed=1;','r->closed=1;r->queue.closed=0;'),
 ('r->state=TSFP_IO_QUEUE_RETIRING;leave(m);','r->state=TSFP_IO_QUEUE_RETIRING;'),
 ('if(!prepared || m->closed || r->closed)','if(!prepared)'),
 ('else r->state=TSFP_IO_QUEUE_QUARANTINED;','else r->state=TSFP_IO_QUEUE_FREE;'),
 ('m->records[i].closed=1;m->records[i].queue.closed=1;','m->records[i].closed=1;'),
])
def test_original_persistent_device_queue_lifetime(tmp_path,mutation):
    providers=('guest_io_scheduler','guest_io_queue_wire','guest_io_startio','guest_io_queue','guest_io_owner','guest_io_request','guest_io_scope','guest_io_devices','guest_callback_call','guest_objects','guest_object_names','guest_device_factory','guest_device_body','guest_device_callback','guest_object_scope','guest_allocation','guest_state','guest_nonlocal_jump')
    headers=['guest_io_call.h','guest_io_target.h','guest_io_stack.h','guest_object_services.h','recomp_types.h','guest_lifetime.h']+[f'{p}.h' for p in providers if p not in ('guest_state','guest_nonlocal_jump')]
    for name in [f'{p}.c' for p in providers]+headers+['guest_device_contract.c','io_scheduler_contract.c']:
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    # Injectable lease-release contract failure; the actual object implementation
    # still executes for every ordinary release and fixture recovery.
    p=tmp_path/'guest_objects.c';s=p.read_text(encoding='utf-8');old='int tsfp_guest_objects_release('
    assert s.count(old)==1;p.write_bytes(s.replace(old,'int tsfp_guest_objects_release_actual(').encode())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_scheduler.c';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'io_scheduler_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken persistent queue lifetime passed'
    else:assert run.returncode==0,run.stdout+run.stderr
