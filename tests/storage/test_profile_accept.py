

import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]


def test_profile_accept(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('profile-accept.exe' if os.name=='nt' else 'profile-accept')
    source=str(ROOT/'tests/storage/profile_accept.c')
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/W4','/I'+str(ROOT/'src'),source,'/Fe:'+str(exe)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),source,'-o',str(exe)]
    subprocess.run(args,cwd=tmp_path,env=env,check=True,timeout=60)
    subprocess.run([str(exe)],check=True,timeout=10)
