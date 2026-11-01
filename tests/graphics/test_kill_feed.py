import os
import subprocess
from pathlib import Path
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
def test_kill_feed(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('feed.exe' if os.name=='nt' else 'feed')
    source=ROOT/'tests/graphics/kill_feed.c'
    args=([compiler,'/nologo','/std:c11','/W4','/WX','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(source),'-o',str(exe)])
    run=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert run.returncode==0,run.stdout+run.stderr


def test_feed_preference(tmp_path):
    compiler,env=native_compiler(tmp_path)
    exe=tmp_path/('settings.exe' if os.name=='nt' else 'settings')
    source=ROOT/'tests/graphics/kill_feed_settings.c'
    args=([compiler,'/nologo','/std:c11','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT/'src'),str(source),'/Fe:'+str(exe)] if os.name=='nt' else
          [compiler,'-std=c11','-I'+str(ROOT/'src'),str(source),'-o',str(exe)])
    result=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    env=dict(os.environ,TSFP_DISPLAY_CONFIG=str(tmp_path/'pc-display.ini'))
    def run(action,value):
        result=subprocess.run([str(exe),action,str(value)],env=env,capture_output=True,text=True,timeout=10)
        assert result.returncode==0,result.stdout+result.stderr
    run('read',0)
    run('write',1)
    run('read',1)
    run('write',0)
    run('read',0)
    (tmp_path/'pc-hud.ini').write_text('invalid')
    run('read',0)
    env['TSFP_DISPLAY_CONFIG']=str(tmp_path/'missing'/'pc-display.ini')
    run('fail',0)
