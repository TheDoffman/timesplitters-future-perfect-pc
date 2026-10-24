"""Launch-only debug policy must not burden each acknowledgement-worker pass."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope='module')
def binaries(tmp_path_factory):
    folder = tmp_path_factory.mktemp('pushbuffer-policy')
    actual = (ROOT/'external/ts3FPrecomp/src/kernel/nv2a_pb_scan.c').read_text()
    assert 'if (s_scan_enabled < 0)' in actual, 'Apply the complete patch stack'
    compiler, env = native_compiler(folder)
    result = {}
    for mutation in ('none', 'eager', 'ignore-executor'):
        text = actual
        if mutation == 'eager':
            text = text.replace('if (s_scan_enabled < 0)', 'if (1)')
        elif mutation == 'ignore-executor':
            text = text.replace('!(s_scan_enabled || s_exec_enabled)', '!s_scan_enabled')
        source = folder/f'{mutation}.c'
        source.write_text((ROOT/'tests/pushbuffer_policy.c').read_text().replace('/* PRODUCTION_SCAN */', text))
        output = folder/(mutation + ('.exe' if os.name=='nt' else ''))
        args = ([compiler,'/nologo','/std:c11','/W4',str(source),'/Fe:'+str(output)] if os.name=='nt'
                else [compiler,'-std=c11','-Wall','-Wextra','-Werror',str(source),'-o',str(output)])
        subprocess.run(args,cwd=folder,env=env,capture_output=True,text=True,check=True,timeout=60)
        result[mutation] = output
    return result

@pytest.mark.parametrize('scan,execute', [(0,0),(1,0),(0,1),(1,1),(2,0),(0,2)])
def test_pushbuffer_policy(binaries, scan, execute):
    result = subprocess.run([str(binaries['none']),str(scan),str(execute)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
    assert ('[PB]' in result.stderr)==bool(scan)

@pytest.mark.parametrize('mutation,scan,execute', [('eager',0,0),('ignore-executor',0,1)])
def test_pushbuffer_policy_mutations(binaries, mutation, scan, execute):
    result = subprocess.run([str(binaries[mutation]),str(scan),str(execute)],capture_output=True,text=True,timeout=10)
    assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr
