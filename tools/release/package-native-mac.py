#!/usr/bin/env python3
"""Assemble a relocatable Apple Silicon game release, without proprietary data."""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
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
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import urllib.request
import zipfile

import vulkan_sdk
from release_channels import identity as release_identity
from build_textures import ensure_pack, optional_identity, zip_entries, write_zip, write_source_files
from bundle_online_macos import bundle as bundle_online

ROOT = _layout_root
GAME = 'Bin/futureperfect_recomp'
APP = 'TimeSplitters Launcher.app'


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def run(*args):
    return subprocess.check_output([str(a) for a in args], text=True).strip()


def dependencies(path):
    return [line.strip().split(' (', 1)[0] for line in
            run('otool', '-arch', 'arm64', '-L', path).splitlines()[1:]
            if line.startswith(('\t', ' '))]


def system_library(name):
    return name.startswith(('/usr/lib/', '/System/Library/'))


def minimum_macos(paths):
    versions = []
    for path in paths:
        text = run('otool', '-arch', 'arm64', '-l', path)
        match = re.search(r'\bminos\s+(\d+(?:\.\d+)+)', text)
        if not match:
            match = re.search(r'LC_VERSION_MIN_MACOSX\s+cmdsize\s+\d+\s+version\s+(\d+(?:\.\d+)+)', text)
        if not match:
            raise ValueError('Missing deployment target in Mach-O: '+str(path))
        versions.append(tuple(int(part) for part in match[1].split('.')))
    return '.'.join(str(part) for part in max(versions))


def prepare_licenses(directory):
    pins = json.loads((ROOT/'config/build/native-mac-runtime-licenses.json').read_text())
    directory.mkdir(parents=True, exist_ok=True)
    for name, pin in pins.items():
        path = directory/name
        if not path.exists():
            with urllib.request.urlopen(pin['url'], timeout=30) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != pin['sha256']:
                raise ValueError('Downloaded license differs from its pin: '+name)
            with path.open('xb') as output:
                output.write(data)
        if path.is_symlink() or digest(path) != pin['sha256']:
            raise ValueError('Runtime license differs from its pin: '+name)


def bundle_libraries(executable, stage, sdk):
    """Copy the complete non-system dependency closure and rewrite install names."""
    pending = [executable, sdk/'lib/libvulkan.1.dylib',
               sdk/'lib/libMoltenVK.dylib', sdk/'lib/libdxcompiler.dylib']
    origins = {}
    records = {}
    while pending:
        source = pending.pop(0)
        if source.resolve() == executable.resolve():
            target = stage/GAME
        else:
            name = source.name
            if name in origins:
                if origins[name] != source.resolve():
                    raise ValueError('Conflicting runtime library basename: '+name)
                continue
            origins[name] = source.resolve()
            target = stage/'Bin'/name
            shutil.copy2(source, target)
            target.chmod(target.stat().st_mode | 0o200)
            records[name] = dict(original_sha256=digest(source), source=str(source))
        if 'arm64' not in run('lipo', '-archs', target).split():
            raise ValueError('Runtime dependency lacks arm64: '+str(source))
        if target.suffix == '.dylib':
            subprocess.run(['install_name_tool', '-id', '@loader_path/'+target.name, str(target)], check=True)
        for dependency in dependencies(source):
            if system_library(dependency):
                continue
            name = Path(dependency).name
            # A dylib's own LC_ID_DYLIB is printed by otool too.
            if name == source.name and source.resolve() != executable.resolve():
                continue
            if dependency.startswith('@'):
                candidates = [source.parent/name, sdk/'lib'/name]
                resolved = next((p for p in candidates if p.is_file()), None)
            elif dependency.startswith('/opt/homebrew/'):
                resolved = Path(dependency)
            else:
                raise ValueError('Unresolved runtime dependency: '+dependency)
            if resolved is None or not resolved.is_file():
                raise ValueError('Missing runtime dependency: '+dependency)
            pending.append(resolved)
            subprocess.run(['install_name_tool', '-change', dependency,
                            '@loader_path/'+name, str(target)], check=True)
    for target in [stage/GAME, *sorted((stage/'Bin').glob('*.dylib'))]:
        for dependency in dependencies(target):
            if not system_library(dependency):
                if not dependency.startswith('@loader_path/') or not (target.parent/Path(dependency).name).is_file():
                    raise ValueError('Package retains an external dependency: '+dependency)
        subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(target)], check=True)
        subprocess.run(['codesign', '--verify', '--strict', str(target)], check=True)
    return records


