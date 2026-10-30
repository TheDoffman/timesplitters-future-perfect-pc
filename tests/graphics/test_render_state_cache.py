"""Complete renderer state module: bound descriptors, reuse and ownership."""


import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]
RUNTIME=ROOT/'external/ts3FPrecomp/src'


@pytest.fixture(scope='module')
def state_cache_runs(tmp_path_factory):
    folder=tmp_path_factory.mktemp('render-state-cache')
    source=(RUNTIME/'d3d/d3d8_states.c').read_text()
    # Reconstruct the actual pre-change module in a disposable tree. Both
    # variants execute the same authored translations; compare every bind.
    baseline=folder/'baseline/src/d3d/d3d8_states.c'
    baseline.parent.mkdir(parents=True)
    baseline.write_text(source)
    for patch in ('0500-sampler-cache-retry.patch','0490-render-state-cache.patch'):
        reversed_patch=subprocess.run(['git','apply','--reverse',str(ROOT/'patches/graphics/rendering'/patch)],
            cwd=folder/'baseline',capture_output=True,text=True,timeout=10)
        assert reversed_patch.returncode==0,reversed_patch.stdout+reversed_patch.stderr
    original=baseline.read_text()
    reapplied=subprocess.run(['git','apply',str(ROOT/'patches/graphics/rendering/0500-sampler-cache-retry.patch')],
        cwd=folder/'baseline',capture_output=True,text=True,timeout=10)
    assert reapplied.returncode==0,reapplied.stdout+reapplied.stderr
    variants={'baseline':original,'sampler-only':baseline.read_text(),'cache':source,
              'no-reuse':source.replace('return name##_cache[i].state;', 'break;'),
              'lost-mask':source.replace('bd.write_mask = rs[D3DRS_COLORWRITEENABLE] & 0x0F;', 'bd.write_mask = 15;'),
              'failed-hit':source.replace('if (!state) return NULL;', '/* mutation: retain failed entry */')}
    compiler,env=native_compiler(folder);results={}
    for name,implementation in variants.items():
        target=folder/name;target.mkdir(exist_ok=True)
        (target/'render_state_impl.h').write_text(implementation)
        executable=target/('states.exe' if os.name=='nt' else 'states')
        includes=[target,RUNTIME,RUNTIME/'d3d',RUNTIME/'config',ROOT/'src']
        defines=[] if name in ('baseline','sampler-only') else ['EXPECT_CACHE=1']
        if os.name=='nt':
            command=[compiler,'/nologo','/std:c11','/O2',*('/D'+x for x in defines),
                     *('/I'+str(p) for p in includes),str(ROOT/'tests/graphics/render_state_cache.c'),'/Fe:'+str(executable)]
        else:
            command=[compiler,'-std=c11','-O2','-DTSFP_RHI_RENDERER',*('-D'+x for x in defines),
                     *('-I'+str(p) for p in includes),str(ROOT/'tests/graphics/render_state_cache.c'),'-o',str(executable)]
        built=subprocess.run(command,cwd=target,env=env,capture_output=True,text=True,timeout=60)
        assert built.returncode==0,built.stdout+built.stderr
        results[name]=subprocess.run([str(executable)],capture_output=True,text=True,timeout=10)
        for mode in ('sampler-retry','sampler-replace'):
            results[name+'-'+mode]=subprocess.run([str(executable),mode],capture_output=True,text=True,timeout=10)
    return results


def test_render_state_cache_matches_original_binds(state_cache_runs):
    for name in ('baseline','sampler-only','cache'):
        result=state_cache_runs[name]
        assert result.returncode==0,result.stdout+result.stderr
    assert state_cache_runs['cache'].stdout==state_cache_runs['baseline'].stdout
    assert state_cache_runs['sampler-only'].stdout==state_cache_runs['baseline'].stdout
    assert 'reuse-phase creates=2000,1000,2000' in state_cache_runs['baseline'].stderr
    assert 'reuse-phase creates=4,4,6' in state_cache_runs['cache'].stderr


@pytest.mark.parametrize('mode,marker',[
    ('sampler-retry','attempts[3]==before+2'),
    ('sampler-replace','held->live&&g_sampler_states[1]==held'),
])
def test_sampler_allocation_failure_recovers(state_cache_runs,mode,marker):
    previous=state_cache_runs['baseline-'+mode]
    assert previous.returncode!=0 and marker in previous.stderr,previous.stdout+previous.stderr
    for implementation in ('sampler-only','cache'):
        current=state_cache_runs[implementation+'-'+mode]
        assert current.returncode==0,current.stdout+current.stderr
        assert 'PASS: sampler failure retries' in current.stdout
        if mode=='sampler-retry':
            assert current.stderr.count('D3D8: CreateSamplerState failed')==4


@pytest.mark.parametrize('name',['no-reuse','lost-mask','failed-hit'])
def test_render_state_cache_negative_controls(state_cache_runs,name):
    result=state_cache_runs[name]
    assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr
