"""Exercise Linux memory-query UAPI decisions on every CI host."""
import os
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def test_linux_binary_map_query(tmp_path):
    for source in (ROOT/'src/runtime/pc_guest_memory.h',ROOT/'tests/compiler/linux_map_query.c'):
        (tmp_path/source.name).write_bytes(source.read_bytes())
    headers={
        'fcntl.h':'#define O_RDONLY 0\nint test_open(const char *,int,...);\n',
        'unistd.h':'int test_close(int);\n',
        'sys/ioctl.h':'#include <stdarg.h>\n#define _IOWR(t,n,s) ((3ul<<30)|(sizeof(s)<<16)|((t)<<8)|(n))\nint test_ioctl(int,unsigned long,...);\n'}
    for name,content in headers.items():
        p=tmp_path/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(content)
    flags=['/W4','/D_CRT_SECURE_NO_WARNINGS','/I'+str(tmp_path)] if os.name=='nt' else ['-Wall','-Wextra','-Werror','-I'+str(tmp_path)]
    exe=_compile(tmp_path,tmp_path/'linux_map_query.c',extra=flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr
