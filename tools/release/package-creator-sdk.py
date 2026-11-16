"""Package the creator source SDK without game assets, hosting code or credentials."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = '0.1.0'
FILES = (
    'LICENSE', 'docs/sdk/README.md', 'docs/sdk/API.md',
    'tools/mods/mod-creator.py', 'tools/mods/mod_creator_cli.py',
    'tools/mods/mod_creator.py', 'tools/mods/mod_package.py',
    'tools/mods/mod_lua_bundle.py', 'tools/mods/mod_wave_input.py',
    'tools/textures/texture-pack.py', 'tools/release/package-creator-sdk.py',
    'examples/mods/first-to-three/mod.json', 'examples/mods/first-to-three/scripts/main.lua',
    'examples/mods/modular-first-to-three/mod.json',
    'examples/mods/modular-first-to-three/scripts/main.lua',
    'examples/mods/modular-first-to-three/scripts/rules/settings.lua',
    'examples/mods/modular-first-to-three/scripts/rules/scores.lua',
    'examples/mods/audio-lab/scripts/main.lua',
)


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def package_sdk(root, output, *, require_clean=True):
    root = Path(root).resolve()
    output = Path(output).absolute()
    revision = git(root, 'rev-parse', 'HEAD')
    if not re.fullmatch('[0-9a-f]{40}', revision):
        raise ValueError('An exact Git source revision is required')
    changes = git(root, 'status', '--porcelain')
    if require_clean and changes:
        raise ValueError('Commit SDK source before making a release archive')
    tracked = set(git(root, 'ls-files', '-z').split('\0'))
    if require_clean and not set(FILES) <= tracked:
        raise ValueError('SDK inputs must all be committed source')
    checksum = output.with_suffix(output.suffix+'.sha256')
    if not output.parent.is_dir() or output.exists() or checksum.exists():
        raise ValueError('Choose a new ZIP filename in an existing output directory')
    if output.is_symlink() or checksum.is_symlink():
        raise ValueError('SDK output paths must not be links')
    payload = {}
    for name in FILES:
        source = root/name
        if (not source.is_file() or source.stat().st_size > 2*1024*1024 or
                any(p.is_symlink() or p.is_junction() for p in (source, *source.parents))):
            raise ValueError('Changed, linked or oversized SDK source: '+name)
        # Git text is canonical LF on every build host, including exported docs.
        target = Path(name).name if name.startswith('docs/sdk/') else name
        data = source.read_text(encoding='utf-8').replace('\r\n', '\n').encode('utf-8')
        if require_clean:
            committed=subprocess.check_output(['git','-C',str(root),'show',revision+':'+name])
            if data != committed.decode('utf-8').replace('\r\n','\n').encode('utf-8'):
                raise ValueError('SDK source differs from its committed bytes: '+name)
        payload[target] = data
    requirements=(root/'requirements.txt').read_text(encoding='utf-8')
    if require_clean:
        committed=subprocess.check_output(['git','-C',str(root),'show',revision+':requirements.txt'])
        if requirements != committed.decode('utf-8').replace('\r\n','\n'):
            raise ValueError('Dependency pins differ from their committed bytes')
    pins = requirements.splitlines()
    pillow = [line for line in pins if re.fullmatch(r'Pillow==[0-9]+\.[0-9]+\.[0-9]+', line)]
    if len(pillow) != 1:
        raise ValueError('The repository-pinned Pillow dependency is required')
    payload['requirements-sdk.txt'] = (pillow[0]+'\n').encode('ascii')
    manifest = dict(schema=1, sdk_version=VERSION, source_revision=revision,
        source_clean=not bool(changes), python_minimum='3.12', package_api='tsfp-mod-v1-draft',
        script_api=1, audio_api=1, files={name:dict(size=len(data),sha256=hashlib.sha256(data).hexdigest())
                                     for name, data in sorted(payload.items())})
    payload['SDK.json'] = (json.dumps(manifest,sort_keys=True,indent=2)+'\n').encode('utf-8')
    if require_clean and (git(root,'rev-parse','HEAD') != revision or git(root,'status','--porcelain')):
        raise ValueError('SDK source changed during export; commit before retrying')
    prefix='TimeSplitters-Mod-Creator-SDK-v'+VERSION+'/'
    with output.open('xb') as stream:
        # A small source bundle needs no compressor; stored entries also keep
        # its identity independent of each build host's zlib version.
        with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_STORED) as archive:
            for name, data in sorted(payload.items()):
                info = zipfile.ZipInfo(prefix+name, (2020,1,1,0,0,0))
                info.create_system=3; info.compress_type=zipfile.ZIP_STORED
                info.external_attr=(stat.S_IFREG|0o644)<<16
                archive.writestr(info,data)
    digest=hashlib.sha256(output.read_bytes()).hexdigest()
    with checksum.open('x',encoding='ascii',newline='\n') as stream:
        stream.write(digest+'  '+output.name+'\n')
    return dict(archive=str(output),sha256=digest,source_revision=revision,
                files=len(payload),bytes=output.stat().st_size)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:
        print(json.dumps(package_sdk(ROOT,args.output),indent=2))
        return 0
    except (OSError,ValueError,subprocess.CalledProcessError) as exc:
        parser.exit(2,'Creator SDK: '+str(exc)+'\n')


if __name__=='__main__':
    raise SystemExit(main())
