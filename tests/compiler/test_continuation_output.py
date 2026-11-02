"""Publication failures, real CLI output and separately compiled continuation chunks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

import pytest
import continuation_output as output
from test_translation_function import translator
from test_translation_cli import fixture
from test_translation_product import _compile
import translation_cli as cli

ROOT=Path(__file__).resolve().parents[2]


def snapshot(directory):
    return {p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in directory.iterdir() if p.is_file()}


def test_generation_failure_preserves_previous_complete_publication(tmp_path,monkeypatch):
    t,info=translator('b82a000000c3')
    report=output.publish(t,[(0x1000,info)],tmp_path)
    before=snapshot(tmp_path)
    def fail(*args,**kwargs):raise RuntimeError('generation failed')
    monkeypatch.setattr(output,'emit',fail)
    with pytest.raises(RuntimeError,match='generation failed'):
        output.publish(t,[(0x1000,info)],tmp_path)
    assert snapshot(tmp_path)==before
    assert report['functions']==1 and report['instruction_entries']==2
    assert not list(tmp_path.glob('.context-generation-*'))


def test_idempotence_stale_cleanup_and_explicit_unsupported_coverage(tmp_path):
    t,info=translator('c3')
    output.publish(t,[(0x1000,info)],tmp_path)
    before=snapshot(tmp_path)
    output.publish(t,[(0x1000,info)],tmp_path)
    assert snapshot(tmp_path)==before
    unrelated=tmp_path/'recomp_user.c';unrelated.write_bytes(b'keep')
    stale=tmp_path/'recomp_context_0009.c';stale.write_bytes(b'stale')
    t,info=translator('0f0bc3')
    report=output.publish(t,[(0x1000,info)],tmp_path)
    assert report['functions']==0 and len(report['rejected'])==1
    assert report['instruction_entries']==0 and not stale.exists()
    assert unrelated.read_bytes()==b'keep'
    assert not (tmp_path/'recomp_context_0000.c').exists()
    for name,digest in report['sha256_text'].items():
        assert hashlib.sha256((tmp_path/name).read_text(encoding='utf-8').encode()).hexdigest()==digest
    assert json.loads((tmp_path/output.MANIFEST).read_text())==report


def test_manual_entries_are_excluded_but_kept_originals_have_real_bodies(tmp_path):
    t,info=translator('c3');t.lifter.manual_functions={0x1000}
    report=output.publish(t,[(0x1000,info)],tmp_path)
    assert report['opaque_entries']==[0x1000] and not report['functions']
    report=output.publish(t,[(0x1000,info)],tmp_path,kept={0x1000})
    assert not report['opaque_entries'] and report['functions']==1
    assert t.lifter.manual_functions=={0x1000}


@pytest.mark.parametrize('flags',[[],['--split','2','--header'],['--split','2','--only-hle-thunks']])
def test_cli_rejects_incompatible_modes_before_publication(tmp_path,flags):
    args,_=fixture(tmp_path)
    assert cli.main(args+flags+['--context-continuations'])==1
    assert not (tmp_path/'gen').exists()


@pytest.mark.parametrize('refs',[False,True])
def test_cli_exports_separately_compilable_exact_pc_execution(tmp_path,refs):
    args,_=fixture(tmp_path)
    (tmp_path/'types.h').write_bytes((ROOT/'src/runtime/recomp_types.h').read_bytes())
    assert cli.main(args+['--all','--split','2','--context-continuations'])==0
    directory=tmp_path/'gen'
    report=json.loads((directory/output.MANIFEST).read_text())
    assert report['functions']==6 and report['instruction_entries']==12
    assert not report['rejected'] and not report['opaque_entries']
    for name in ('guest_state.c','guest_status_flags.h'):
        (directory/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    source=directory/'harness.c'
    source.write_bytes(b'''#include "recomp_types.h"
#include "guest_context_execution.h"
ptrdiff_t g_xbox_mem_offset;
static union {uint64_t alignment;unsigned char data[8192];} memory;
int main(void){
g_xbox_mem_offset=(ptrdiff_t)memory.data;g_esp=1000;MEM32(1000)=0x12345678;
uint32_t next=0;
if(!tsfp_context_generated_admit(0x11005)||tsfp_context_generated_admit(0x11001))return 1;
if(tsfp_context_generated_step(0x11050,&next)!=TSFP_CONTEXT_STEP_RETURN)return 2;
if(g_eax!=6||next!=0x12345678||g_esp!=1004)return 3;
g_eax=99;g_esp=1000;
if(tsfp_context_generated_step(0x11005,&next)!=TSFP_CONTEXT_STEP_RETURN)return 4;
return g_eax!=99||next!=0x12345678||g_esp!=1004;
}
''')
    policy=directory/'policy.h'
    policy.write_bytes(('#define TSFP_RECORD_STATUS_FLAGS 1\n'+
        ('#define RECOMP_USE_GPR_REFS 1\n#define RECOMP_GPR_ANCHORS(site) 1\n' if refs else '')).encode())
    flags=['/FI'+str(policy)] if os.name=='nt' else ['-include',str(policy)]
    exe=_compile(directory,source,extra=[directory/'guest_state.c',*directory.glob('recomp_context_*.c'),*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    assert run.returncode==0,run.stdout+run.stderr
