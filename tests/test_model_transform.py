"""Audit the affine fast path and its post-callback generated entry."""
import hashlib
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler
from test_recomp_float_joins import ROOT, RUNTIME, config, FunctionTranslator

@pytest.fixture(autouse=True)
def restore_config():
    previous=dict(vars(config))
    yield
    vars(config).update(previous)

def synthetic_entry(**kwargs):
    start=0x151e80
    config._install([config.Section('.text', start, 1, 0, 1, True)],start,start,'entry-test')
    info={'name':'candidate_transform','end':start+1}
    return FunctionTranslator(b'\xc3',{start:info},**kwargs).translate_function(start,info)

def test_entry_order_and_diagnostic_fallback():
    code=synthetic_entry(trace_all_entries=True)
    assert code.index('RECOMP_TRACE_ENTER') < code.index('RECOMP_FAST_ENTRY') < code.index('loc_00151E80:')
    assert 'RECOMP_FAST_ENTRY' not in synthetic_entry(trace_functions={0x151e80})
    assert 'RECOMP_FAST_ENTRY' not in synthetic_entry(force_returns={0x151e80:7})
    types=(RUNTIME/'templates/runtime/recomp_types.h').read_text(encoding='utf-8')
    assert '#ifndef RECOMP_FAST_ENTRY\n#define RECOMP_FAST_ENTRY(site) 0\n#endif' in types

@pytest.mark.parametrize('reference,mutation', [('portable',x) for x in
    ('none','upper-lane','middle-register','range','alias','callback','stack')] + [('original','none'),('original','disabled')])
def test_transform_state(tmp_path,reference,mutation):
    if reference=='original':
        path=ROOT/'private/game/default.xbe'
        if not path.is_file():pytest.skip('Supported private US XBE required')
        image=path.read_bytes()
        assert hashlib.sha256(image).hexdigest()=='3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
        config.configure_from_xbe(str(path))
        info={'name':'original_transform','end':0x151f1c}
        original=FunctionTranslator(image,{0x151e80:info},trace_all_entries=True).translate_function(0x151e80,info)
        candidate=original.replace('original_transform','candidate_transform')
    else:
        original=(ROOT/'tests/model_transform_reference.h').read_text()
        candidate=synthetic_entry(trace_all_entries=True).replace('esp += 4; return;', 'portable_transform_body(); return;')
    helper=(ROOT/'src/pc_model_transform_guest.inc').read_text()
    changes={
        'upper-lane':('g_xmm0 = XMM_SCALAR(x);','g_xmm0.f[0] = x;'),
        'middle-register':('g_xmm4 = XMM_SCALAR(middle_x);','g_xmm4 = XMM_SCALAR(x_term);'),
        'range':('0x50000000u','0x7f800000u'),
        'alias':('((uint64_t)point < (uint64_t)matrix + 60u &&','(0 && (uint64_t)point < (uint64_t)matrix + 60u &&'),
        'stack':('g_esp += 4u;','g_esp += 8u;'),
    }
    if mutation in changes:
        old,new=changes[mutation];assert old in helper;helper=helper.replace(old,new)
    if mutation=='callback':
        lines=candidate.splitlines()
        first=next(i for i,line in enumerate(lines) if 'RECOMP_TRACE_ENTER' in line)
        second=next(i for i,line in enumerate(lines) if 'RECOMP_FAST_ENTRY' in line)
        lines[first],lines[second]=lines[second],lines[first];candidate='\n'.join(lines)
    (tmp_path/'pc_model_transform_guest.inc').write_text(helper)
    source=(ROOT/'tests/model_transform.c').read_text().replace('/* ORIGINAL */',original).replace('/* CANDIDATE */',candidate)
    source=source.replace('/* POLICY */',f'#define RECOMP_NUMERIC_POLICY_HEADER "{(ROOT / "src/pc_native_numeric.h").as_posix()}"')
    if mutation=='disabled':source='#define DISABLE_FAST 1\n'+source
    file=tmp_path/'model.c';file.write_text(source)
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('model.exe' if os.name=='nt' else 'model')
    include=RUNTIME/'templates/runtime'
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11','/O2','/I'+str(include),str(file),'/Fe:'+str(exe),'/Fo:'+str(tmp_path/'model.obj')]
    else:
        command=[compiler,'-std=c11','-O2','-I'+str(include),str(file),'-lm','-o',str(exe)]
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation in ('none','disabled'):
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS: 40000 transform comparisons' in result.stdout
    else:
        assert result.returncode!=0,'Mutation escaped: '+mutation
        assert 'Assertion' in result.stderr or 'assertion' in result.stderr,result.stderr
