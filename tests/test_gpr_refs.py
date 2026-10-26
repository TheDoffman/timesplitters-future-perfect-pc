"""Generated register references must remain live, aliased and thread-local."""
import os
from pathlib import Path
import struct
import subprocess

import pytest
from native_compiler import native_compiler
from test_recomp_float_joins import ROOT,RUNTIME,config,FunctionTranslator


@pytest.fixture(scope='module')
def gpr_runs(tmp_path_factory):
    prior=dict(vars(config))
    try:
        parent=bytes.fromhex('8b742404 8b06 8b4c2408 894e04 31c8 50 56')
        parent+=b'\xe8'+struct.pack('<i',0x1100-(0x1000+len(parent)+5))
        parent+=bytes.fromhex('83c408 0306 334e04 894608 894e0c c3')
        child=bytes.fromhex('8b442404 8b542408 8910 8b08 31d1 83c103 83c001 6683c207 b39a 66bf55aa 0fb6d3 c3')
        image=parent+b'\x90'*(0x100-len(parent))+child
        config._install([config.Section('.text',0x1000,len(image),0,len(image),True)],0x1000,0x1000+len(image),'gpr-test')
        info={0x1000:{'name':'parent','end':0x1000+len(parent)},0x1100:{'name':'child','end':0x1100+len(child)}}
        translator=FunctionTranslator(image,info,trace_all_entries=True)
        generated='\n'.join(translator.translate_function(a,info[a]) for a in info)
        for a in info:
            block=generated[generated.index('void '+info[a]['name']):]
            assert block.index('RECOMP_TRACE_ENTER')<block.index('RECOMP_DECLARE_GPRS')<block.index(f'loc_{a:08X}:')
    finally:
        vars(config).clear();vars(config).update(prior)
    folder=tmp_path_factory.mktemp('gpr-refs');compiler,env=native_compiler(folder)
    template=(RUNTIME/'templates/runtime/recomp_types.h').read_text(encoding='utf-8')
    results={}
    for mode in range(7):
        target=folder/str(mode);target.mkdir()
        header=template
        if mode==6:
            old='uint32_t *const _recomp_eax=&g_eax;'
            assert old in header
            header=header.replace(old,'uint32_t *const _recomp_eax=bad_cell(&g_eax);')
        (target/'recomp_types.h').write_text(header)
        source=target/'refs.c';source.write_text((ROOT/'tests/gpr_refs.c').read_text().replace('/* GENERATED_FUNCTIONS */',generated))
        exe=target/('refs.exe' if os.name=='nt' else 'refs')
        include=[target,RUNTIME/'templates/runtime']
        command=([compiler,'/nologo','/std:c11','/O2','/DMODE='+str(mode),*('/I'+str(p) for p in include),str(source),'/Fe:'+str(exe)] if os.name=='nt'
                 # Generated basic blocks retain labels even when reached only
                 # by fall-through. Keep other warnings fatal in this fixture.
                 else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-label','-pthread','-DMODE='+str(mode),*('-I'+str(p) for p in include),str(source),'-lm','-o',str(exe)])
        built=subprocess.run(command,cwd=target,env=env,capture_output=True,text=True,timeout=60)
        assert built.returncode==0,built.stdout+built.stderr
        results[mode]=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    return results


def test_live_register_references(gpr_runs):
    for mode in range(6):
        result=gpr_runs[mode]
        assert result.returncode==0,result.stdout+result.stderr
        assert result.stdout==gpr_runs[0].stdout


def test_copied_register_value_is_rejected(gpr_runs):
    result=gpr_runs[6]
    assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr


def test_deployed_reference_scope(tmp_path):
    source=tmp_path/'scope.c'
    source.write_text('#include <assert.h>\n#define RECOMP_NUMERIC_POLICY_HEADER "'+
        (ROOT/'src/pc_native_numeric.h').as_posix()+'"\n#include "recomp_types.h"\n'+
        'int main(void) { assert(RECOMP_USE_GPR_REFS==1);\n'+
        'unsigned selected[]={0x18ea90,0x21450,0x214c0};\n'+
        'unsigned unchanged[]={0,0x18ea8f,0x18ea91,0x2144f,0x21451,0x214bf,0x214c1,0x151e80};\n'+
        'for(unsigned i=0;i<sizeof(selected)/sizeof(*selected);i++)assert(RECOMP_GPR_ANCHORS(selected[i]));\n'+
        'for(unsigned i=0;i<sizeof(unchanged)/sizeof(*unchanged);i++)assert(!RECOMP_GPR_ANCHORS(unchanged[i]));\nreturn 0;}\n')
    compiler,env=native_compiler(tmp_path);exe=tmp_path/('scope.exe' if os.name=='nt' else 'scope')
    includes=[RUNTIME/'templates/runtime',ROOT/'src']
    command=([compiler,'/nologo','/std:c11','/O2',*('/I'+str(p) for p in includes),str(source),'/Fe:'+str(exe)] if os.name=='nt'
             else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',*('-I'+str(p) for p in includes),str(source),'-lm','-o',str(exe)])
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr
