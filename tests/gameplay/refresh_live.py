"""Opt-in physical 120/144 Hz monitor check, including abrupt process death.

Run only with --switch-monitor. Mode changes preserve the desktop resolution
and the child recovery helper restores the original rate on process death.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import ctypes
import os
from pathlib import Path
import struct
import subprocess
import time
from native_compiler import native_compiler

ROOT=_layout_root


def mode():
    buffer=ctypes.create_string_buffer(220)
    struct.pack_into('H',buffer,68,220)
    assert ctypes.windll.user32.EnumDisplaySettingsW(None,-1,buffer)
    return struct.unpack_from('IIII',buffer,172)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--switch-monitor',action='store_true');args=parser.parse_args()
    if os.name!='nt' or not args.switch_monitor:parser.error('Requires Windows and --switch-monitor')
    before=mode()
    directory=ROOT/'build/issue167';directory.mkdir(exist_ok=True)
    compiler,env=native_compiler(directory)
    output=ROOT/'build/windows/Release/tsfp_refresh_validation.exe'
    subprocess.run([compiler,'/nologo','/std:c11','/MD','/I'+str(ROOT/'src'),
        str(ROOT/'tests/gameplay/pc_refresh_live.c'),str(ROOT/'src/interface/pc_refresh.c'),
        '/Fe:'+str(output),'/Fo:'+str(directory)+os.sep,'/link','user32.lib'],env=env,cwd=directory,check=True)
    try:
        for extra in ([],['--abrupt']):
            result=subprocess.run([str(output),*extra],capture_output=True,text=True,timeout=15)
            print(result.stdout,result.stderr,flush=True)
            if result.returncode==77 or (result.returncode==78 and 'unavailable at' in result.stderr):
                print('SKIP: active test display/focus cannot exercise physical 120/144 Hz; original mode preserved',before,flush=True)
                return
            assert result.returncode==0,result.returncode
            for _ in range(50):
                if mode()==before:break
                time.sleep(.1)
            assert mode()==before,(before,mode())
        print('PASS: physical monitor unchanged after normal and abrupt exits',before,flush=True)
    finally:
        for _ in range(50):
            if mode()==before:break
            time.sleep(.1)
        assert mode()==before,'Refresh restoration failed; inspect the active display before continuing'


if __name__=='__main__':main()
