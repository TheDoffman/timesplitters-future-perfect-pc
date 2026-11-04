#!/usr/bin/env python3
"""Build/package the Linux x64 launcher using the shared Avalonia UI and policy."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dotnet', type=Path)
    parser.add_argument('--install-sdk', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/linux-launcher')
    parser.add_argument('--artwork-dir', type=Path, default=ROOT / 'private/launcher-art')
    parser.add_argument('--require-artwork', action='store_true')
    parser.add_argument('--test', action='store_true')
    parser.add_argument('--package', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        parser.error('Build and test on Linux x64 (including Ubuntu under WSL).')
    pin = json.loads((ROOT / 'config/build/linux-launcher-sdk.json').read_text())
    sdk = (args.dotnet or ROOT / 'external/dotnet-linux/dotnet').resolve()
    if args.install_sdk and not sdk.exists():
        sdk.parent.mkdir(parents=True, exist_ok=True)
        archive = sdk.parent / 'sdk.tar.gz'
        urllib.request.urlretrieve(pin['url'], archive)
        if hashlib.sha512(archive.read_bytes()).hexdigest() != pin['sha512']:
            raise SystemExit('SDK checksum mismatch; archive retained for inspection.')
        with tarfile.open(archive) as tar:
            tar.extractall(sdk.parent, filter='data')
        archive.unlink()
    if not sdk.exists():
        raise SystemExit('Pinned SDK missing. Use --install-sdk or --dotnet.')
    env = dict(os.environ, DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_NOLOGO='1',
               DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1')
    if subprocess.check_output([str(sdk), '--version'], env=env, text=True).strip() != pin['version']:
        raise SystemExit('Use the SDK pinned in config/build/linux-launcher-sdk.json.')
    if args.require_artwork:
        for name in ('launcher-banner.png', 'launcher-logo.png', 'launcher-backdrop.png'):
            if not (args.artwork_dir / name).is_file():
                raise SystemExit('Required artwork missing: ' + name)
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents and not output.is_relative_to(ROOT / 'build'):
        raise SystemExit('Use a dedicated build output directory.')
    output.mkdir(parents=True, exist_ok=True)
    # Generate the same layout/themes without writing into the Mac source tree.
    layout_dir = output / 'layout'
    layout_dir.mkdir(exist_ok=True)
    spec = importlib.util.spec_from_file_location('layout', ROOT / 'tools/launcher/macos/generate_layout.py')
    layout = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(layout)
    layout.generate(ROOT / 'tools/launcher/Launcher.xaml', layout_dir / 'NativeWindow.axaml')
    for theme in ('PageTheme', 'TabsTheme'):
        layout.generate_theme(ROOT / f'tools/launcher/{theme}.xaml', layout_dir / f'{theme}.axaml')
    publish = output / 'publish'
    subprocess.run([str(sdk), 'publish', str(ROOT / 'tools/launcher/linux/LinuxLauncher.csproj'),
                    '-c', 'Release', '-r', 'linux-x64', '--self-contained', 'true',
                    '-p:RestoreLockedMode=true', '-p:PublishSingleFile=true',
                    '-p:IncludeNativeLibrariesForSelfExtract=true', '-p:EnableCompressionInSingleFile=true',
                    '-p:DebugType=None', '-p:DebugSymbols=false',
                    f'-p:BaseIntermediateOutputPath={output}/obj/', f'-p:BaseOutputPath={output}/bin/',
                    f'-p:LayoutDir={layout_dir}', f'-p:ArtworkDir={args.artwork_dir.resolve()}',
                    '-o', str(publish)], check=True, cwd=ROOT, env=env)
    package = output / 'package'
    package.mkdir(exist_ok=True)
    # A single-file host contains its native dependencies. Reject unexpected output
    # instead of silently producing an incomplete Linux archive.
    extras = [p.name for p in publish.iterdir() if p.name != 'TimeSplitters.Launcher']
    if extras:
        raise SystemExit('Unexpected publish companions: ' + ', '.join(extras))
    binary = package / 'TimeSplitters.Launcher'
    shutil.copyfile(publish / binary.name, binary)
    binary.chmod(0o755)
    version = re.search(r'Value = "([^"]+)"', (ROOT / 'tools/launcher/LauncherVersion.cs').read_text())[1]
    support = package / 'Support/Launcher'
    licenses = support / 'Licenses'
    licenses.mkdir(parents=True, exist_ok=True)
    # Reuse the repository's complete Avalonia notice, plus runtime notices from
    # the verified SDK and locked package graph.
    for name in ('LICENSE.txt', 'ThirdPartyNotices.txt'):
        shutil.copyfile(sdk.parent / name, licenses / ('dotnet-' + name))
    shutil.copyfile(ROOT / 'LICENSES/Avalonia.txt', licenses / 'Avalonia.txt')
    assets = json.loads((output / 'obj/project.assets.json').read_text())
    for name, metadata in assets.get('libraries', {}).items():
        for base in assets.get('packageFolders', {}):
            for notice in (Path(base) / metadata.get('path', '')).glob('*.txt'):
                if any(word in notice.name.lower() for word in ('license', 'notice', 'copying')):
                    shutil.copyfile(notice, licenses / (name.replace('/', '-') + '-' + notice.name))
    (support / 'application.json').write_text(json.dumps({'platform': 'linux-x64', 'version': version}) + '\n')
    files = {p.relative_to(package).as_posix(): digest(p) for p in package.rglob('*') if p.is_file() and p.name != 'manifest.json'}
    manifest = support / 'manifest.json'
    manifest.write_text(json.dumps({'schema': 1, 'version': version, 'files': files}, indent=2) + '\n')
    sources = [ROOT / 'tools/build/build-linux-launcher.py', ROOT / 'config/build/linux-launcher-sdk.json',
               *sorted((ROOT / 'tools/launcher').rglob('*.cs')), *sorted((ROOT / 'tools/launcher').glob('*.xaml')),
               ROOT / 'tools/launcher/linux/LinuxLauncher.csproj', ROOT / 'tools/launcher/linux/packages.lock.json',
               ROOT / 'tools/launcher/macos/generate_layout.py']
    receipt = {'source_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
               'working_tree_changes': subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True),
               'source_sha256': {p.relative_to(ROOT).as_posix(): digest(p) for p in sources},
               'sdk': pin, 'files': files}
    (output / 'BUILD-launcher.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if args.test:
        for mode in ('--test', '--test-ui'):
            subprocess.run([str(binary), mode], check=True, cwd=ROOT, env=env)
    if args.package:
        archive = output / f'TimeSplitters-Launcher-v{version}-linux-x64.zip'
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as zip:
            for name in [*files, 'Support/Launcher/manifest.json']:
                zip.write(package / name, name)
        checksum = digest(archive)
        archive.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive.name}\n')
        if args.test:
            subprocess.run([str(binary), '--verify-package', str(archive), version], check=True, cwd=ROOT, env=env)
        (output / 'launcher-update-linux-x64.json').write_text(json.dumps({
            'schema': 1, 'version': version, 'asset_name': archive.name,
            'size': archive.stat().st_size, 'sha256': checksum}, indent=2) + '\n')
    print(binary)


if __name__ == '__main__':
    main()
