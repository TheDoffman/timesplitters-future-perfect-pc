"""Compare complete production expansion against its frozen pre-change body."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('mutation',[None,'overread','read-only-overread','wrong-step','reverse-attributes','missing-fallback','tile-origin'])
def test_vertex_expand(tmp_path,mutation):
    text=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8.c').read_text(encoding='utf-8')
    start=text.index('struct shadow_program {')
    structure=text[start:text.index('\n};',start)+3]
    start=text.index('#define FF_MAX_INPUTS ')
    definitions=structure+'\n'+text[start:text.index('static unsigned long g_draws_ff_declaration;',start)]
    for signature in ('static int ff_read_floats(', 'static UINT ff_format_size('):
        start=text.index(signature);definitions+='\n'+text[start:text.index('\n}\n',start)+3]
    start=text.index('static uint8_t *shadow_expand_generic_vertices(')
    end=text.index('static uint8_t *shadow_expand_vertices(',start)
    end=text.index('\n}\n',end)+3
    candidate=text[start:end]
    helper_start=candidate.index('static void shadow_expand_program_vertices(')
    helper_end=candidate.index('\n}\n',helper_start)+3
    helper=candidate[helper_start:helper_end]
    if mutation=='overread':
        old='SHADOW_EXPAND_COPY_RAW(12)';new='SHADOW_EXPAND_COPY_RAW(16)'
    elif mutation=='read-only-overread':
        old='case 12: SHADOW_EXPAND_COPY_RAW(12); break;'
        new='''case 12:
            for (v=block_begin;v<block_end;v++) {
                volatile uint8_t extra=input[(size_t)v*step+12];(void)extra;
                memcpy(output+(size_t)v*out_stride,input+(size_t)v*step,12);
            }
            break;'''
    elif mutation=='wrong-step':
        old='input + (size_t)v * step';new='input + (size_t)v * (step + 1u)'
    elif mutation=='tile-origin':
        old='input + (size_t)v * step';new='input + (size_t)(v - block_begin) * step'
    elif mutation=='reverse-attributes':
        old='for (k = 0; k < p->packed_count; k++)';new='for (k = p->packed_count - 1; k >= 0; k--)'
    elif mutation=='missing-fallback':
        old='default: SHADOW_EXPAND_COPY_RAW(p->packed_size[k]); break;';new='default: break;'
    if mutation:
        assert old in helper
        candidate=candidate[:helper_start]+helper.replace(old,new)+candidate[helper_end:]
    candidate=candidate.replace('shadow_expand_vertices(', 'candidate(')
    reference=(ROOT/'tests/vertex_expand_reference.inc').read_text(encoding='utf-8')
    fixture=(ROOT/'tests/vertex_expand.c').read_text(encoding='utf-8')
    code=fixture.replace('/* DEFINITIONS */',definitions).replace('/* REFERENCE */',reference).replace('/* CANDIDATE */',candidate)
    code=code.replace('out = calloc(', 'out = tracked_calloc(')
    source=tmp_path/'expand.c';source.write_text(code,encoding='utf-8')
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('expand.exe' if os.name=='nt' else 'expand')
    command=([compiler,'/nologo','/std:c11','/O2','/W4',str(source),'/Fe:'+str(exe)]
             if os.name=='nt' else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',str(source),'-lm','-o',str(exe)])
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:
        assert result.returncode!=0,result.stdout+result.stderr
    else:
        assert result.returncode==0 and 'PASS:48000' in result.stdout,result.stdout+result.stderr
