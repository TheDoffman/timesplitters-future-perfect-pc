"""Bounded, isolated startup-video comparison; --exe selects an existing build.

No desktop input is injected. Retains one PNG per run and bounded logs, and
removes only this invocation's fresh disposable save directory.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[2] / 'tools'
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from PIL import Image

ROOT = _layout_root


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.resolve()
    assert hashlib.sha256((ROOT / 'private/game/default.xbe').read_bytes()).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    folder = ROOT / 'private/startup-validation'
    folder.mkdir(parents=True, exist_ok=True)
    for skip in (False, True):
        run = folder / ('skip' if skip else 'normal')
        run.mkdir(exist_ok=True)
        saves = Path(tempfile.mkdtemp(prefix='startup-saves-', dir=run)).resolve()
        controls = run / 'pc-controls.json'
        controls.write_bytes((ROOT / 'tests/fixtures/pc-controls.json').read_bytes())
        runtime = run / 'runtime.conf'
        runtime.write_text('resolution_scale = 1\nfullscreen = 0\n')
        display = run / 'pc-display.ini'
        display.write_text('TSFP_DISPLAY_V1 0 0\n')
        env = {k: v for k, v in os.environ.items() if not k.startswith(('RECOMP_', 'TSFP_'))}
        env.update(RECOMP_GAME_DIR=str(ROOT / 'private/game'), RECOMP_SAVE_DIR=str(saves),
                   RECOMP_INPUT_CONFIG=str(controls), RECOMP_DISPLAY_CONFIG=str(runtime),
                   TSFP_DISPLAY_CONFIG=str(display), RECOMP_TRACE_BUDGET='0',
                   RECOMP_FFMPEG_DIR=str(ROOT / 'external/ts3FPrecomp/third_party/ffmpeg/bin'),
                   RECOMP_XMV_PLAY='0' if skip else '1', RECOMP_WINDOW_BACKGROUND='1',
                   RECOMP_HLE_D3D8_DUMP=str(run / 'frame'),
                   RECOMP_HLE_D3D8_DUMP_EVERY='120', RECOMP_HLE_D3D8_DUMP_KEEP_LAST='2')
        try:
            with (run / 'game.out').open('wb') as out, (run / 'game.err').open('wb') as err:
                game = subprocess.Popen([str(exe)], cwd=exe.parent, env=env, stdout=out, stderr=err)
                try:
                    deadline = time.monotonic() + 20
                    while time.monotonic() < deadline:
                        assert game.poll() is None, 'Game exited during startup'
                        assert (run / 'game.err').stat().st_size < 4 * 1024 * 1024
                        for frame in sorted(run.glob('frame*.bmp'), key=lambda p: p.stat().st_mtime_ns)[:-2]:
                            frame.unlink()
                        time.sleep(.2)
                    frames = sorted(run.glob('frame*.bmp'), key=lambda p: p.stat().st_mtime_ns)
                    assert frames, 'No rendered frame captured'
                    # Use the older completed capture while the newest may be writing.
                    with Image.open(frames[0]) as image:
                        image.save(run / 'startup.png')
                finally:
                    if game.poll() is None:
                        game.terminate()
                    game.wait(timeout=5)
            log = (run / 'game.err').read_text(errors='replace')
            assert '[CRASH]' not in log and '[ICALL] unresolved' not in log
            if skip:
                assert '[XMV] playing ' not in log
                assert 'the movie is not being played' in log
            else:
                assert '[XMV] playing ' in log and 'eag_e.xmv' in log and 'frd.xmv' in log
            print('PASS:', 'skip enabled' if skip else 'normal startup', flush=True)
        finally:
            assert saves.parent == run.resolve() and saves.name.startswith('startup-saves-')
            assert not saves.is_symlink() and not any(p.is_symlink() for p in saves.rglob('*'))
            shutil.rmtree(saves)
            for frame in run.glob('frame*.bmp'):
                frame.unlink()


if __name__ == '__main__':
    main()
