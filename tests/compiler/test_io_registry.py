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

def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name

@pytest.mark.parametrize('mutation',[None,
    ('if(tsfp_io_owner_retain(r->entries[i].owner))owner=', 'if(1)owner='),
    ('r->entries[i].owner==owner || (r->entries[i].owner && r->entries[i].address==owner->address)', 'r->entries[i].owner==NULL && owner==NULL'),
    ('if(owner)tsfp_io_owner_release(owner);', 'if(owner) { (void)owner; }'),
    ('if(!r->closed)for', 'if(1)for'),
])
def test_request_registry_retains_lookup_and_detaches(tmp_path,mutation):
    for name in ('guest_io_registry.c','guest_io_registry.h','guest_io_owner.c','guest_io_owner.h',
                 'guest_io_request.c','guest_io_request.h','guest_io_stack.h','io_registry_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_io_registry.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_registry_contract.c',extra=[
        tmp_path/'guest_io_registry.c',tmp_path/'guest_io_owner.c',tmp_path/'guest_io_request.c',*flags])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert result.returncode!=0,'Broken request registry passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout
