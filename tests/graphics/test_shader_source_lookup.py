"""Exercise the actual HLE lookup; guest addresses never stand in for contents."""


import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('mutation', ['none','stale-source','eager-hash'])
def test_shader_source_lookup(tmp_path,mutation):
    actual=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8.c').read_text(encoding='utf-8')
    body=actual[actual.index('typedef struct SlotProgram {'):actual.index('static int slot_program_cached(DWORD host)')]
    assert 'slot_program_lookup(function, count, microcode, &hash)' in actual
    assert 'sp->source = function;' in actual
    body=body.replace('uint32_t h = 0x811C9DC5u;', 'uint32_t h = 0x811C9DC5u; ++hash_calls;')
    if mutation=='stale-source':
        old='g_slot_programs[i].source == source && g_slot_programs[i].count == count &&\n            host_vsh_same_microcode(g_slot_programs[i].host, microcode, count)'
        assert body.count(old)==1
        body=body.replace(old,'g_slot_programs[i].source == source && g_slot_programs[i].count == count')
    if mutation=='eager-hash':
        old='    int i;\n    for (i = 0; i < g_slot_program_count; i++)\n        if (g_slot_programs[i].source'
        assert body.count(old)==1
        body=body.replace(old,'    int i;\n    *hash = microcode_hash(microcode, count);\n    for (i = 0; i < g_slot_program_count; i++)\n        if (g_slot_programs[i].source')
    source=tmp_path/'lookup.c'
    source.write_text((ROOT/'tests/graphics/shader_source_lookup.c').read_text().replace('/* PRODUCTION_LOOKUP */',body),encoding='utf-8')
    compiler,env=native_compiler(tmp_path)
    output=tmp_path/('lookup.exe' if os.name=='nt' else 'lookup')
    args=([compiler,'/nologo','/W4','/std:c11',str(source),'/Fe:'+str(output)] if os.name=='nt'
          else [compiler,'-std=c11','-Wall','-Wextra','-Werror',str(source),'-o',str(output)])
    subprocess.run(args,cwd=tmp_path,env=env,check=True,capture_output=True,text=True,timeout=60)
    result=subprocess.run([str(output)],capture_output=True,text=True,timeout=10)
    if mutation=='none':assert result.returncode==0,result.stdout+result.stderr
    else:assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr
