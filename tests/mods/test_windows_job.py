"""Actual worker death kills descendants, retaining handles against PID reuse."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.skipif(os.name != 'nt', reason='Actual Windows job-object process lifetime')


@pytest.mark.parametrize('ending', ['terminate', 'exit', 'exception'])
def test_worker_death_reaps_child_without_inherited_job_handle(tmp_path, ending):
    ready, release = tmp_path / 'ready.json', tmp_path / 'release'
    script = r'''
import json, pathlib, subprocess, sys, time
sys.path.insert(0, sys.argv[1])
import mod_windows_job
mod_windows_job.attach()
child = subprocess.Popen([sys.executable, '-I', '-S', '-c', 'import time; time.sleep(45)'], close_fds=False)
pathlib.Path(sys.argv[2]).write_text(json.dumps(dict(pid=child.pid)))
while not pathlib.Path(sys.argv[3]).exists(): time.sleep(.02)
if sys.argv[4] == 'exception': raise RuntimeError('Dedicated fixture worker exit')
sys.exit(0)
'''
    process = subprocess.Popen([sys.executable, '-I', '-S', '-u', '-c', script,
                                str(ROOT / 'tools/mods'), str(ready), str(release), ending],
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.WaitForSingleObject.restype = wintypes.DWORD
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    child = None
    try:
        deadline = time.monotonic() + 12
        while not ready.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(.02)
        assert ready.exists(), process.communicate(timeout=2)[1].decode(errors='replace')
        child = kernel.OpenProcess(0x100000, False, json.loads(ready.read_text())['pid'])  # SYNCHRONIZE
        assert child and kernel.WaitForSingleObject(child, 0) == 258
        if ending == 'terminate':
            process.kill()
        else:
            release.write_bytes(b'finish')
        process.wait(timeout=10)
        assert kernel.WaitForSingleObject(child, 10000) == 0
        assert process.returncode == (0 if ending == 'exit' else 1)
    finally:
        if process.poll() is None:
            process.kill()
        process.communicate(timeout=10)
        if child:
            kernel.CloseHandle(child)
