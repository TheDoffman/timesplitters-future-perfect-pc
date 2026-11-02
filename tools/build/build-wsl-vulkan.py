#!/usr/bin/env python3
"""Build the isolated experimental Dozen WSL GPU driver; keep system Mesa intact."""
import argparse
import json
import platform
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
from vulkan_sdk import sha256

VERSION = '26.2.5'
ARCHIVE_HASH = '794b3d1729075dae7ebd47c7283b7ca0cdb5dec668f7f11cc0a5d82f6138a0a5'
URL = f'https://archive.mesa3d.org/mesa-{VERSION}.tar.xz'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if platform.system() != 'Linux' or not Path('/usr/lib/wsl/lib/libd3d12.so').is_file():
        parser.error('Requires Linux under WSL with the Windows D3D12 bridge')
    if not 1 <= args.jobs <= 16:
        parser.error('Use 1–16 build jobs')
    base = ROOT / 'external/mesa-dzn'
    if base.is_symlink() or not base.resolve().is_relative_to((ROOT / 'external').resolve()):
        parser.error('Refusing redirected build directory')
    if shutil.disk_usage(ROOT).free < 2 * 1024**3:
        parser.error('Driver build needs at least 2 GiB free')
    base.mkdir(parents=True, exist_ok=True)
    archive = base / f'mesa-{VERSION}.tar.xz'
    if not archive.exists():
        partial = archive.with_suffix('.partial')
        with urllib.request.urlopen(URL, timeout=60) as response, partial.open('wb') as output:
            shutil.copyfileobj(response, output)
        partial.replace(archive)
    if sha256(archive) != ARCHIVE_HASH:
        raise RuntimeError('Mesa archive differs from the publisher SHA-256 pin')
    source = base / f'mesa-{VERSION}'
    if not source.exists():
        with tarfile.open(archive, 'r:xz') as bundle:
            bundle.extractall(base, filter='data')
    run = lambda *command, **kwargs: subprocess.run([str(c) for c in command], check=True, **kwargs)
    patch = ROOT / 'tools/build/mesa-dzn-granularity.diff'
    # Preserve an already-patched source; never reset unknown source changes.
    reverse = subprocess.run(['patch', '-p1', '--dry-run', '--batch', '--reverse', '-i', str(patch)],
                             cwd=source, capture_output=True)
    if reverse.returncode:
        run('patch', '-p1', '--batch', '--forward', '-i', patch, cwd=source)
    venv = base / '.venv'
    if not (venv / 'bin/python').exists():
        run(sys.executable, '-m', 'venv', '--system-site-packages', venv)
    meson = venv / 'bin/meson'
    if not meson.exists() or subprocess.check_output([str(meson), '--version'], text=True).strip() != '1.9.2':
        run(venv / 'bin/python', '-m', 'pip', 'install', 'meson==1.9.2')
    build, install = base / 'build', base / 'install'
    options = ['--reconfigure'] if (build / 'meson-private/coredata.dat').exists() else []
    run(meson, 'setup', *options, build, source, '--prefix=' + str(install), '--buildtype=release',
        '-Dgallium-drivers=', '-Dvulkan-drivers=microsoft-experimental', '-Dplatforms=x11,wayland',
        '-Dglx=disabled', '-Degl=disabled', '-Dopengl=false', '-Dgles1=disabled', '-Dgles2=disabled',
        '-Dllvm=disabled', '-Dbuild-tests=false', '-Dvideo-codecs=', '-Dlibunwind=disabled')
    run(meson, 'compile', '-C', build, '-j', args.jobs)
    run(meson, 'install', '-C', build, '--no-rebuild')
    library = install / 'lib/x86_64-linux-gnu/libvulkan_dzn.so'
    manifest = install / 'share/vulkan/icd.d/dzn_icd.x86_64.json'
    receipt = dict(url=URL, sha256=ARCHIVE_HASH, mesa_version=VERSION,
                   source_revision=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                   patch_sha256=sha256(patch), library_sha256=sha256(library),
                   manifest_sha256=sha256(manifest),
                   scope='Experimental isolated WSL Dozen GPU driver; system Mesa unchanged')
    (base / 'SOURCE.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Isolated ICD:', manifest)
    print('Run renderer and actual-game acceptance with --driver-manifest before interactive use.')


if __name__ == '__main__':
    main()
