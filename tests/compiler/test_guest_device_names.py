from pathlib import Path
import os
import sys
import subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

@pytest.mark.parametrize('mutation',[None,
    ('word(target,body);','word(target,(uint32_t)lease.host);'),
    ('if(expected_type && (lease.kind>=TSFP_KIND_COUNT ||',
     'if(expected_type==UINT32_MAX && (lease.kind>=TSFP_KIND_COUNT ||'),
])
def test_typed_device_body_reference_through_counted_namespace(tmp_path,mutation):
    for name in ('guest_objects.c','guest_objects.h','guest_object_services.c',
                 'guest_object_services.h','guest_device_name_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_object_names.c','guest_object_names.h','guest_name_service.c','guest_name_service.h'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_allocation.c','guest_allocation.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_object_services.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_device_name_contract.c',extra=[tmp_path/name for name in (
        'guest_objects.c','guest_object_services.c','guest_object_names.c','guest_name_service.c','guest_allocation.c')]+flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert result.returncode!=0,'Broken typed device namespace passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
