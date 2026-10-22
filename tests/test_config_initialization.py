import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[1]


@pytest.mark.skipif(os.name!='nt',reason='Windows concurrent initialization regression')
def test_renderer_cannot_cache_partial_settings(tmp_path):
    compiler,env=native_compiler(tmp_path)
    output=tmp_path/'config-initialization.exe'
    subprocess.run([compiler,'/nologo','/std:c11','/MD','/I'+str(ROOT/'external/ts3FPrecomp/src'),
                    '/I'+str(ROOT/'src'),
                    str(ROOT/'tests/config_initialization.c'),'/Fe:'+str(output),
                    '/Fo:'+str(tmp_path/'config.obj')],env=env,cwd=tmp_path,check=True)
    subprocess.run([str(output),str(tmp_path/'runtime.ini')],check=True,timeout=15)
