#!/usr/bin/env python3
"""Run a package as nobody in a prepared minimal Ubuntu userspace (root required).

Use an isolated Xvfb display with access control disabled. Mounts are private to
the child namespace; the package and original game data are bound read-only.
The prepared root must contain libvulkan1, Mesa Vulkan drivers, fonts and CAs.
This checks userspace relocation, not a physical Linux desktop or GPU driver.
"""
import argparse
import json
import os
import re
import time
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
APP = '/app test Ω'
DATA = '/game data Ω'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rootfs', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--game-dir', type=Path, required=True)
    parser.add_argument('--inside', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if sys.platform != 'linux' or os.geteuid() != 0:
        parser.error('Run as root on Linux in a private Xvfb display')
    rootfs, package, game = (p.resolve(strict=True) for p in (args.rootfs, args.package, args.game_dir))
    if rootfs == Path('/') or not (rootfs/'usr/bin/env').is_file():
        parser.error('Expected a prepared separate Ubuntu rootfs')
    if not (package/'BUILD.json').is_file() or not (game/'default.xbe').is_file():
        parser.error('Package or supported game directory missing')
    if not os.environ.get('DISPLAY') or not Path('/tmp/.X11-unix').is_dir():
        parser.error('Start under a dedicated Xvfb display')
    if not args.inside:
        display = re.fullmatch(r':([0-9]+)(?:\.[0-9]+)?', os.environ['DISPLAY'])
        if not display:
            parser.error('Expected a local dedicated Xvfb display')
        server = Path('/tmp/.X'+display[1]+'-lock').read_text().strip()
        if b'Xvfb' not in Path('/proc/'+server+'/cmdline').read_bytes().split(b'\0')[0]:
            parser.error('Refusing to start a window manager on a non-Xvfb desktop')
        wm = subprocess.Popen(['openbox', '--sm-disable'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(.5)
        try:
            subprocess.run(['unshare', '--mount', '--fork', sys.executable, str(Path(__file__).resolve()),
                        '--inside', '--rootfs', str(rootfs), '--package', str(package),
                        '--game-dir', str(game)], check=True)
        finally:
            wm.terminate()
            wm.wait(timeout=10)
        return
    run = lambda *command: subprocess.run([str(x) for x in command], check=True)
    run('mount', '--make-rprivate', '/')
    for source, target, readonly in ((package, APP[1:], True), (game, DATA[1:], True),
                                      (Path('/dev'), 'dev', False),
                                      (Path('/tmp/.X11-unix'), 'tmp/.X11-unix', True)):
        destination = rootfs/target
        if destination.is_symlink():
            raise ValueError('Linked rootfs mount target: '+target)
        destination.mkdir(parents=True, exist_ok=True)
        run('mount', '--bind', source, destination)
        if readonly:
            run('mount', '-o', 'remount,bind,ro', destination)
    run('mount', '-t', 'proc', 'proc', rootfs/'proc')
    # tmpfs keeps each clean-room profile disposable and isolated from past runs.
    home = rootfs/'tmp/tsfp-clean-home'
    home.mkdir(exist_ok=True)
    # Save-backup preflight reserves 512 MiB plus overhead. A 512 MiB test
    # filesystem rejects that check even when the fixture stores tiny files.
    run('mount', '-t', 'tmpfs', '-o', 'mode=1777,size=1024m', 'tmpfs', home)
    env = ['HOME=/tmp/tsfp-clean-home', 'PATH=/usr/bin:/bin', 'LANG=C.UTF-8',
           'DISPLAY='+os.environ['DISPLAY'], 'SDL_VIDEODRIVER=x11', 'SDL_AUDIODRIVER=dummy',
           'LIBGL_ALWAYS_SOFTWARE=1', 'LD_LIBRARY_PATH='+APP+'/Bin']
    prefix = ['chroot', '--userspec=65534:65534', str(rootfs), '/usr/bin/env', '-i', *env, '/bin/sh', '-c', 'cd "$HOME" && exec "$@"', 'tsfp-clean']
    results = {}
    for option in ('settings', 'debug', 'input', 'renderer', 'online'):
        command = [*prefix, APP+'/Launch.sh', '--game', '--data', DATA, '--'+option+'-self-test']
        result = subprocess.run(command, capture_output=True, text=True, timeout=90)
        print(result.stdout+result.stderr, flush=True)
        results[option] = dict(exit_code=result.returncode, output=result.stdout+result.stderr)
    code = ('import ssl,tkinter,cryptography,cffi; '
            't=tkinter.Tk();t.withdraw();t.update();t.destroy(); '
            'assert ssl.create_default_context().cert_store_stats()["x509_ca"]>0; '
            'print("PASS clean-userspace bundled Python/Tk/TLS")')
    result = subprocess.run([*prefix, APP+'/Support/Python/bin/python3', '-c', code],
                            capture_output=True, text=True, timeout=45)
    print(result.stdout+result.stderr, flush=True)
    results['python'] = dict(exit_code=result.returncode, output=result.stdout+result.stderr)
    result = subprocess.run([*prefix, '/usr/bin/env', 'FONTCONFIG_FILE='+APP+'/Support/Desktop/fonts.conf', APP+'/TimeSplitters.Launcher', '--test-ui'],
                            capture_output=True, text=True, timeout=90)
    print(result.stdout+result.stderr, flush=True)
    results['launcher_ui'] = dict(exit_code=result.returncode, output=result.stdout+result.stderr)
    logs = ROOT/'logs'
    logs.mkdir(exist_ok=True)
    skipped = [name for name,r in results.items() if name=='online' and r['exit_code']==77]
    report = dict(scope='Read-only package and assets, nobody, minimal Ubuntu userspace; software Vulkan',
                  package=str(package), tests=results, skipped=skipped,
                  passed=all(r['exit_code']==0 or name in skipped for name,r in results.items()))
    (logs/'linux-clean-package.json').write_text(json.dumps(report, indent=2)+'\n')
    if not report['passed']:
        raise SystemExit('FAIL clean-userspace acceptance; see logs/linux-clean-package.json')
    print('PASS clean-userspace package acceptance')


if __name__ == '__main__':
    main()
