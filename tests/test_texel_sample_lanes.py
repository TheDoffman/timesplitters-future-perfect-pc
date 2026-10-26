"""The optimized texture survey must retain its exact original read footprint."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[1]

@pytest.mark.parametrize('mutation,fortified',[
    ('none',False),('none',True),('tail-lane',False),('overread',False),('redefine-copy',True)])
def test_texel_sample_lanes(tmp_path,mutation,fortified):
    text=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8_texture.c').read_text(encoding='utf-8')
    begin=text.index('static uint32_t texel_sample(')
    body=text[begin:text.index('\n/* Have this texture',begin)]
    assert 'words - i > 3 * step' in body
    if mutation=='tail-lane':
        body=body.replace('for (k = 0; i < words;', 'for (k = 1; i < words;')
    if mutation=='overread':
        body=body.replace('words - i > 3 * step','words - i >= 3 * step')
    # Instrument only the five production sample loads. Do not redefine libc's
    # memcpy macro: fortified platform headers may already define that name.
    assert body.count('memcpy(&w,')==5
    body=body.replace('memcpy(&w,','checked_copy(&w,')
    if mutation=='redefine-copy':body='#define memcpy checked_copy\n'+body
    source=tmp_path/'sample.c'
    source.write_text((ROOT/'tests/texel_sample_lanes.c').read_text().replace('/* PRODUCTION_SAMPLE */',body),encoding='utf-8')
    compiler,env=native_compiler(tmp_path)
    output=tmp_path/('sample.exe' if os.name=='nt' else 'sample')
    args=([compiler,'/nologo','/O2','/W4','/WX','/std:c11',str(source),'/Fe:'+str(output)] if os.name=='nt'
          else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',str(source),'-o',str(output)])
    if fortified:args.insert(1,('/D' if os.name=='nt' else '-D')+'TSFP_TEST_FORTIFIED_COPY=1')
    compiled=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    if mutation=='redefine-copy':
        assert compiled.returncode!=0 and 'memcpy' in compiled.stdout+compiled.stderr
        return
    assert compiled.returncode==0,compiled.stdout+compiled.stderr
    result=subprocess.run([str(output)],capture_output=True,text=True,timeout=30)
    if mutation=='none':assert result.returncode==0,result.stdout+result.stderr
    else:assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr
