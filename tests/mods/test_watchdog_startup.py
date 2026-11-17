"""A delayed real libc thread must initialize before the production TSYNC seal."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(sys.platform!='linux' or os.uname().machine!='x86_64',reason='Linux x64 production seccomp/rseq boundary')
def test_delayed_thread_registration_precedes_unchanged_process_wide_seal(tmp_path):
    import resource
    tracer=shutil.which('strace');compiler=shutil.which(os.environ.get('CC','clang')) or shutil.which('cc')
    if not tracer:pytest.skip('Install strace for actual thread-start syscall scheduling')
    assert compiler,'A native C compiler is required'
    binary=tmp_path/'watchdog-seal';trace=tmp_path/'startup.strace'
    command=[compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror','-pthread','-I'+str(ROOT/'src/mods'),
             str(ROOT/'tests/mods/native_watchdog_startup.c'),str(ROOT/'src/mods/mod_watchdog.c'),
             str(ROOT/'src/mods/mod_seal.c'),'-o',str(binary)]
    built=subprocess.run(command,capture_output=True,timeout=30)
    assert built.returncode==0,(built.stdout,built.stderr)
    original=resource.getrlimit(resource.RLIMIT_CORE)
    try:
        resource.setrlimit(resource.RLIMIT_CORE,(0,original[1]))
        result=subprocess.run([tracer,'-f','-qq','-e','inject=rseq:delay_enter=100ms','-o',str(trace),str(binary)],
                              capture_output=True,timeout=8)
    finally:resource.setrlimit(resource.RLIMIT_CORE,original)
    evidence=trace.read_text()
    assert result.returncode==0 and result.stdout==b'PASS\n',(result.returncode,result.stderr,evidence[-4096:])
    assert 'SECCOMP_FILTER_FLAG_TSYNC' in evidence and 'rseq(' in evidence and '(DELAYED)' in evidence
    assert 'SIGSEGV' not in evidence and 'SIGABRT' not in evidence
