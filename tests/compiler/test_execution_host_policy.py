"""Compile production host policies with deterministic races and native APIs."""
from pathlib import Path
import os,subprocess
import pytest
from test_translation_product import _compile
ROOT=Path(__file__).resolve().parents[2]

def execute(tmp_path,fixture,mutation=None):
    names=('guest_execution_host.inc','guest_execution_cpu.inc','guest_cpu_choice.h',
           'guest_kernel_execution.inc','guest_kernel_execution.h',
           'guest_execution_state.h','guest_execution_state.c')
    for name in names:
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        name,old,new=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8')
        assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    source=tmp_path/fixture
    source.write_bytes((ROOT/'tests/compiler'/fixture).read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,source,extra=[tmp_path/'guest_execution_state.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)

def test_host_config_ownership_bounded_waits_and_accounting(tmp_path):
    r=execute(tmp_path,'execution_host_policy_contract.c');assert r.returncode==0,r.stdout+r.stderr

def test_cpu_records_masks_settings_and_malformed_api_results(tmp_path):
    r=execute(tmp_path,'execution_cpu_contract.c');assert r.returncode==0,r.stdout+r.stderr

@pytest.mark.parametrize('mutation',[
    ('guest_cpu_choice.h','if(group || index>=64)return;','(void)group;if(index>=64)return;'),
    ('guest_cpu_choice.h','bit>choice->mask','bit<choice->mask'),
    ('guest_cpu_choice.h','if(errno || end==text || *end || !requested)return 0;',
       'if(errno || end==text || !requested)return 0;'),
    ('guest_execution_cpu.inc','&& received<=capacity','&& received==0'),
])
def test_wrong_cpu_policies_compile_then_fail(tmp_path,mutation):
    r=execute(tmp_path,'execution_cpu_contract.c',mutation)
    assert r.returncode!=0,'Incorrect CPU policy passed'

@pytest.mark.parametrize('mutation',[
    ('guest_execution_host.inc','if(execution_host_park_depth++)return;','++execution_host_park_depth;'),
    ('guest_execution_host.inc','if(--execution_host_park_depth || !execution_host_park_held)return;',
       'if(--execution_host_park_depth)return;'),
    ('guest_execution_host.inc','if(!owner || owner==me)break;','(void)me;if(!owner)break;'),
    ('guest_execution_host.inc','if(running>1)InterlockedIncrement(&execution_host_overlaps);',
       'InterlockedIncrement(&execution_host_overlaps);'),
    ('guest_execution_host.inc','execution_host_park_on=0;','execution_host_park_on=1;'),
])
def test_wrong_host_policies_compile_then_fail(tmp_path,mutation):
    r=execute(tmp_path,'execution_host_policy_contract.c',mutation)
    assert r.returncode!=0,'Incorrect host policy passed'

@pytest.mark.skipif(os.name!='nt',reason='Real Windows scheduling and affinity APIs')
def test_real_windows_threads_lock_park_and_affinity(tmp_path):
    r=execute(tmp_path,'execution_host_windows.c');assert r.returncode==0,r.stdout+r.stderr
