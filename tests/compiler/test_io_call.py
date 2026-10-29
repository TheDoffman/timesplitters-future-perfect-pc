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
 ('g->owner=NULL;return owner?tsfp_io_owner_release(owner):1;','g->owner=NULL;(void)owner;return 1;'),
 ('if(!ok)frame.scope.abandoned=1;','if(!ok)frame.scope.abandoned=0;'),
 ('p[0x19]=(unsigned char)location;put(p+0x5c,current);','(void)location;(void)current;'),
 ('if(result==TSFP_IO_CALL_RETURNED)*status=value;','if(result==TSFP_IO_CALL_RETURNED)*status=value+1;'),
])
def test_original_initial_driver_call(tmp_path,mutation):
    providers=('guest_io_call','guest_io_owner','guest_io_request','guest_io_scope','guest_io_devices','guest_io_target','guest_callback_call','guest_objects','guest_object_names','guest_device_factory','guest_device_body','guest_device_callback','guest_object_scope','guest_allocation','guest_state','guest_nonlocal_jump')
    headers=['guest_io_stack.h','guest_object_services.h','recomp_types.h','guest_lifetime.h']+[f'{base}.h' for base in providers if base not in ('guest_state','guest_nonlocal_jump')]
    for name in [f'{base}.c' for base in providers]+headers:
        candidate=HERE/name;source=candidate if candidate.is_file() else ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'io_call_contract.c').write_bytes((HERE/'io_call_contract.c').read_bytes())
    (tmp_path/'guest_device_contract.c').write_bytes((ROOT/'tests/compiler/guest_device_contract.c').read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_call.c';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_call_contract.c',extra=[*[tmp_path/f'{base}.c' for base in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken initial driver call passed'
    else:assert run.returncode==0,run.stdout+run.stderr


@pytest.mark.skipif(os.name!='nt',reason='Actual Windows metadata unmapping; portable call contracts separate')
def test_call_disposes_real_windows_metadata(tmp_path):
    providers=('guest_io_call','guest_io_owner','guest_io_request','guest_io_scope','guest_io_devices','guest_io_target','guest_io_pool','guest_io_registry','guest_callback_call','guest_objects','guest_object_names','guest_device_factory','guest_device_body','guest_device_callback','guest_object_scope','guest_allocation','guest_state','guest_nonlocal_jump')
    headers=['guest_io_stack.h','guest_object_services.h','recomp_types.h','guest_lifetime.h']+[f'{base}.h' for base in providers if base not in ('guest_state','guest_nonlocal_jump')]
    for name in [f'{base}.c' for base in providers]+headers:
        candidate=HERE/name;source=candidate if candidate.is_file() else ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'io_call_windows.c').write_bytes((HERE/'io_call_windows.c').read_bytes())
    (tmp_path/'guest_device_contract.c').write_bytes((ROOT/'tests/compiler/guest_device_contract.c').read_bytes())
    exe=_compile(tmp_path,tmp_path/'io_call_windows.c',extra=[*[tmp_path/f'{base}.c' for base in providers],'/W4'])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
