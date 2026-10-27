"""Execute the actual setter and upload block, with controlled shader/RHI services."""


import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.parametrize('mutation',['none','always-dirty','lost-change','failed-map'])
def test_vsh_constant_dirty(tmp_path,mutation):
    current=(ROOT/'external/ts3FPrecomp/src/d3d/d3d8_vsh.c').read_text()
    start=current.index('void d3d8_vsh_set_constant(')
    setter=current[start:current.index('\n}\n',start)+3]
    start=current.index('    /* Update constant buffer if dirty,')
    upload=current[start:current.index('    /* Bind constant buffer to slot b1 */',start)]
    reference=(ROOT/'tests/support/vsh_constant_reference.inc').read_text()
    old_setter,old_upload=reference.split('/* REFERENCE_UPLOAD */')
    old_setter=old_setter.replace('void d3d8_vsh_set_constant(','static void reference_set_constant(')
    if mutation=='always-dirty':
        assert 'if (changed) g_vsh_constants_dirty = TRUE;' in setter
        setter=setter.replace('if (changed) g_vsh_constants_dirty = TRUE;','(void)changed; g_vsh_constants_dirty = TRUE;')
    if mutation=='lost-change':
        assert 'if (changed) g_vsh_constants_dirty = TRUE;' in setter
        setter=setter.replace('if (changed) g_vsh_constants_dirty = TRUE;','(void)changed;')
    if mutation=='failed-map':
        anchor='            } /* A failed map leaves the pending data dirty for retry. */'
        assert anchor in upload
        upload=upload.replace(anchor,anchor+'\n            g_vsh_constants_dirty = FALSE;')
    source=(ROOT/'tests/graphics/vsh_constant_dirty.c').read_text()
    for marker,body in [('REFERENCE_SETTER',old_setter),('REFERENCE_UPLOAD',old_upload),('PRODUCTION_SETTER',setter),('PRODUCTION_UPLOAD',upload)]:
        source=source.replace('/* '+marker+' */',body)
    path=tmp_path/'constants.c';path.write_text(source)
    compiler,env=native_compiler(tmp_path);exe=tmp_path/('constants.exe' if os.name=='nt' else 'constants')
    args=([compiler,'/nologo','/O2','/W4','/WX','/std:c11',str(path),'/Fe:'+str(exe),'/Fo:'+str(tmp_path/'constants.obj')]
          if os.name=='nt' else [compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror',str(path),'-lm','-o',str(exe)])
    built=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert built.returncode==0,built.stdout+built.stderr
    ran=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation=='none':assert ran.returncode==0,ran.stdout+ran.stderr
    else:assert ran.returncode!=0,'Independent dirty/upload mutation escaped its check'
