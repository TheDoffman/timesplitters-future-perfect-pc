"""Verify actual shared texture persistence, restart and failed-save behavior."""
import os
from pathlib import Path
import subprocess
from native_compiler import native_compiler

ROOT=Path(__file__).resolve().parents[2]

def test_texture_preference(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('textures.exe' if os.name=='nt' else 'textures')
    sources=[str(ROOT/'tests/textures/texture_preference.c'),str(ROOT/'src/textures/texture_mode.c')]
    args=([compiler,'/nologo','/std:c11','/W4',*sources,'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-Wall','-Wextra','-Werror','-pthread',*sources,'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    env={k:v for k,v in env.items() if not k.startswith(('RECOMP_','TSFP_'))}
    env['TSFP_DISPLAY_CONFIG']=str(tmp_path/'display.ini')
    preference=tmp_path/'pc-textures.ini'
    def run(initial,change=None,ok=1):
        args=[str(exe),str(initial)]
        if change is not None:args += [str(change),str(ok)]
        result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=10)
        assert result.returncode==0,result.stdout+result.stderr
    run(0,1)
    assert preference.read_text()=='TSFP_TEXTURES_V1 1\n'
    env['TSFP_TEXTURE_MODE']='classic'
    run(1,0)
    env['TSFP_TEXTURE_MODE']='4k'
    run(0)  # saved OFF also overrides the old launcher default
    run(0,2)  # F10 saves ON
    run(1,2)  # F10 saves OFF
    run(0)
    assert not (tmp_path/'pc-textures.ini.tmp').exists()
    del env['TSFP_DISPLAY_CONFIG']
    env['RECOMP_INPUT_CONFIG']=str(tmp_path/'controls.json')
    run(0,1)  # input-config fallback selects the same directory
    run(1)
    env['TSFP_DISPLAY_CONFIG']=str(tmp_path/'missing'/'display.ini')
    run(1,0,0)  # neither the live setting nor existing preference changes
    run(1,2,0)
    assert preference.read_text()=='TSFP_TEXTURES_V1 1\n'
    env['TSFP_DISPLAY_CONFIG']=str(tmp_path/'display.ini')
    for invalid in ('broken','TSFP_TEXTURES_V1 2','TSFP_TEXTURES_V1 0 trailing'):
        preference.write_text(invalid)
        run(1)  # malformed files fall back to the launch mode, without rewriting
        assert preference.read_text()==invalid
    preference.unlink()
    del env['RECOMP_INPUT_CONFIG']
    del env['TSFP_TEXTURE_MODE']
    env['TSFP_TEXTURE_PACK_DIR']=str(tmp_path)
    run(1)  # existing explicit pack-preview default stays available
