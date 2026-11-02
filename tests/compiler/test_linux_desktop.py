"""Compile the real Linux GTK adapter with the pinned SDL/public shared headers."""
import os
from pathlib import Path
import subprocess
import sys
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
@pytest.mark.skipif(sys.platform!='linux',reason='Linux GTK desktop adapter')
def test_linux_desktop_adapter_compiles(tmp_path):
    compiler,env=native_compiler(tmp_path)
    flags=subprocess.check_output(['pkg-config','--cflags','gtk+-3.0'],text=True).split()
    command=[compiler,'-std=c11','-fsyntax-only','-Werror=implicit-function-declaration',*flags,
        '-I'+str(ROOT/'external/ts3FPrecomp/third_party/SDL/include'),
        str(ROOT/'src/native/settings_linux.c')]
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==0,result.stdout+result.stderr
