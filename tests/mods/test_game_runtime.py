"""Production factory with real compile-pinned helpers and save/cache ownership."""
from pathlib import Path
import shutil
import subprocess
import sys

import pytest
from test_script_process import process_tools


@pytest.mark.parametrize('mode',['ordinary','run','results','audio-api','audio-budget','audio-unknown','audio-epoch','fault','repeat','tamper','other-thread','other-thread-live','other-thread-probe','missing','changed','cache-env','cache-missing',
    pytest.param('cache-empty',marks=pytest.mark.skipif(sys.platform=='win32',reason='Win32 empty environment values remove the variable')),
    pytest.param('large-parent',marks=pytest.mark.skipif(sys.platform!='linux',reason='Linux fork/exec RSS boundary'))])
def test_compiled_factory_only_admits_verified_offline_play_and_drains(process_tools,tmp_path,mode):
    worker=process_tools['worker'];app=tmp_path/'App \u03a9';app.mkdir()
    driver=app/('factory'+worker.suffix)
    shutil.copy2(worker.with_name('tsfp-mod-game-runtime-driver'+worker.suffix),driver)
    bundle=app/'ModRuntime'/('a'*64);bundle.mkdir(parents=True)
    shutil.copy2(worker,bundle/worker.name)
    guardian=worker.with_name('tsfp-mod-guardian')
    if guardian.is_file():shutil.copy2(guardian,bundle/guardian.name)
    if mode=='missing':(bundle/worker.name).unlink()
    if mode=='changed':(bundle/worker.name).write_bytes(b'changed compiled worker')
    cache=tmp_path/'Cache \u03a9';cache.mkdir()
    original=tmp_path/'Saves';original.mkdir();(original/'keep.sav').write_bytes(b'original progress')
    p=subprocess.run([str(driver),'refused' if mode in ('missing','changed') else mode,str(cache),str(original)],capture_output=True,timeout=15)
    assert p.returncode==0 and b'PASS' in p.stdout,(p.returncode,p.stdout,p.stderr)
    assert len(p.stderr)<2048
    assert not list(cache.iterdir()),'Private images stay owned until probe/child shutdown'
    assert (original/'keep.sav').read_bytes()==b'original progress' and len(list(original.iterdir()))==1
