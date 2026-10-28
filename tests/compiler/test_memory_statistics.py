"""Independent statistics policy with 64-bit host counts and atomic publication."""
from pathlib import Path
import os
import subprocess
import sys
import pytest

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile


def build(tmp_path,mutation=None):
    for name in ('memory_statistics.c','memory_statistics.h','memory_statistics_contracts.c'):
        path=HERE/name
        if not path.exists():path=ROOT/'src/runtime'/name
        source=path.read_text(encoding='utf-8')
        if name=='memory_statistics.c' and mutation:
            old,new=mutation
            assert source.count(old)==1
            source=source.replace(old,new)
        (tmp_path/name).write_text(source,encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    return _compile(tmp_path,tmp_path/'memory_statistics_contracts.c',extra=[tmp_path/'memory_statistics.c',*flags])


def test_statistics_policy(tmp_path):
    exe=build(tmp_path)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert 'contracts passed' in run.stdout


@pytest.mark.parametrize('mutation',[
    ('pages=bytes/4096u;','pages=(uint32_t)(bytes/4096u);'),
    ('if(!available(context,&bytes))return UINT32_C(0xc0000001);',
     'if(!available(context,&bytes)){ memset(words,0,36);return UINT32_C(0xc0000001); }'),
])
def test_statistics_negative_controls(tmp_path,mutation):
    exe=build(tmp_path,mutation)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert run.returncode!=0,'Mutated statistics provider incorrectly passed'
