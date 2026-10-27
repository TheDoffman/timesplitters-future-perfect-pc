"""Pushed-array copy specialization must preserve the original draw inputs."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('mutation',['none','overread','wrong-stride','wide-source'])
def test_push_gather(tmp_path,mutation):
    text=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8_vertex.c').read_text(encoding='utf-8')
    fmt=text[text.index('static UINT vsdt_bytes('):text.index('/* The commands between BeginPush')]
    start=text.index('static void push_copy_attribute(')
    end=text.index('/* Title RAM or the contiguous window. */',start)
    candidate=text[start:end]
    if mutation=='overread':candidate=candidate.replace('PUSH_COPY_BYTES(12)', 'PUSH_COPY_BYTES(16)')
    if mutation=='wrong-stride':
        assert 'source += sstride' in candidate
        candidate=candidate.replace('source += sstride','source += 16u')
    if mutation=='wide-source':
        assert 'uint32_t v, source;' in candidate
        candidate=candidate.replace('uint32_t v, source;','uint32_t v; uintptr_t source;')
    reference=(ROOT/'tests/push_gather_reference.inc').read_text(encoding='utf-8')
    code=(ROOT/'tests/push_gather.c').read_text(encoding='utf-8').replace('/* FORMAT */',fmt).replace('/* REFERENCE */',reference).replace('/* CANDIDATE */',candidate).replace('out = calloc(', 'out = tracked_calloc(')
    source=tmp_path/'gather.c';source.write_text(code,encoding='utf-8')
    compiler,env=native_compiler(tmp_path);exe=tmp_path/('gather.exe' if os.name=='nt' else 'gather')
    command=([compiler,'/nologo','/std:c11','/O2',str(source),'/Fe:'+str(exe)] if os.name=='nt' else [compiler,'-std=c11','-O2',str(source),'-o',str(exe)])
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation=='none':assert result.returncode==0 and 'PASS:' in result.stdout,result.stdout+result.stderr
    else:assert result.returncode!=0,result.stdout+result.stderr
