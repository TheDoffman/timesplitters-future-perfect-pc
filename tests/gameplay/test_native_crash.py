"""Real fatal-signal/helper and nonfatal watchdog tests with disposable reports."""


import json
import os
from pathlib import Path
import subprocess
import sys
import pytest
ROOT=Path(__file__).resolve().parents[2]
FATAL_STATUS=132 if sys.platform=='linux' else 134
pytestmark=pytest.mark.skipif(sys.platform not in ('darwin','linux'),reason='POSIX crash adapters')
@pytest.fixture(scope='module')
def client(tmp_path_factory):
    exe=tmp_path_factory.mktemp('native-crash')/'capture'
    flags=['-D_GNU_SOURCE','-no-pie','-lcrypto','-ldl'] if sys.platform=='linux' else []
    subprocess.run(['clang','-std=c11','-g','-O0','-Wall','-Wextra','-Werror','-DTSFP_CRASH_TEST','-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/diagnostics'),str(ROOT/'tests/gameplay/native_crash.c'),str(ROOT/'src/native/crash_posix.c'),'-o',str(exe),*flags],check=True)
    if sys.platform=='darwin':subprocess.run(['dsymutil',str(exe)],check=True)
    return exe
@pytest.mark.parametrize('mode,count',[('crash',1),('hang',1),('twice',2),('then-crash',2),('startup',0),('unarmed',0),('background',0),('modal',0),('responsive',0)])
def test_capture(client,tmp_path,mode,count):
    log=tmp_path/'game.err';log.write_text('local diagnostic tail\n')
    env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(tmp_path/'reports'),RECOMP_LOG_FILE=str(log))
    result=subprocess.run([str(client),mode],env=env,capture_output=True,text=True,timeout=15)
    assert result.returncode==(FATAL_STATUS if 'crash' in mode else 0),result.stderr
    reports=sorted((tmp_path/'reports').glob('*/report.json'));assert len(reports)==count
    for path in reports:
        crumbs=(path.parent/'breadcrumbs.txt').read_text();assert 'scene-id 4' in crumbs and len(crumbs)<8192
        health=json.loads((tmp_path/'reports/capture-health.json').read_text());assert health['session_id']=='c'*32
        allocations=(path.parent/'allocation-history.txt').read_text()
        assert 'allocation-event 257' in allocations and 'allocation-bytes 4096' in allocations
        models=(path.parent/'model-history.txt').read_text();assert 'node-index 15' in models and 'traversal-depth 123' in models
        network=(path.parent/'network-history.txt').read_text();assert 'bot-accepted 12' in network and 'rtt-ms 180' in network and len(network)<8192
        details=json.loads((path.parent/'diagnostics.json').read_text());assert details['root_cause']=='undetermined'
        data=json.loads(path.read_text());assert data['schema']==1 and len(data['executable_sha256'])==64
        assert data['native_capture_saved'];assert (path.parent/'game-log-tail.txt').read_text()=='local diagnostic tail\n'
        stack=(path.parent/'native-stack.txt').read_text();assert len(stack)<=262144
        if data['kind']=='unhandled_exception':assert '12340000' in stack and 'tsfp_native_crash_test_site' in stack,stack
        elif sys.platform=='linux':assert 'Live stack sampling unavailable' in stack,stack
        else:assert 'Call graph:' in stack or 'Sampling process' in stack,stack

def test_refuses_linked_report_root(client,tmp_path):
    outside=tmp_path/'outside';outside.mkdir();link=tmp_path/'reports';link.symlink_to(outside,target_is_directory=True)
    result=subprocess.run([str(client),'crash'],env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(link)),timeout=15)
    assert result.returncode==FATAL_STATUS and not list(outside.iterdir())


def test_library_loaded_after_startup_is_the_fault_image(client,tmp_path):
    directory=tmp_path/'shared runtime Ω';directory.mkdir()
    library=directory/('test-runtime.dylib' if sys.platform=='darwin' else 'test-runtime.so')
    obj=directory/'library.o'
    subprocess.run(['clang','-g','-O0','-fPIC','-c',str(ROOT/'tests/gameplay/native_crash_library.c'),'-o',str(obj)],check=True)
    flags=['-dynamiclib'] if sys.platform=='darwin' else ['-shared']
    subprocess.run(['clang',*flags,str(obj),'-o',str(library)],check=True)
    if sys.platform=='darwin':subprocess.run(['dsymutil',str(library)],check=True)
    result=subprocess.run([str(client),'crash-library',str(library)],env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(tmp_path/'reports')),capture_output=True,text=True,timeout=15)
    assert result.returncode==(133 if sys.platform=='darwin' else 132),result.stderr
    reports=list((tmp_path/'reports').glob('*/report.json'));assert len(reports)==1
    stack=(reports[0].parent/'native-stack.txt').read_text()
    assert 'PC image: '+str(library) in stack and 'tsfp_native_library_fault' in stack,stack
    assert len(stack)<=262144


@pytest.mark.skipif(sys.platform!='linux',reason='ELF PIE address normalization')
def test_pie_main_fault_uses_elf_load_bias(tmp_path):
    exe=tmp_path/'pie-capture'
    subprocess.run(['clang','-std=c11','-g','-O0','-fPIE','-pie','-D_GNU_SOURCE','-DTSFP_CRASH_TEST',
        '-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/diagnostics'),str(ROOT/'tests/gameplay/native_crash.c'),
        str(ROOT/'src/native/crash_posix.c'),'-lcrypto','-ldl','-o',str(exe)],check=True)
    result=subprocess.run([str(exe),'crash'],env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(tmp_path/'reports')),capture_output=True,text=True,timeout=15)
    assert result.returncode==132,result.stderr
    reports=list((tmp_path/'reports').glob('*/report.json'));assert len(reports)==1
    stack=(reports[0].parent/'native-stack.txt').read_text()
    assert 'PC image: '+str(exe) in stack and 'tsfp_native_crash_test_site' in stack,stack
def test_nonfatal_network_export(client,tmp_path):
    area=tmp_path/'reports'
    result=subprocess.run([str(client),'responsive'],env=dict(os.environ,TSFP_CAPTURE_SESSION='c'*32,TSFP_CRASH_DIR=str(area)),capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stderr
    import time
    deadline=time.monotonic()+3
    while not (area/'network-sync.txt').exists() and time.monotonic()<deadline:time.sleep(.05)
    text=(area/'network-sync.txt').read_text()
    assert 'bot-accepted 12' in text and 'rtt-ms 180' in text and len(text)<8192
