"""Execute original caller-cleaned handler calls and reject malformed cleanup."""
from pathlib import Path
import os,subprocess
import pytest
from test_translation_product import _compile
ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('status',[False,True])
@pytest.mark.parametrize('mutation',[None,
 ('caller_cleanup?4*count:0','caller_cleanup?0:0'),
 ('if(balanced && result)*result=value;','if(result)*result=value;'),
 ('value=g_eax;balanced=','value=saved.eax;balanced='),
 ('candidate.words[i]=arguments[i]','candidate.words[i]=arguments[i+1]'),
])
def test_cdecl_handler_frame_state_result_and_native_transfer(tmp_path,status,mutation):
    names=('guest_callback_call.c','guest_callback_call.h','recomp_types.h','guest_status_flags.h','guest_state.c','guest_nonlocal_jump.c','guest_lifetime.h')
    for n in names:(tmp_path/n).write_bytes((ROOT/'src/runtime'/n).read_bytes())
    if mutation:
        p=tmp_path/'guest_callback_call.c';s=p.read_text(encoding='utf-8');a,b=mutation;assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    p=tmp_path/'guest_cdecl_callback_contract.c';p.write_bytes((ROOT/'tests/compiler'/p.name).read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    if status:flags+=['/DTSFP_RECORD_STATUS_FLAGS=1' if os.name=='nt' else '-DTSFP_RECORD_STATUS_FLAGS=1']
    exe=_compile(tmp_path,p,extra=[tmp_path/n for n in ('guest_callback_call.c','guest_state.c','guest_nonlocal_jump.c')]+flags)
    r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert r.returncode!=0,'Compiled incorrect cdecl callback provider passed'
    else:assert r.returncode==0,r.stdout+r.stderr
