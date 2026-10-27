"""Compare production indexed-draw dispatch with the pre-copy-elision driver."""


import os
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(sys.platform not in ('win32','darwin'), reason='Native HLE hosts')
@pytest.mark.parametrize('mutation',['none','always-copy','omit-rebase','leak-indices'])
def test_zero_base_indices(tmp_path,mutation):
    text=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8_vertex.c').read_text()
    start=text.index('HLE_EXPORT(D3DDevice_DrawIndexedVertices)\n')
    candidate=text[start:text.index('\n#if defined(_WIN32) || defined(__APPLE__)\nstatic void forward_constants',start)]
    assert 'if (!lo || rebased)' in candidate
    reference=(ROOT/'tests/support/zero_base_indices_reference.inc').read_text()
    replacements={
        'always-copy':('if (lo) rebased = malloc(', 'if (1) rebased = malloc('),
        'omit-rebase':('rebased[i] = (uint16_t)(idx[i] - lo);','rebased[i] = idx[i];'),
        'leak-indices':('free(rebased);','/* intentionally leaked index copy */'),
    }
    if mutation!='none':
        old,new=replacements[mutation];assert candidate.count(old)==1
        candidate=candidate.replace(old,new)
    def instrument(body,name):
        return body.replace('HLE_EXPORT(D3DDevice_DrawIndexedVertices)','static void '+name+'(void)').replace('malloc(','tracked_malloc(').replace('free(rebased)','tracked_free(rebased)')
    source=tmp_path/'indices.c'
    source.write_text((ROOT/'tests/graphics/zero_base_indices.c').read_text()
        .replace('/* ORIGINAL */',instrument(reference,'driver_original'))
        .replace('/* CANDIDATE */',instrument(candidate,'driver_candidate')))
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('indices.exe' if os.name=='nt' else 'indices')
    command=([compiler,'/nologo','/std:c11','/O2',str(source),'/Fe:'+str(exe)] if os.name=='nt' else
             [compiler,'-std=c11','-O2',str(source),'-o',str(exe)])
    compiled=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert compiled.returncode==0,compiled.stdout+compiled.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation=='none':assert result.returncode==0,result.stdout+result.stderr
    else:assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr
