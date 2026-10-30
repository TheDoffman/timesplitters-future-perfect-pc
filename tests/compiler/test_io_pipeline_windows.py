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
@pytest.mark.skipif(os.name!='nt',reason='Real Windows event and metadata unmapping; portable providers tested separately')
def test_construct_dispatch_complete_with_real_windows_event(tmp_path):
    providers=('guest_io_build','guest_io_transfer','guest_io_complete','guest_io_call','guest_io_owner','guest_io_request','guest_io_scope','guest_io_devices','guest_io_target','guest_io_pool','guest_io_registry','guest_callback_call','guest_objects','guest_object_names','guest_device_factory','guest_device_body','guest_device_callback','guest_object_scope','guest_allocation','guest_state','guest_nonlocal_jump')
    headers=['guest_io_stack.h','guest_object_services.h','recomp_types.h','guest_lifetime.h']+[f'{p}.h' for p in providers if p not in ('guest_state','guest_nonlocal_jump')]
    for name in [*[f'{p}.c' for p in providers],*headers]:
        choices=[HERE/name,ROOT/'src/runtime'/name]
        source=next(p for p in choices if p.is_file());(tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'guest_device_contract.c').write_bytes((ROOT/'tests/compiler/guest_device_contract.c').read_bytes())
    (tmp_path/'io_pipeline_windows.c').write_bytes((HERE/'io_pipeline_windows.c').read_bytes())
    exe=_compile(tmp_path,tmp_path/'io_pipeline_windows.c',extra=[*[tmp_path/f'{p}.c' for p in providers],'/W4'])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
