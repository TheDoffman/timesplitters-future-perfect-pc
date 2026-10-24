"""Exercise the real Windows crash adapter/helper without proprietary game data."""
import json
from pathlib import Path
import subprocess
import sys
import os
import time

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture
def crash_client(tmp_path):
    if sys.platform != 'win32': pytest.skip('Windows crash adapter')
    compiler, environment = native_compiler(tmp_path)
    driver = tmp_path / 'driver.c'
    driver.write_text('#include "pc_crash.h"\n'
                      'int main(int argc,char **argv) { if (!pc_crash_init(0)) return 4; '
                      'pc_capture_gameplay(4,4,0,0,0,0); pc_capture_gameplay(4,4,0,1,2,0); '
                      'if(argc>1) { pc_crash_watchdog_start(); Sleep(15000); return 0; } '
                      'return pc_crash_test_command("--tsfp-crash-test=concurrent"); }\n')
    for sources, name, libraries in [
            ([ROOT/'tools/crash_helper.c'], 'tsfp_crash_helper.exe', ['dbghelp.lib', 'bcrypt.lib', 'user32.lib']),
            ([driver, ROOT/'src/pc_crash.c'], 'driver.exe', ['user32.lib'])]:
        subprocess.run([compiler, '/nologo', '/std:c11', '/O2', '/MD', '/Zi',
                        '/I'+str(ROOT/'src'), *map(str, sources), '/Fe:'+str(tmp_path/name),
                        '/link', '/DEBUG', *libraries], check=True, cwd=tmp_path, env=environment)
    environment = {key: value for key, value in environment.items()
                   if not key.startswith(('TSFP_', 'RECOMP_'))}
    return tmp_path,environment

def test_simultaneous_unhandled_exceptions_wait_for_dump(crash_client):
    tmp_path,environment=crash_client
    environment['TSFP_CAPTURE_SESSION']='a'*32
    for iteration in range(3):
        area = tmp_path / str(iteration)
        environment['TSFP_CRASH_DIR'] = str(area)
        crashed = subprocess.run([str(tmp_path/'driver.exe')], cwd=tmp_path,
                                 env=environment, capture_output=True, timeout=20)
        assert crashed.returncode == 0xC0000005
        reports = list(area.glob('crash-*/report.json'))
        assert len(reports) == 1, 'Concurrent faults must leave one completed report'
        health=json.loads((area/'capture-health.json').read_text())
        assert health['session_id']=='a'*32 and health['pid']>0 and health['helper_pid']>0
        crumbs=(reports[0].parent/'breadcrumbs.txt').read_text()
        assert 'scene-id 4' in crumbs and 'paused 1' in crumbs and 'network-mode 2' in crumbs
        assert len(crumbs.encode())<8192
        report = json.loads(reports[0].read_text())
        assert report['dump_saved'], report
        assert report['exception_code'] == '0xC0000005'
        resolved = subprocess.run([str(tmp_path/'tsfp_crash_helper.exe'), '--resolve',
                                   str(reports[0].parent/'crash.dmp'), str(tmp_path/'driver.exe'),
                                   str(tmp_path), 'tsfp_crash_test_site'], capture_output=True,
                                  text=True, timeout=20)
        assert resolved.returncode == 0, resolved.stdout + resolved.stderr


def test_helper_heartbeat_and_death(crash_client):
    tmp_path,env=crash_client
    area=tmp_path/'health';env.update(TSFP_CRASH_DIR=str(area),TSFP_CAPTURE_SESSION='b'*32)
    child=subprocess.Popen([str(tmp_path/'driver.exe'),'health'],env=env,cwd=tmp_path)
    try:
        path=area/'capture-health.json';deadline=time.monotonic()+8
        while time.monotonic()<deadline:
            if path.exists():
                record=json.loads(path.read_text())
                if record['armed']:break
            time.sleep(.05)
        else:pytest.fail('No armed helper heartbeat')
        assert record['pid']==child.pid and record['session_id']=='b'*32
        first=path.stat().st_mtime_ns
        while path.stat().st_mtime_ns==first and time.monotonic()<deadline:time.sleep(.05)
        assert path.stat().st_mtime_ns>first
        # Terminate only the helper explicitly identified by our isolated child.
        import ctypes
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.OpenProcess.restype=ctypes.c_void_p
        kernel.TerminateProcess.argtypes=[ctypes.c_void_p,ctypes.c_uint]
        kernel.CloseHandle.argtypes=[ctypes.c_void_p]
        handle=kernel.OpenProcess(1,False,record['helper_pid']);assert handle
        try:assert kernel.TerminateProcess(handle,9)
        finally:kernel.CloseHandle(handle)
        time.sleep(.2);last=path.stat().st_mtime_ns;time.sleep(2.2)
        assert child.poll() is None and path.stat().st_mtime_ns==last
    finally:
        if child.poll() is None:child.terminate()
        child.wait(timeout=10)

def test_breadcrumb_ring(tmp_path):
    compiler,env=native_compiler(tmp_path);exe=tmp_path/'breadcrumbs.exe'
    args=([compiler,'/nologo','/std:c11','/W3','/I'+str(ROOT/'src'),str(ROOT/'tests/breadcrumbs.c'),'/Fe:'+str(exe)]
          if sys.platform=='win32' else [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(ROOT/'tests/breadcrumbs.c'),'-o',str(exe)])
    subprocess.run(args,cwd=tmp_path,env=env,check=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.parametrize('failure',['missing_helper','blocked_storage','invalid_session'])
def test_unavailable_capture_never_leaves_healthy_handshake(crash_client,failure):
    root,env=crash_client;area=root/'unavailable'
    env.update(TSFP_CRASH_DIR=str(area),TSFP_CAPTURE_SESSION='d'*32)
    if failure=='missing_helper':(root/'tsfp_crash_helper.exe').rename(root/'helper-disabled.exe')
    if failure=='blocked_storage':area.write_text('blocked')
    if failure=='invalid_session':env['TSFP_CAPTURE_SESSION']='not-a-valid-session'
    result=subprocess.run([str(root/'driver.exe')],env=env,cwd=root,capture_output=True,timeout=20)
    assert result.returncode==(4 if failure=='missing_helper' else 0xC0000005)
    assert not (area/'capture-health.json').exists()
