"""Real native control transfer for the original guest execution checkpoint core."""
from pathlib import Path
import os,subprocess
import pytest
from test_translation_product import _compile
ROOT=Path(__file__).resolve().parents[2]
def fixture(tmp_path,mutation=None):
    for name in ('guest_context_execution.c','guest_context_execution.h','guest_x86_context.h','recomp_types.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        p=tmp_path/'guest_context_execution.c';s=p.read_text(encoding='utf-8');a,b=mutation;assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode())
    p=tmp_path/'context_execution_contract.c';p.write_bytes((ROOT/'tests/compiler'/p.name).read_bytes())
    extra=[tmp_path/'guest_context_execution.c',*(['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread'])]
    return _compile(tmp_path,p,extra=extra)
def run(exe,*args):return subprocess.run([str(exe),*args],capture_output=True,text=True,timeout=30)
def test_real_checkpoint_context_cleanup_roots_threads_and_fibers(tmp_path):
    r=run(fixture(tmp_path));assert r.returncode==0,r.stdout+r.stderr
@pytest.mark.parametrize('mutation',[
 ('saved.esp<=target->entry_sp','saved.esp>=target->entry_sp'),
 ('outcome==TSFP_CONTEXT_STEP_RETURN && next==root->stop','next==root->stop'),
 ('outcome!=TSFP_CONTEXT_STEP_YIELD && outcome!=TSFP_CONTEXT_STEP_RETURN','outcome==TSFP_CONTEXT_STEP_ERROR'),
 ('target->pending_apply=clean','target->pending_apply=0'),
 ('cursor->ops.rewind(cursor->ops.opaque,cursor->mark)','cursor->ops.rewind(cursor->ops.opaque,cursor->mark+1)'),
 ('--root_depth;free(discard);discard=previous;','root_depth-=0;free(discard);discard=previous;'),
 ('current_root=root->previous;--root_depth;','current_root=root->previous;root_depth-=0;'),
])
def test_compiled_broken_checkpoint_providers_rejected(tmp_path,mutation):
    r=run(fixture(tmp_path,mutation));assert r.returncode!=0,'Broken compiled execution checkpoint passed'
@pytest.mark.skipif(os.name!='nt',reason='owned Win32 fiber adapter boundary')
def test_foreign_adapter_must_not_return(tmp_path):
    r=run(fixture(tmp_path),'bad-adapter');assert r.returncode!=0
    assert 'foreign-stack adapter returned' in r.stderr
