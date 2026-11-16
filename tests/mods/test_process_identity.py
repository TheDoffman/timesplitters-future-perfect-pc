"""Actual native creation stamps, read-only PID-reuse decisions and exit evidence."""
import contextlib
import json
import os
import queue
import subprocess
import sys
import threading
import shutil
from pathlib import Path

import pytest

from test_script_process import process_tools


def call(tools,*args):
    driver=tools['worker'].with_name('tsfp-mod-process-identity-driver'+tools['worker'].suffix)
    result=subprocess.run([str(driver),*map(str,args)],capture_output=True,timeout=5)
    assert result.returncode==0 and not result.stderr,(result.returncode,result.stdout,result.stderr)
    return json.loads(result.stdout)


def state(tools,record):return call(tools,'state',*(record[key] for key in ('kind','pid','start_a','start_b')))


@contextlib.contextmanager
def held_python(setup=''):
    program=setup+'\nimport sys\nsys.stdout.buffer.write(b"ready\\n")\nsys.stdout.buffer.flush()\nsys.stdin.buffer.read(1)\n'
    child=subprocess.Popen([sys.executable,'-c',program],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    ready=queue.Queue();reader=threading.Thread(target=lambda:ready.put(child.stdout.readline()),daemon=True);reader.start()
    try:
        assert ready.get(timeout=5)==b'ready\n'
        yield child
    finally:
        try:child.communicate(b'x' if child.poll() is None else None,timeout=5)
        except subprocess.TimeoutExpired:child.kill();child.communicate(timeout=5)


def test_actual_held_process_has_repeatable_birth_and_exits_without_signalling_it(process_tools):
    with held_python() as child:
        first=call(process_tools,'read',child.pid);second=call(process_tools,'read',child.pid)
        assert first==second and first['valid'] and first['state']=='alive'
        assert state(process_tools,first)['state']=='alive' and child.poll() is None
        child.communicate(b'x',timeout=5);assert child.returncode==0
    assert state(process_tools,first)['state']=='exited'


def test_same_pid_with_different_birth_refers_to_an_exited_identity_and_preserves_live_task(process_tools):
    with held_python() as child:
        record=call(process_tools,'read',child.pid);record['start_a']+=1
        assert state(process_tools,record)['state']=='exited'
        assert child.poll() is None and call(process_tools,'read',child.pid)['state']=='alive'


@pytest.mark.parametrize('change',[{'kind':0},{'kind':4294967295},{'pid':0},{'pid':18446744073709551615},
                                  {'start_a':0},{'start_b':1000000}])
def test_invalid_and_foreign_records_are_unknown_not_death(process_tools,change):
    with held_python() as child:
        record=call(process_tools,'read',child.pid);record.update(change)
        assert state(process_tools,record)['state']=='unknown' and child.poll() is None


def test_another_platform_stamp_cannot_authorize_cleanup(process_tools):
    with held_python() as child:
        record=call(process_tools,'read',child.pid);record['kind']=record['kind']%3+1
        assert state(process_tools,record)['state']=='unknown' and child.poll() is None


def test_native_driver_reports_its_own_stamp(process_tools):
    record=call(process_tools,'self')
    assert record['state']=='alive' and record['valid'] and record['pid']>0 and record['start_a']>0


def test_owned_startup_stamps_survive_termination_without_reopening_initial_pid(process_tools,tmp_path):
    arguments=[str(process_tools['lifetime']),str(process_tools['probe']),str(tmp_path),'--stamps']
    parent=subprocess.Popen(arguments,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    ready=queue.Queue();threading.Thread(target=lambda:ready.put(parent.stdout.readline()),daemon=True).start()
    try:
        record=json.loads(ready.get(timeout=5));expected=2 if sys.platform=='darwin' else 1
        assert record['count']==len(record['stamps'])==expected
        assert len({stamp['pid'] for stamp in record['stamps']})==expected
        for stamp in record['stamps']:
            assert stamp['pid'] not in (parent.pid,os.getpid()) and stamp['start_a']>0
            assert state(process_tools,stamp)['state'] in ('alive','unknown')
        out,err=parent.communicate(b'\n',timeout=6)
        assert parent.returncode==0 and not err and json.loads(out)['retained'],(parent.returncode,out,err)
        for stamp in record['stamps']:assert state(process_tools,stamp)['state']=='exited'
        assert not list(tmp_path.iterdir())
    finally:
        if parent.poll() is None:parent.kill();parent.communicate(timeout=5)


@pytest.mark.skipif(sys.platform!='darwin',reason='Darwin private guardian control wire')
@pytest.mark.parametrize('payload',[b'\x01'*8,b'\x00'*72])
def test_legacy_or_invalid_guardian_birth_message_is_refused(process_tools,tmp_path,payload):
    source=tmp_path/'source';source.mkdir();cache=tmp_path/'cache';cache.mkdir()
    image=source/'worker';shutil.copy2(process_tools['probe'],image)
    guardian=source/'tsfp-mod-guardian'
    guardian.write_text('#!'+sys.executable+'\nimport os\nos.write(4,'+repr(payload)+')\nos.close(4)\n');guardian.chmod(0o500)
    result=subprocess.run([str(process_tools['lifetime']),str(image),str(cache)],stdin=subprocess.DEVNULL,capture_output=True,timeout=6)
    assert result.returncode==125 and b'Lifetime guardian startup failed' in result.stderr
    assert not list(cache.iterdir())


@pytest.mark.skipif(os.name!='nt',reason='Windows borrowed-handle identity')
def test_windows_owned_handle_snapshot_does_not_require_reopening_pid(process_tools):
    record=call(process_tools,'owned')
    assert record['state']=='alive' and record['valid'] and record['kind']==1


@pytest.mark.skipif(os.name!='nt',reason='Windows owned process query-denial control')
def test_denied_query_preserves_record_as_unknown_while_original_process_is_alive(process_tools):
    import ctypes
    security=ctypes.WinDLL('advapi32',use_last_error=True);kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    security.ConvertStringSecurityDescriptorToSecurityDescriptorW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint,ctypes.POINTER(ctypes.c_void_p),ctypes.c_void_p]
    security.ConvertStringSecurityDescriptorToSecurityDescriptorW.restype=ctypes.c_int
    security.SetKernelObjectSecurity.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_void_p]
    security.SetKernelObjectSecurity.restype=ctypes.c_int
    kernel.LocalFree.argtypes=[ctypes.c_void_p];kernel.LocalFree.restype=ctypes.c_void_p
    with held_python() as child:
        record=call(process_tools,'read',child.pid);assert record['state']=='alive'
        descriptor=ctypes.c_void_p()
        # Only this newly-created fixture process changes. Its original owned
        # handle retains exit/control rights; no account/file ACL is modified.
        assert security.ConvertStringSecurityDescriptorToSecurityDescriptorW('D:(D;;0x00101000;;;WD)',1,ctypes.byref(descriptor),None)
        try:assert security.SetKernelObjectSecurity(ctypes.c_void_p(int(child._handle)),4,descriptor),ctypes.get_last_error()
        finally:kernel.LocalFree(descriptor)
        assert call(process_tools,'read',child.pid)['state']=='unknown'
        assert state(process_tools,record)['state']=='unknown' and child.poll() is None


