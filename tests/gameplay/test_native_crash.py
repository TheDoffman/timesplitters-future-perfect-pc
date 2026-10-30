"""Real fatal-signal/helper and nonfatal watchdog tests with disposable reports."""


import json
import os
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
pytestmark=pytest.mark.skipif(sys.platform!='darwin',reason='Darwin crash adapter')
@pytest.fixture(scope='module')
def client(tmp_path_factory):
    exe=tmp_path_factory.mktemp('native-crash')/'capture'
    subprocess.run(['clang','-std=c11','-g','-O0','-Wall','-Wextra','-Werror','-DTSFP_CRASH_TEST','-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/diagnostics'),str(ROOT/'tests/gameplay/native_crash.c'),str(ROOT/'src/native/crash_posix.c'),'-o',str(exe)],check=True)
    subprocess.run(['dsymutil',str(exe)],check=True)
    return exe
@pytest.mark.parametrize('mode,count',[('crash',1),('hang',1),('twice',2),('then-crash',2),('startup',0),('unarmed',0),('background',0),('modal',0),('responsive',0)])
def test_capture(client,tmp_path,mode,count):
    log=tmp_path/'game.err';log.write_text('local diagnostic tail\n')
    env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(tmp_path/'reports'),RECOMP_LOG_FILE=str(log))
    result=subprocess.run([str(client),mode],env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==(134 if 'crash' in mode else 0),result.stderr
    reports=sorted((tmp_path/'reports').glob('*/report.json'));assert len(reports)==count
    for path in reports:
        crumbs=(path.parent/'breadcrumbs.txt').read_text();assert 'scene-id 4' in crumbs and len(crumbs)<8192
        health=json.loads((tmp_path/'reports/capture-health.json').read_text());assert health['session_id']=='c'*32
        allocations=(path.parent/'allocation-history.txt').read_text()
        assert 'allocation-event 257' in allocations and 'allocation-bytes 4096' in allocations
        models=(path.parent/'model-history.txt').read_text();assert 'node-index 15' in models and 'traversal-depth 123' in models
        details=json.loads((path.parent/'diagnostics.json').read_text());assert details['root_cause']=='undetermined'
        data=json.loads(path.read_text());assert data['schema']==1 and len(data['executable_sha256'])==64
        assert data['native_capture_saved'];assert (path.parent/'game-log-tail.txt').read_text()=='local diagnostic tail\n'
        stack=(path.parent/'native-stack.txt').read_text();assert len(stack)<=262144
        if data['kind']=='unhandled_exception':assert '12340000' in stack and 'tsfp_native_crash_test_site' in stack,stack
        else:assert 'Call graph:' in stack or 'Sampling process' in stack,stack

def test_refuses_linked_report_root(client,tmp_path):
    outside=tmp_path/'outside';outside.mkdir();link=tmp_path/'reports';link.symlink_to(outside,target_is_directory=True)
    result=subprocess.run([str(client),'crash'],env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(link)),timeout=15)
    assert result.returncode==134 and not list(outside.iterdir())
