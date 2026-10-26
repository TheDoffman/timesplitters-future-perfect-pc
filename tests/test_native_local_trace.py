"""Exercise the shared local trace, its absent/failed paths and finite limits."""
import csv
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]

@pytest.fixture(scope='module')
def binaries(tmp_path_factory):
    folder=tmp_path_factory.mktemp('local-trace')
    compiler,env=native_compiler(folder)
    original=(ROOT/'src/pc_native_local_trace.h').read_text()
    mutations={'none':original,
        'caller':original.replace('caller!=0x0003d3ebu','caller!=0x0003d455u'),
        'stale':original.replace('a.time=pc_native_local_float(read,a.model+0xdcu);','a.time=0;'),
        'overrun':original.replace('trace->rows<PC_NATIVE_LOCAL_LIMIT','trace->rows<=PC_NATIVE_LOCAL_LIMIT')}
    outputs={}
    for name,body in mutations.items():
        if name!='none':assert body!=original
        header=folder/(name+'.h');header.write_text(body)
        source=folder/(name+'.c')
        source.write_text((ROOT/'tests/native_local_trace.c').read_text().replace('../src/pc_native_local_trace.h',header.as_posix()))
        output=folder/(name+('.exe' if os.name=='nt' else ''))
        args=([compiler,'/nologo','/O2','/W4','/WX','/std:c11',str(source),'/Fe:'+str(output),'/Fo:'+str(folder/(name+'.obj'))] if os.name=='nt'
              else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',str(source),'-lm','-o',str(output)])
        r=subprocess.run(args,cwd=folder,env=env,capture_output=True,text=True,timeout=60)
        assert r.returncode==0,r.stdout+r.stderr
        outputs[name]=output
    return outputs

@pytest.mark.parametrize('mode',['disabled','failure','rows','limit','bytes'])
def test_local_trace(binaries,tmp_path,mode):
    path=tmp_path/'trace.csv';env={k:v for k,v in os.environ.items() if k!='TSFP_NATIVE_LOCAL_TRACE'}
    if mode!='disabled':env['TSFP_NATIVE_LOCAL_TRACE']=str(path if mode!='failure' else tmp_path/'absent'/'trace.csv')
    r=subprocess.run([str(binaries['none']),mode],env=env,capture_output=True,text=True,timeout=15)
    assert r.returncode==0,r.stdout+r.stderr
    if mode in ('disabled','failure'):assert not path.exists();return
    rows=list(csv.DictReader(path.open()))
    assert len(rows)=={'rows':2,'limit':20000,'bytes':0}[mode]
    assert path.stat().st_size<=8*1024*1024
    assert all(None not in row and len(row)==50 for row in rows)
    if mode=='rows':
        assert [row['hand_time'] for row in rows]==['2.25','2.5']
        assert [row['frame'] for row in rows]==['123','124']
        assert all(row['game_ticks']=='0' and row['valid']=='15' and row['dir_z']=='2.25' for row in rows)

@pytest.mark.parametrize('name,mode',[('caller','rows'),('stale','rows'),('overrun','limit')])
def test_local_trace_mutations(binaries,tmp_path,name,mode):
    env=dict(os.environ,TSFP_NATIVE_LOCAL_TRACE=str(tmp_path/'trace.csv'))
    r=subprocess.run([str(binaries[name]),mode],env=env,capture_output=True,text=True,timeout=15)
    assert r.returncode!=0,'The independent trace mutation escaped its check'
