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
 ('guest_io_owner.c','++o->internal;ticket->owner=o;ticket->kind=4;','o->internal+=2;ticket->owner=o;ticket->kind=4;'),
 ('guest_io_owner.c','        --o->internal;\n    } else {','        o->phase=TSFP_IO_READY;--o->internal;\n    } else {'),
 ('guest_io_owner.c','forward && o->phase==TSFP_IO_PENDING','forward && o->phase==TSFP_IO_READY'),
 ('guest_io_scope.c','kind==5','kind==6'),
 ('guest_io_call.c','args[0]=body;args[1]=owner->address;','put(owner->bytes+(target.stack-owner->address)+20,body);args[0]=body;args[1]=owner->address;'),
 ('guest_io_call.c','args[0]=body;args[1]=owner->address;','put(owner->bytes+(target.stack-owner->address)+4,body);args[0]=body;args[1]=owner->address;'),
])
def test_layered_driver_calls_and_async_forwarding(tmp_path,mutation):
    providers=('guest_io_call','guest_io_owner','guest_io_request','guest_io_scope','guest_io_devices','guest_io_target','guest_callback_call','guest_objects','guest_object_names','guest_device_factory','guest_device_body','guest_device_callback','guest_object_scope','guest_allocation','guest_state','guest_nonlocal_jump')
    headers=['guest_io_stack.h','guest_object_services.h','recomp_types.h','guest_lifetime.h']+[f'{base}.h' for base in providers if base not in ('guest_state','guest_nonlocal_jump')]
    for name in [f'{base}.c' for base in providers]+headers:
        candidate=HERE/name;source=candidate if candidate.is_file() else ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'io_forward_contract.c').write_bytes((HERE/'io_forward_contract.c').read_bytes())
    (tmp_path/'guest_device_contract.c').write_bytes((ROOT/'tests/compiler/guest_device_contract.c').read_bytes())
    if mutation:
        name,old,new=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_forward_contract.c',extra=[*[tmp_path/f'{base}.c' for base in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken layered dispatch passed'
    else:assert run.returncode==0,run.stdout+run.stderr
