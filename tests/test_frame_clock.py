"""Exercise the fixed 60 FPS configuration policy on both hosts."""
import os
from pathlib import Path
import subprocess
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('native_phase,omit_observer',
                         [(None,False),(0,False),(100,False),(500,False),
                          (900,False),(999,False),(500,True)])
def test_late_frame_does_not_wait_twice(tmp_path,native_phase,omit_observer):
    source=(ROOT/'external/ts3FPrecomp/src/kernel/xbox_memory_layout.c').read_text(encoding='utf-8')
    start=source.index('void xbox_Nv2aFlipGateArm(void)')
    end=source.index('\nvoid xbox_Nv2aFlipGateRelease(void)',start)
    observer='void xbox_Nv2aFlipGateNativeFrame(void)'
    body=source[source.index(observer):end]
    if omit_observer:
        # Mutation control: skipping the observation must reproduce the hitch.
        body=observer+' {}\n'+source[start:end]
    (tmp_path/'arm.inc').write_text(body,encoding='utf-8')
    compiler, environment=native_compiler(tmp_path)
    output=tmp_path/('gate-deadline.exe' if os.name=='nt' else 'gate-deadline')
    fixture=str(ROOT/'tests/flip_gate_deadline.c')
    command=([compiler,'/nologo','/std:c11','/I'+str(tmp_path),fixture,'/Fe:'+str(output)]
             if os.name=='nt' else [compiler,'-std=c11','-I'+str(tmp_path),fixture,'-o',str(output)])
    subprocess.run(command,cwd=tmp_path,env=environment,check=True,timeout=60)
    result=subprocess.run([str(output)]+([] if native_phase is None else [str(native_phase)]),
                          capture_output=True,text=True,timeout=10)
    if omit_observer:
        assert result.returncode != 0
        assert 'now==2100 && waits==0' in result.stderr
    else:
        assert result.returncode == 0, result.stderr


def test_target_settings_round_trip(tmp_path):
    compiler, environment = native_compiler(tmp_path)
    config=ROOT / 'external/ts3FPrecomp/src/config'
    output=tmp_path / ('target-settings.exe' if os.name == 'nt' else 'target-settings')
    sources=[str(ROOT / 'tests/frame_target_settings.c'), str(config / 'recomp_config.c')]
    command=([compiler, '/nologo', '/std:c11', '/I'+str(config), '/I'+str(ROOT/'src'), *sources,
              '/Fe:'+str(output), '/Fo:'+str(tmp_path)+os.sep]
             if os.name == 'nt' else [compiler, '-std=c11', '-I'+str(config), '-I'+str(ROOT/'src'), *sources, '-o', str(output)])
    subprocess.run(command, cwd=tmp_path, env=environment, check=True)
    subprocess.run([str(output), str(tmp_path / 'runtime.ini')], check=True, timeout=10)


def test_player_frame_rate_startup_preference(tmp_path):
    compiler, environment = native_compiler(tmp_path)
    config=ROOT/'external/ts3FPrecomp/src/config'
    output=tmp_path/('rates.exe' if os.name=='nt' else 'rates')
    sources=[str(ROOT/'tests/frame_rate_settings.c'),str(config/'recomp_config.c')]
    command=([compiler,'/nologo','/std:c11','/I'+str(config),'/I'+str(ROOT/'src'),*sources,
              '/Fe:'+str(output),'/Fo:'+str(tmp_path)+os.sep] if os.name=='nt' else
             [compiler,'-std=c11','-I'+str(config),'-I'+str(ROOT/'src'),*sources,'-o',str(output)])
    subprocess.run(command,cwd=tmp_path,env=environment,check=True)
    display=tmp_path/'display.ini'
    display.write_text('TSFP_DISPLAY_V2 1920 1080 75\n')
    for before,after in [(144,120),(120,144),(144,0),(0,120)]:
        subprocess.run([str(output),str(display),str(before),str(after)],check=True,timeout=10)
        assert display.read_text()=='TSFP_DISPLAY_V2 1920 1080 75\n'
    preference=tmp_path/'pc-frame-rate.ini'
    for invalid in ['TSFP_FRAME_RATE_V1 240','TSFP_FRAME_RATE_V1 120 extra','TSFP_FRAME_RATE_V2 120']:
        preference.write_text(invalid)
        subprocess.run([str(output),str(display),'144','0'],check=True,timeout=10)
