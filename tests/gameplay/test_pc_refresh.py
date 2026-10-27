"""Window display adapter regression with mocked physical monitor operations."""


import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]


@pytest.mark.skipif(os.name!='nt',reason='Win32 monitor adapter')
@pytest.mark.parametrize('fixture',['pc_refresh','pc_refresh_guard'])
def test_refresh_mode_lifecycle(tmp_path,fixture):
    compiler,env=native_compiler(tmp_path)
    output=tmp_path/(fixture+'.exe')
    subprocess.run([compiler,'/nologo','/std:c11','/MD',str(ROOT/('tests/gameplay/'+fixture+'.c')),
                    '/Fe:'+str(output),'/Fo:'+str(tmp_path/'refresh.obj'),'/link','user32.lib'],
                   env=env,cwd=tmp_path,check=True)
    subprocess.run([str(output)],check=True,timeout=10)
