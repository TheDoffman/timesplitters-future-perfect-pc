from pathlib import Path
import os,sys,subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
pytestmark=pytest.mark.skipif(os.name!='nt',reason='Actual Windows host lock/heap binding')
@pytest.mark.parametrize('case',[1,2,3,4,
    ('ops.defer_finalization=1;','ops.defer_finalization=0;',1),
    ('bridge_object_services.type_id[TSFP_KIND_DEVICE]=XBOX_KERNEL_DATA_BASE+KDATA_IO_DEVICE_TYPE;','bridge_object_services.type_id[TSFP_KIND_DEVICE]=XBOX_KERNEL_DATA_BASE+KDATA_FILE_OBJ_TYPE;',1),
    ('entry=recomp_lookup_manual(address);return entry?entry:recomp_lookup(address);','entry=recomp_lookup(address);return entry?entry:recomp_lookup_manual(address);',1),
    ('*(int *)context=0;','*(int *)context=abandoned?1:0;',2),
    ('LeaveCriticalSection(&bridge_device_lock);','(void)&bridge_device_lock;',1),
    ('guest_device_event_batch.c','while(frame->count) {','while(frame->count>1) {',4),
    ('guest_kernel_device_events.inc','if(state<0)return;\n    legacy();','if(state<0)state=0;\n    legacy();',3),
    ('guest_kernel_device_events.inc','&bridge_devices,array,count);','&bridge_devices,array+4,count);',3),
    ('guest_kernel_device_events.inc','value=tsfp_device_event_reset(&frame.event);','value=tsfp_device_event_set(&frame.event);',3),
])
def test_real_windows_device_host_binding(tmp_path,case):
    providers=('guest_objects','guest_object_services','guest_object_scope','guest_device_factory','guest_device_body','guest_device_callback','guest_callback_call','guest_device_dispatch','guest_device_event','guest_event','guest_device_event_scope','guest_device_event_batch')
    for name in [f'{n}.{ext}' for n in providers for ext in ('c','h')]+['guest_kernel_device_host.inc','guest_kernel_devices.inc','guest_kernel_device_events.inc','guest_device_host_contract.c']:
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_object_names.c','guest_object_names.h','guest_name_service.c','guest_name_service.h','guest_kernel_names.inc'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_allocation.c','guest_allocation.h','guest_virtual_memory.h','recomp_types.h','guest_state.c','guest_lifetime.h','guest_nonlocal_jump.c'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    mode=case if isinstance(case,int) else case[-1]
    if isinstance(case,tuple):
        name,old,new,_=case if len(case)==4 else ('guest_kernel_device_host.inc',*case)
        p=tmp_path/name;s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new),encoding='utf-8')
    runtime=ROOT/'external/ts3FPrecomp'
    flags=['/W4','/D_CRT_SECURE_NO_WARNINGS',f'/DTSFP_HOST_MODE={mode}','/I'+str(runtime/'src'),'/I'+str(runtime/'src/kernel')]
    sources=[tmp_path/f'{n}.c' for n in providers+('guest_object_names','guest_name_service','guest_allocation','guest_state','guest_nonlocal_jump')]
    exe=_compile(tmp_path,tmp_path/'guest_device_host_contract.c',extra=sources+flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if isinstance(case,tuple):assert result.returncode!=0,'Broken host binding passed'
    else:assert result.returncode==0,result.stdout+result.stderr


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
