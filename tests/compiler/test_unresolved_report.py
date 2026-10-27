"""Original unresolved-address diagnostics, without captured game layouts."""

from project_paths import source_file as repo_file

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import unresolved_report as report
from recomp_image import load_image
from test_xbe_image import fixture


def inputs(tmp_path):
    xbe, functions, symbols, output = (tmp_path/name for name in
        ('sample.xbe','functions.json','unresolved.txt','report.json'))
    xbe.write_bytes(fixture())
    functions.write_text('[]')
    symbols.write_text('')
    return xbe, functions, symbols, output


def command(paths):
    xbe, functions, symbols, output = paths
    return [str(xbe),'--functions',str(functions),'--unresolved',str(symbols),'--out',str(output)]


def test_empty_inputs_are_reportable_and_output_is_idempotent(tmp_path):
    paths=inputs(tmp_path)
    assert report.main(command(paths))==0
    data=json.loads(paths[-1].read_text())
    assert data['observations']==[] and data['counts']=={}
    assert data['executable_sha256']==hashlib.sha256(paths[0].read_bytes()).hexdigest()
    before=paths[-1].stat().st_mtime_ns
    assert report.main(command(paths))==0
    assert paths[-1].stat().st_mtime_ns==before


def test_real_section_header_zero_fill_and_unmapped_are_distinct(tmp_path):
    paths=inputs(tmp_path)
    image=load_image(paths[0])
    rows=report.describe(image,report.FunctionMap([]),[0x10000,0x11000,0x11200,0x11300])
    assert [row['mapping'] for row in rows]==['header','section','section','unmapped']
    assert [row['file_backed'] for row in rows]==[True,True,False,False]
    assert rows[1]['section']=='.text' and rows[1]['section_executable'] is True
    assert all(row['relation']=='outside_recorded_bodies' for row in rows)
    assert all('estimated_end' not in row for row in rows)


def test_all_overlapping_bodies_and_aliases_are_reported(tmp_path):
    image=load_image(inputs(tmp_path)[0])
    records=[dict(start=0x11000,end=0x11200,name='outer')]
    records += [dict(start=0x11010+8*n,size=4) for n in range(20)]
    records += [dict(start=0x11001,end=0x11200,name='overlap'),
                dict(start=0x11100,size=0,name='alias')]
    rows=report.describe(image,report.FunctionMap(records),[0x110ff,0x11100,0x11200])
    assert rows[0]['relation']=='inside_recorded_body'
    assert len(rows[0]['containing_bodies'])==2
    assert rows[1]['relation']=='known_entry' and len(rows[1]['containing_bodies'])==2
    assert rows[2]['relation']=='recorded_end' and len(rows[2]['ending_bodies'])==2


@pytest.mark.parametrize('bad',['sub_123456789','sub_bad_12','-1','oops','0x100000000'])
def test_invalid_symbols_fail_with_line_number(bad):
    with pytest.raises(ValueError,match='line 3'):
        report.read_symbols('# heading\n\n'+bad)


def test_symbol_normalization_and_deduplication():
    assert report.read_symbols('sub_00001000\n0x1000\n1000\n0X2000')==[0x1000,0x2000]


@pytest.mark.parametrize('bad',[{},[dict(start=1,end=0)],[dict(start=True,size=1)],
    [dict(start=1,size=-1)],[dict(start=1,size=True)],[dict(start=1,end=2,name=3)]])
def test_invalid_metadata_rejected(bad):
    with pytest.raises((ValueError,TypeError)):
        report.FunctionMap(bad)


def test_failure_and_output_collisions_preserve_existing_files(tmp_path):
    paths=inputs(tmp_path)
    paths[-1].write_text('previous')
    paths[2].write_text('invalid!')
    assert report.main(command(paths))==1
    assert paths[-1].read_text()=='previous'
    original=paths[0].read_bytes()
    assert report.main(command(paths)[:-1]+[str(paths[0])])==1
    assert paths[0].read_bytes()==original


def test_copied_cli_runs_without_toolkit_or_capstone(tmp_path):
    paths=inputs(tmp_path)
    for name in ('unresolved_report.py','branch_feedback.py','generation_output.py',
                 'recomp_image.py','xbe_image.py','xbe_kernel_exports.def'):
        shutil.copyfile(repo_file(ROOT, 'tools', name),tmp_path/name)
    script='''import runpy,sys
sys.path.insert(0,sys.argv.pop(1))
sys.modules['capstone']=None
runpy.run_module('unresolved_report',run_name='__main__')
'''
    result=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path),*command(paths)],
                          cwd=tmp_path,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
    assert paths[-1].exists()


def test_production_report_works_as_module_and_direct_script(tmp_path):
    runtime=ROOT/'external/ts3FPrecomp'
    entry=runtime/'tools/recomp/analyze_unresolved.py'
    if not entry.is_file():
        pytest.skip('Pinned compatibility entry unavailable')
    paths=inputs(tmp_path)
    for prefix in (['-m','tools.recomp.analyze_unresolved'],[str(entry)]):
        result=subprocess.run([sys.executable,*prefix,*command(paths)],cwd=runtime,
                              env=dict(os.environ,TSFP_PROJECT_ROOT=str(ROOT)),
                              capture_output=True,text=True,timeout=30)
        assert result.returncode==0,result.stdout+result.stderr
    assert json.loads(paths[-1].read_text())['observations']==[]