@pytest.mark.skipif(sys.platform!='linux',reason='Actual Linux proc comm parser')
def test_linux_process_name_with_parentheses_and_newline_cannot_shift_birth_field(process_tools):
    setup='import ctypes\nlib=ctypes.CDLL(None)\nassert lib.prctl(15,ctypes.c_char_p(b"odd ) (\\nname"),0,0,0)==0\n'
    with held_python(setup) as child:
        record=call(process_tools,'read',child.pid)
        assert record['state']=='alive' and record['kind']==2 and state(process_tools,record)['state']=='alive'


@pytest.mark.skipif(sys.platform!='linux',reason='Actual waitid WNOWAIT zombie')
def test_linux_exited_unreaped_task_is_not_mistaken_for_live_process(process_tools):
    program='import os,sys\npid=os.fork()\nif pid==0:os._exit(0)\nos.waitid(os.P_PID,pid,os.WEXITED|os.WNOWAIT)\nprint(pid,flush=True)\nsys.stdin.buffer.read(1)\nos.waitpid(pid,0)\n'
    parent=subprocess.Popen([sys.executable,'-c',program],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        ready=queue.Queue();threading.Thread(target=lambda:ready.put(parent.stdout.readline()),daemon=True).start()
        pid=int(ready.get(timeout=5));record=call(process_tools,'read',pid)
        assert record['state']=='exited' and record['valid'] and state(process_tools,record)['state']=='exited'
    finally:
        parent.communicate(b'x',timeout=5)
