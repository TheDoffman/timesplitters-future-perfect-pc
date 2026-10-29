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
    ('p[0x1e]=1','p[0x1d]=1'),
    ('address+TSFP_DEVICE_BODY_BYTES:0','address+0x38:0'),
    ('p[0x28]=0x14','p[0x28]=4'),
    ('p[0x36]=4','p[0x36]=16'),
    ('p[0x1f]=1','p[0x1e]=1'),
])
def test_original_device_layout_and_bounds(tmp_path,mutation):
    for name in ('guest_device_body.c','guest_device_body.h','guest_device_body_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_device_body.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_device_body_contract.c',extra=[tmp_path/'guest_device_body.c',*flags])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert result.returncode!=0,'Broken device layout passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
