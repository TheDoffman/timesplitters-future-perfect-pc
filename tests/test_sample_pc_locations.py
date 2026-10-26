"""Report exact diagnostic PCs without changing collection or hiding truncation."""
import os
from pathlib import Path
import subprocess

import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('mutation',[None,'hidden-overflow','prefix','disabled-work','private-path'])
def test_sample_pc_locations(tmp_path,mutation):
    text=(ROOT/'external/ts3FPrecomp/src/kernel/recomp_sample.c').read_text()
    start=text.index('#define SAMPLE_PC_MAX ')
    source=text[start:text.index('\n/* A chain folded',start)]
    if mutation=='hidden-overflow':
        source=source.replace('omitted += slot->count;','omitted += 0;')
    elif mutation=='prefix':
        source=source.replace('strcmp(resolved->name, s_pc_symbol)',
                              'strncmp(resolved->name, s_pc_symbol, strlen(s_pc_symbol))')
    elif mutation=='disabled-work':
        source=source.replace('if (!dump || !s_pc_symbol[0]) return;','/* mutation: disabled work */')
    elif mutation=='private-path':
        source=source.replace('file = at + 1;','; /* mutation: retain private path */')
    fixture=tmp_path/'sample_pc.c'
    fixture.write_text((ROOT/'tests/sample_pc_locations.c').read_text().replace('/* PRODUCTION_PC_REPORT */',source))
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('pc.exe' if os.name=='nt' else 'pc')
    command=([compiler,'/nologo','/std:c11','/O2','/W4',str(fixture),'/Fe:'+str(exe)] if os.name=='nt'
             else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',str(fixture),'-o',str(exe)])
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    if mutation:assert result.returncode!=0 and 'FAIL:' in result.stderr,result.stdout+result.stderr
    else:assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.skipif(os.name!='nt',reason='Real Win32 sampler and DbgHelp adapter')
def test_live_pc_symbol_and_pdb_lines(tmp_path):
    compiler,env=native_compiler(tmp_path)
    runtime=ROOT/'external/ts3FPrecomp/src'
    exe=tmp_path/'sample-pc-live.exe'
    command=[compiler,'/nologo','/std:c11','/O2','/Zi','/Fd:'+str(tmp_path/'compile.pdb'),
             '/I'+str(runtime/'kernel'),'/I'+str(runtime),
             str(ROOT/'tests/sample_pc_live.c'),str(runtime/'kernel/recomp_sample.c'),
             '/Fe:'+str(exe),'/link','/DEBUG:FULL','/INCREMENTAL:NO',
             '/PDB:'+str(tmp_path/'sample-pc-live.pdb'),'dbghelp.lib','psapi.lib']
    built=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    dump=tmp_path/'sample.txt'
    result=subprocess.run([str(exe),str(dump)],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr
    active=False;rows=[];coverage=None
    for line in dump.read_text().splitlines():
        if line.startswith('# thread '):active='guest main' in line
        if active and line.startswith('pc\t'):rows.append(line.split('\t'))
        if active and line.startswith('# pc coverage:'):coverage=line
    assert rows and coverage and 'omitted_samples=0' in coverage
    assert sum(int(row[1]) for row in rows)>0
    assert any(row[4]=='sample_pc_live.c' and int(row[5])>0 for row in rows),rows
    assert all('/' not in row[4] and '\\' not in row[4] for row in rows)
