"""Shared slot input/guest selection policy, vocabulary and host integration."""


import json
import re
from pathlib import Path
import subprocess
import os
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

def test_keyboard_menu(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('keyboard-menu.exe' if os.name=='nt' else 'keyboard-menu')
    source=str(ROOT/'tests/input/keyboard_menu.c')
    args=([compiler,'/nologo','/std:c11','/W4','/WX','/I'+str(ROOT/'src'),source,'/Fe:'+str(exe)]
          if os.name=='nt' else [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),source,'-o',str(exe),'-lm'])
    p=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert p.returncode==0,p.stdout+p.stderr
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert p.returncode==0,p.stdout+p.stderr
