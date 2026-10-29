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
pytestmark=pytest.mark.skipif(os.name!='nt',reason='Actual Windows request-pool concurrency; shared contracts separate')

def test_real_concurrent_request_allocation_and_stop(tmp_path):
    providers=('guest_io_pool','guest_io_registry','guest_io_owner','guest_io_request')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_pool_windows.c']:
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_allocation.c','guest_allocation.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    exe=_compile(tmp_path,tmp_path/'io_pool_windows.c',extra=[
        *[tmp_path/f'{p}.c' for p in providers],tmp_path/'guest_allocation.c','/W4'])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=45)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS:' in result.stdout
