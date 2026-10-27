"""Layout-slot preparation must preserve full-module live host call streams."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

@pytest.fixture(scope='module')
def state_slot_results(tmp_path_factory):
    folder=tmp_path_factory.mktemp('state-layout-slots')
    runtime=ROOT/'external/ts3FPrecomp/src'
    text=(runtime/'hle/hle_d3d8_state.c').read_text()
    base=folder/'original';(base/'src/hle').mkdir(parents=True)
    (base/'src/hle/hle_d3d8_state.c').write_text(text)
    reversed_patch=subprocess.run(['git','apply','--reverse',str(ROOT/'patches/graphics/rendering/0550-state-layout-slots.patch')],cwd=base,capture_output=True,text=True)
    assert reversed_patch.returncode==0,reversed_patch.stderr
    original=(base/'src/hle/hle_d3d8_state.c').read_text()
    assert 'prepare_slots' not in original
    compiler,env=native_compiler(folder)
    variants={'original':original,'current':text,
              'stale-layout':text.replace('slot_layout == (int)g_layout','slot_layout >= 0'),
              'stale-values':text.replace('uint32_t v=slot<0 ? 0u : HLE_MEM32(hle_var_D3D_g_RenderState+4u*(uint32_t)slot);','uint32_t v=g_prev_valid ? g_prev_rs[x] : (slot<0 ? 0u : HLE_MEM32(hle_var_D3D_g_RenderState+4u*(uint32_t)slot));')}
    assert all(v!=text for k,v in variants.items() if k!='current')
    results={}
    for name,source in variants.items():
        d=folder/name;(d/'hle').mkdir(parents=True,exist_ok=True)
        (d/'hle/hle_d3d8_state.c').write_text(source)
        exe=d/('states.exe' if os.name=='nt' else 'states')
        includes=[d,runtime,ROOT/'src',*(runtime/n for n in ('hle','d3d','kernel','config','platform'))]
        args=([compiler,'/nologo','/std:c11','/O2','/Gy','/Gw',*('/I'+str(p) for p in includes),str(ROOT/'tests/state_layout_slots.c'),'/Fe:'+str(exe),'/Fo:'+str(d/'states.obj'),'/link','/OPT:REF']
              if os.name=='nt' else [compiler,'-std=gnu11','-O2','-Wl,-dead_strip',*('-I'+str(p) for p in includes),str(ROOT/'tests/state_layout_slots.c'),'-o',str(exe)])
        compiled=subprocess.run(args,cwd=d,env=env,capture_output=True,text=True,timeout=60)
        assert compiled.returncode==0,compiled.stdout+compiled.stderr
        ran=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
        assert ran.returncode==0,ran.stdout+ran.stderr
        assert len(ran.stdout.split())==2
        results[name]=ran.stdout
    return results

def test_layout_slot_live_state_parity(state_slot_results):
    assert state_slot_results['current']==state_slot_results['original']

@pytest.mark.parametrize('mutation',['stale-layout','stale-values'])
def test_layout_slot_negative_controls(state_slot_results,mutation):
    assert state_slot_results[mutation]!=state_slot_results['original']
