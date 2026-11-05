"""Real SDL startup ownership on both Linux desktop backends."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import pytest
ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(sys.platform!='linux',reason='Linux SDL host execution')
@pytest.mark.parametrize('backend',['x11','wayland'])
def test_audio_first_cannot_claim_main_thread(tmp_path,backend):
    candidates=[ROOT/'build/linux/rhi-tests/SDL/libSDL3.a',ROOT/'build/linux/game/xboxrecomp/third_party/SDL/libSDL3.a']
    library=next((p for p in candidates if p.is_file()),None)
    assert library is not None,'Build the Linux RHI contracts or game/SDL first'
    tool='xvfb-run' if backend=='x11' else 'weston'
    assert shutil.which(tool),f'Install {tool} for Linux display acceptance'
    exe=tmp_path/'video-startup'
    subprocess.run(['cc',str(ROOT/'tests/gameplay/native_video_startup.c'),
        '-I'+str(ROOT/'src/native'),'-I'+str(ROOT/'external/ts3FPrecomp/third_party/SDL/include'),
        str(library),'-lm','-ldl','-lpthread','-o',str(exe)],check=True)
    tmp_path.chmod(0o700)
    env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER=backend,
             XDG_RUNTIME_DIR=str(tmp_path),XDG_CONFIG_HOME=str(tmp_path/'config'),
             XDG_CACHE_HOME=str(tmp_path/'cache'),XDG_DATA_HOME=str(tmp_path/'data'))
    compositor=None
    with (tmp_path/'compositor.log').open('w') as log:
        try:
            prefix=['xvfb-run','-a'] if backend=='x11' else []
            if backend=='wayland':
                env['WAYLAND_DISPLAY']='tsfp-test'
                compositor=subprocess.Popen(['weston','--backend=headless-backend.so',
                    '--renderer=pixman','--width=1280','--height=800','--socket=tsfp-test',
                    '--idle-time=0','--no-config'],env=env,stdout=log,stderr=log)
                for _ in range(100):
                    if (tmp_path/'tsfp-test').exists():break
                    if compositor.poll() is not None:break
                    time.sleep(.1)
                assert (tmp_path/'tsfp-test').exists(),(tmp_path/'compositor.log').read_text()
            good=subprocess.run(prefix+[str(exe)],env=env,cwd=tmp_path,capture_output=True,text=True,timeout=20)
            assert good.returncode==0,good.stdout+good.stderr
            assert f'display backend: {backend}' in good.stderr
            bad=subprocess.run(prefix+[str(exe),'audio-first'],env=env,cwd=tmp_path,capture_output=True,text=True,timeout=20)
            assert bad.returncode==4,bad.stdout+bad.stderr
        finally:
            if compositor is not None:
                compositor.terminate()
                try:compositor.wait(timeout=10)
                except subprocess.TimeoutExpired:compositor.kill();compositor.wait()
