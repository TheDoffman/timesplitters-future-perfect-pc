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
@pytest.mark.parametrize('mutation',[None,
    ('put(frame+4,argument);','put(frame+4,argument+1);'),
    ('balanced=g_esp==saved.esp;','balanced=1;'),
    ('g_xmm7=s->xmm[7];','g_xmm7=s->xmm[6];g_xmm7.q[1]^=1;'),
    ('g_fp_cc=s->cc;','g_fp_cc=s->control;'),
    ('g_seh_ebp=s->seh;','g_seh_ebp=s->ebp;'),
])
def test_callback_call_state_and_real_native_jump(tmp_path,mutation):
    for name in ('guest_callback_call.c','guest_callback_call.h','guest_callback_call_contract.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('recomp_types.h','guest_state.c','guest_nonlocal_jump.c','guest_lifetime.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_callback_call.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_callback_call_contract.c',extra=[tmp_path/n for n in ('guest_callback_call.c','guest_state.c','guest_nonlocal_jump.c')]+flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert result.returncode!=0,'Broken callback wrapper passed'
    else:assert result.returncode==0,result.stdout+result.stderr


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
