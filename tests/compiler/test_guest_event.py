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
 ('value[2]=4;','value[2]=16;'),
 ('put(value+8,address+8);','put(value+8,0);'),
 ('type>1','type>255'),
 ('span<16','span<15'),
 ('if(!view->synchronization)return tsfp_event_state(view);','if(view->synchronization>1)return tsfp_event_state(view);'),
 ('tsfp_atomic_cas32(view->signal,1,0)','tsfp_atomic_cas32(view->signal,1,1)'),
])
def test_original_event_bytes_and_native_waiters(tmp_path,mutation):
    for name in ('guest_event.c','guest_event.h','recomp_types.h','guest_event_contract.c'):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_event.c';s=p.read_text(encoding='utf-8');assert old in s;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'guest_event_contract.c',extra=[tmp_path/'guest_event.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken guest event passed'
    else:assert run.returncode==0,run.stdout+run.stderr

@pytest.mark.parametrize('mutation',[None,
 ('address,type,initial_state','address,1,initial_state'),
 ('memcpy(mapping,candidate,sizeof(candidate));','memcpy(mapping,candidate,sizeof(candidate)-1);'),
])
def test_original_kernel_event_initializer(tmp_path,mutation):
    for name in ('guest_event.c','guest_event.h','recomp_types.h','guest_kernel_event_init.inc','kernel_event_init_contract.c'):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_kernel_event_init.inc';s=p.read_text(encoding='utf-8');assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'kernel_event_init_contract.c',extra=[tmp_path/'guest_event.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken kernel event initializer passed'
    else:assert run.returncode==0,run.stdout+run.stderr