def verify_payload(stage, manifest, texture_rows=()):
    textures = {name: sha for name, _, sha in zip_entries(texture_rows)}
    required = {GAME, 'Launch.command', APP+'/Contents/MacOS/TimeSplitters.Launcher',
                'Settings/pc-controls.json', 'Bin/libvulkan.1.dylib',
                'Bin/libMoltenVK.dylib', 'Bin/libdxcompiler.dylib',
                'Support/Vulkan/MoltenVK_icd.json', 'Source/pc-port-source.zip',
                'Support/Python/bin/python3', 'Support/Python/bin/python-real',
                'Support/Online/online.py', 'Support/Online/online_platform.py',
                'Support/Online/network-identity.json', 'Support/Online/network-build.json',
                'Support/Online/multiplayer.png'}
    if not required <= manifest['files'].keys():
        raise ValueError('Incomplete Mac runtime package')
    for name, expected in manifest['files'].items():
        if name in textures:
            if expected != textures[name]:
                raise ValueError('Texture differs from audited pack: '+name)
            continue
        path = Path(name)
        if (path.is_absolute() or '..' in path.parts or path.parts[0] in
                ('Data', 'Saves', 'Textures', 'private') or
                path.suffix.lower() in ('.xbe', '.pak', '.iso', '.img', '.pdb') or
                (path.parts[0] == 'Settings' and name != 'Settings/pc-controls.json')):
            raise ValueError('Protected file in package: '+name)
        if (stage/path).is_symlink() or digest(stage/path) != expected:
            raise ValueError('Package file differs from manifest: '+name)
    actual = {p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()}
    if not textures.keys() <= manifest['files'].keys():
        raise ValueError('Texture files missing from package manifest')
    if actual != (set(manifest['files']) - textures.keys()) | {'BUILD.json'}:
        raise ValueError('Unlisted files in Mac runtime package')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'build/releases')
    parser.add_argument('--launcher', type=Path, default=ROOT/'build/macos-launcher')
    parser.add_argument('--with-textures', action='store_true', help='Explicitly bundle the audited enhanced pack (offline distribution)')
    parser.add_argument('--without-textures', action='store_true', help='Make a lightweight package without enhanced textures')
    parser.add_argument('--texture-pack', type=Path, help='Use this audited compiled pack instead of the committed repository pack')
    parser.add_argument('--licenses', type=Path, default=ROOT/'external/native-mac-licenses',
                        help='Directory containing verified Vulkan/DXC third-party notices')
    args = parser.parse_args()
    try:
        release_identity(args.version, publishing=True)
    except ValueError as error:
        parser.error(str(error))
    if args.without_textures and (args.texture_pack or args.with_textures):
        parser.error('--texture-pack cannot be combined with --without-textures')
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Package the native game on its Apple Silicon build host.')
    if not re.fullmatch(r'v\d+\.\d+\.\d+(?:-[\w.-]+)?', args.version):
        parser.error('Use a release tag such as v0.3.1-alpha.1.')
    if shutil.disk_usage(ROOT).free < 1024**3:
        parser.error('Packaging requires at least 1 GiB free.')
    native = json.loads((ROOT/'build/macos/BUILD.json').read_text())
    executable = ROOT/'build/macos/futureperfect_recomp'
    if native['executable_sha256'] != digest(executable):
        parser.error('Native executable differs from BUILD.json; rebuild first.')
    if native['renderer'] != 'vulkan' or native.get('vulkan_driver') != 'moltenvk':
        parser.error('This package path requires the shared Vulkan/MoltenVK build.')
    release_source = run('git', 'rev-parse', args.version+'^{commit}')
    if native['source_revision'] != release_source:
        parser.error('Game source must match the existing release tag.')
    sdk = vulkan_sdk.verify_install()
    texture_rows, textures = [], optional_identity(ROOT)
    if args.with_textures or args.texture_pack:
        _, texture_rows, textures = ensure_pack(ROOT, args.texture_pack)
    prepare_licenses(args.licenses)
    name = f'TimeSplitters-Future-Perfect-PC-{args.version}-macos-arm64'
    stage = args.output.resolve()/name
    stage.mkdir(parents=True, exist_ok=False)

    def copy(source, relative):
        target = stage/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)

    copy(executable, GAME)
    libraries = bundle_libraries(executable, stage, sdk)
    online = bundle_online(ROOT, stage, copy)
    minimum = minimum_macos([stage/GAME, *sorted((stage/'Bin').glob('*.dylib')),
        stage/'Support/Python/bin/python-real', *sorted((stage/'Support/Python/lib/native').iterdir()),
        *sorted((stage/'Support/Python').rglob('*.so'))])
    shutil.copytree(args.launcher/APP, stage/APP)
    copy(args.launcher/'Support/Launcher/manifest.json', 'Support/Launcher/manifest.json')
    copy(args.launcher/'BUILD-launcher.json', 'Support/Launcher/BUILD-launcher.json')
    copy(ROOT/'config/pc-controls.default.json', 'Settings/pc-controls.json')
    copy(ROOT/'config/pc-discord.default.ini', 'Support/Discord/pc-discord.default.ini')
    copy(ROOT/'docs/players/discord-presence.md', 'Support/Discord/README.txt')
    icd = json.loads((sdk/'share/vulkan/icd.d/MoltenVK_icd.json').read_text())
    icd['ICD']['library_path'] = '../../Bin/libMoltenVK.dylib'
    (stage/'Support/Vulkan').mkdir(parents=True)
    (stage/'Support/Vulkan/MoltenVK_icd.json').write_text(json.dumps(icd, indent=2)+'\n')
    copy(ROOT/'config/build/vulkan-sdk.json', 'Support/Vulkan/sdk-pin.json')
    copy(ROOT/'config/build/native-mac-runtime-licenses.json', 'Support/Vulkan/license-pins.json')
    copy(sdk.parent/'VERSIONS.txt', 'Support/Vulkan/VERSIONS.txt')
    upstream = ROOT/'external/ts3FPrecomp'
    for source, target in [('LICENSE', 'Runtime-GPL-3.0.txt'),
                           ('LICENSE.upstream-MIT', 'Runtime-upstream-MIT.txt'),
                           ('NOTICE', 'Runtime-NOTICE.txt'),
                           ('LICENSES/LGPL-2.1.txt', 'Runtime-LGPL-2.1.txt'),
                           ('third_party/SDL/LICENSE.txt', 'SDL.txt')]:
        copy(upstream/source, 'Licenses/'+target)
    copy(Path('/opt/homebrew/opt/openssl@3/LICENSE.txt'), 'Licenses/OpenSSL.txt')
    copy(sdk.parent/'Licenses/LICENSE.txt', 'Licenses/Vulkan-SDK.txt')
    for filename in ('Vulkan-Loader.txt', 'MoltenVK.txt', 'SPIRV-Cross.txt',
                     'SPIRV-Tools.txt', 'DXC.txt', 'DXC-ThirdParty.txt'):
        copy(args.licenses/filename, 'Licenses/'+filename)
    copy(ROOT/'tools/launcher/GameMonogram.NOTICE.txt', 'Licenses/Game-icon-lettering.txt')
    (stage/'Launch.command').write_text('#!/bin/sh\nset -eu\nROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$ROOT/TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher" --root "$ROOT" "$@"\n')
    (stage/'Launch.command').chmod(0o755)
    (stage/GAME).chmod(0o755)
    (stage/'Source').mkdir()
    sources = sorted(set(run('git', 'ls-files', '-z').split('\0')) | set(native['source_sha256']))
    with zipfile.ZipFile(stage/'Source/pc-port-source.zip', 'x', zipfile.ZIP_DEFLATED) as archive:
        write_source_files(archive, ROOT, sources)
        archive.writestr('runtime-local.patch', subprocess.check_output(['git', '-C', str(upstream), 'diff', '--binary']))
    (stage/'Source/README.txt').write_text('Project source and the exact runtime patch are in pc-port-source.zip.\n'
        'Enhanced textures are an optional launcher download; source archives omit them.\n'
        'Source builds may use assets/textures; explicit offline bundles include Textures.\n'
        'Bundled runtime source: vendor/ts3FPrecomp in pc-port-source.zip; prepare with tools/build/prepare-runtime.py.\n'
        'Runtime origin (credit): https://github.com/nemonicicon/ts3FPrecomp/tree/'+native['runtime_revision']+'\n'
        'SDL: https://github.com/libsdl-org/SDL/tree/'+run('git','-C',upstream/'third_party/SDL','rev-parse','HEAD')+'\n'
        'Build with tools/build/build-native-mac.py; package with tools/release/package-native-mac.py.\n')
    (stage/'README.txt').write_text(f'''TimeSplitters: Future Perfect PC {args.version} - experimental Apple Silicon build

Requires an Apple Silicon Mac running macOS {minimum} or newer and your own supported
US Xbox game files. Extract the whole ZIP to a writable local folder, then open
TimeSplitters Launcher.app (or Launch.command). Import your ISO or extracted
default.xbe, pak and xmv with the launcher, then Play. No Homebrew, Python,
Vulkan SDK or development checkout is needed. Intel Macs are unsupported.

The executable and dependencies are ad-hoc signed, not notarized. macOS may
require System Settings > Privacy & Security > Open Anyway for the downloaded app.

WASD: move. Mouse: look/fire/aim. E/Enter: confirm. Escape: pause.
F2: controls/settings. F12: debug. F11: fullscreen. F9: FPS display.
Saves and settings stay in Saves and Settings. Keep Data, Saves, Settings and
Textures when upgrading. Enhanced textures are an optional launcher download.
Game data, personal settings and saves are not bundled.

Uses the shared experimental Xbox/Vulkan renderer with bundled MoltenVK.
Basic first-level gameplay has been checked previously; complete rendering,
full-campaign completion and complete mixed Windows/Mac match behavior remain unverified.
Online and System Link use the shared desktop multiplayer protocol. All players
need matching builds and game content. Online includes its own Python/Tk runtime.

Supported default.xbe SHA-256:
3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc

Source and issues: https://github.com/TheDoffman/timesplitters-future-perfect-pc
Exact native build and packaging provenance: BUILD.json and Support/Launcher.
Source and third-party notices: Source and Licenses.
''', encoding='utf-8')
    manifest = dict(auto_display_resolution=native.get("auto_display_resolution", False), version=args.version, platform='macos-arm64', source_commit=release_source,
        release_channel=release_identity(args.version),
        packaging_commit=run('git','rev-parse','HEAD'),
        packaging_changes=run('git','status','--porcelain'),
        renderer='vulkan', vulkan_driver='moltenvk', minimum_macos=minimum,
        signing='ad-hoc; not notarized', runtime_libraries=libraries, native_build=native, online=online,
        launcher_version=json.loads((stage/'Support/Launcher/manifest.json').read_text())['version'],
        files={p.relative_to(stage).as_posix(): digest(p) for p in sorted(stage.rglob('*')) if p.is_file()})
    manifest['textures'] = textures
    manifest['files'].update({name: sha for name, _, sha in zip_entries(texture_rows)})
    (stage/'BUILD.json').write_text(json.dumps(manifest, indent=2)+'\n')
    verify_payload(stage, manifest, texture_rows)
    launcher = stage/APP/'Contents/MacOS/TimeSplitters.Launcher'
    archive_path = stage.with_name(name+'.zip')
    with zipfile.ZipFile(archive_path, 'x', zipfile.ZIP_DEFLATED, compresslevel=1) as archive:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                archive.write(path, name+'/'+path.relative_to(stage).as_posix())
        write_zip(archive, name+'/', texture_rows)
    with zipfile.ZipFile(archive_path) as archive:
        if archive.testzip() is not None:
            raise ValueError('Corrupt release archive')
    subprocess.run([str(launcher), '--verify-game-package', str(archive_path), args.version], check=True, cwd=ROOT)
    checksum = digest(archive_path)
    archive_path.with_suffix('.zip.sha256').write_text(f'{checksum}  {archive_path.name}\n')
    shutil.copy2(stage/'BUILD.json', stage.with_name(name+'.BUILD.json'))
    print(json.dumps(dict(archive=str(archive_path), size=archive_path.stat().st_size, sha256=checksum), indent=2))


if __name__ == '__main__':
    main()
