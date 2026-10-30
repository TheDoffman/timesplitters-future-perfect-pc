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
 ('guest_io_request','if(value.current!=expected)return 0;','if(value.current==0 && expected==0)return 0;'),
 ('guest_io_owner','value.phase=TSFP_IO_READY;','value.phase=TSFP_IO_PENDING;'),
 ('guest_io_owner','layout.bytes!=bytes','(layout.bytes==0 && bytes==0)'),
 ('guest_io_pool','if(!n->owns_storage) {drop_metadata(n);return 1;}','if(n->owns_storage<0) {drop_metadata(n);return 1;}'),
 ('guest_io_pool','if(!claim_range(n)) {drop_metadata(n);return 0;}','if(!claim_range(n) && n->address==0) {drop_metadata(n);return 0;}'),
 ('guest_io_pool','n->owns_storage=1;n->bytes=bytes;','n->owns_storage=0;n->bytes=bytes;'),
 ('guest_io_pool','n->owns_storage && other->owns_storage && other->cleaning','n->owns_storage && other->owns_storage && other->cleaning<0'),
])
def test_original_caller_owned_packet_adoption(tmp_path,mutation):
    providers=('guest_io_pool','guest_io_registry','guest_io_owner','guest_io_request','guest_allocation')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_pool_contract.c','io_adopt_contract.c']:
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        provider,old,new=mutation;p=tmp_path/(provider+'.c');s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_adopt_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken caller-owned adoption passed'
    else:assert run.returncode==0,run.stdout+run.stderr


@pytest.mark.skipif(os.name!='nt',reason='Real Windows event and metadata unmapping; portable providers tested separately')
def test_adopt_dispatch_complete_with_real_windows_event(tmp_path):
    providers=('guest_event','guest_io_build','guest_io_transfer','guest_io_complete','guest_io_call','guest_io_owner','guest_io_request','guest_io_scope','guest_io_devices','guest_io_target','guest_io_pool','guest_io_registry','guest_callback_call','guest_objects','guest_object_names','guest_device_factory','guest_device_body','guest_device_callback','guest_object_scope','guest_allocation','guest_state','guest_nonlocal_jump')
    headers=['guest_io_stack.h','guest_object_services.h','recomp_types.h','guest_lifetime.h']+[f'{p}.h' for p in providers if p not in ('guest_state','guest_nonlocal_jump')]
    for name in [*[f'{p}.c' for p in providers],*headers]:
        choices=[HERE/name,ROOT/'src/runtime'/name]
        source=next(p for p in choices if p.is_file());(tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'guest_device_contract.c').write_bytes((ROOT/'tests/compiler/guest_device_contract.c').read_bytes())
    (tmp_path/'io_adopt_pipeline_windows.c').write_bytes((HERE/'io_adopt_pipeline_windows.c').read_bytes())
    exe=_compile(tmp_path,tmp_path/'io_adopt_pipeline_windows.c',extra=[*[tmp_path/f'{p}.c' for p in providers],'/W4'])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
