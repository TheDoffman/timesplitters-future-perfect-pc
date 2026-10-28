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
from test_io_packet_abi import function
@pytest.mark.parametrize('mutation',[None,'create-cleanup','delete-cleanup','extra-argument'])
def test_candidate_device_adapter_with_actual_cleanup(tmp_path,mutation):
    for name in ('guest_device_factory.h','guest_object_services.h','guest_objects.h','guest_kernel_devices.inc'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    (tmp_path/'guest_object_names.h').write_bytes(source_file('guest_object_names.h').read_bytes())
    text=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    cleanup=function(text,'static int stdcall_args_for_ordinal(ULONG ordinal)')
    if mutation in ('create-cleanup','delete-cleanup'):
        old,new=(('case  65: return 24;','case  65: return 28;') if mutation=='create-cleanup' else ('case  68: return  4;','case  68: return  8;'))
        assert cleanup.count(old)==1;cleanup=cleanup.replace(old,new)
    if mutation=='extra-argument':
        p=tmp_path/'guest_kernel_devices.inc';s=p.read_text();old='output=STACK_ARG(5)'
        assert s.count(old)==1;p.write_text(s.replace(old,'output=STACK_ARG(6)'))
    fixture=tmp_path/'device.c';fixture.write_text((HERE/'guest_device_abi_contract.c').read_text().replace('/*ACTUAL_CLEANUP*/',cleanup),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,fixture,extra=flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert result.returncode!=0,'Broken device ABI passed'
    else:assert result.returncode==0,result.stdout+result.stderr


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
