"""Production upload copying preserves every byte while batching tight spans."""
import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]


def test_upload_rows(tmp_path):
    compiler,env=native_compiler(tmp_path)
    output=tmp_path/('upload-rows.exe' if os.name=='nt' else 'upload-rows')
    source=str(ROOT/'tests/graphics/upload_rows.c')
    include=str(ROOT/'src/graphics')
    command=([compiler,'/nologo','/std:c11','/W4','/O2','/I'+include,source,'/Fe:'+str(output)]
             if os.name=='nt' else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',
                                  '-fsanitize=address,undefined','-I'+include,source,'-o',str(output)])
    subprocess.run(command,cwd=tmp_path,env=env,check=True,timeout=60)
    subprocess.run([str(output)],env=env,check=True,timeout=15)
