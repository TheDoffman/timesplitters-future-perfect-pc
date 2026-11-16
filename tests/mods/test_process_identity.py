"""Actual native creation stamps, read-only PID-reuse decisions and exit evidence."""
import contextlib
import json
import os
import queue
import subprocess
import sys
import threading

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


@pytest.mark.skipif(os.name!='nt',reason='Windows borrowed-handle identity')
def test_windows_owned_handle_snapshot_does_not_require_reopening_pid(process_tools):
    record=call(process_tools,'owned')
    assert record['state']=='alive' and record['valid'] and record['kind']==1


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
