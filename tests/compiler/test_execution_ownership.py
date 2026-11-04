"""Compile actual execution ownership and guest nonlocal-cleanup contracts."""
from pathlib import Path
import os,subprocess
import pytest
from test_translation_product import _compile
ROOT=Path(__file__).resolve().parents[2]


def execute(tmp_path,mutation=None):
    providers=('guest_execution_state','guest_execution_scope','guest_object_scope',
               'guest_objects','guest_nonlocal_jump','guest_state')
    headers=('guest_execution_state.h','guest_execution_scope.h','guest_object_scope.h',
             'guest_objects.h','guest_lifetime.h','recomp_types.h','guest_status_flags.h')
    for name in (*headers,*(n+'.c' for n in providers)):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        name,old,new=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8')
        assert s.count(old)==1;p.write_bytes(s.replace(old,new).encode())
    source=tmp_path/'execution_ownership_contract.c'
    source.write_bytes((ROOT/'tests/compiler'/source.name).read_bytes())
    flags=['/W4','/DTSFP_RECORD_STATUS_FLAGS=1'] if os.name=='nt' else ['-Wall','-Wextra','-pthread','-DTSFP_RECORD_STATUS_FLAGS=1']
    exe=_compile(tmp_path,source,extra=[*(tmp_path/(n+'.c') for n in providers),*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)


def test_exact_state_lease_order_actual_guest_jump_and_threads(tmp_path):
    r=execute(tmp_path);assert r.returncode==0,r.stdout+r.stderr


@pytest.mark.parametrize('mutation',[
    ('guest_execution_state.c','saved.owner!=owner()','saved.owner==0'),
    ('guest_execution_state.c','if(!current.active_depth)ops->activate(ops->opaque);','ops->activate(ops->opaque);'),
    ('guest_execution_state.c','current.active_depth=saved.active_depth;return 1;','current.active_depth=1;return 1;'),
    ('guest_execution_scope.c','unwinding->target=frame->saved;return 1;',
        'return tsfp_guest_execution_restore(frame->saved.domain,&frame->saved);'),
    ('guest_execution_scope.c','ok=tsfp_guest_scope_unwind(stack,mark);','ok=1;'),
    ('guest_execution_scope.c','restored=tsfp_guest_execution_restore(ops,resume?&batch.target:&empty);','restored=1;'),
    ('guest_execution_scope.c','return unwind_to(stack,0,ops,0);','return unwind_to(stack,0,ops,1);'),
])
def test_incorrect_compiled_ownership_providers_rejected(tmp_path,mutation):
    r=execute(tmp_path,mutation);assert r.returncode!=0,'Incorrect ownership provider passed'
