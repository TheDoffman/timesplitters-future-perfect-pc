"""Publish opt-in repository DDS packs as bounded GitHub release assets.

Only keyed DDS and sanitized provenance metadata are uploaded. Source artwork,
original game files, absolute paths, saves and settings never enter the package.
"""

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
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import urllib.request
import zipfile

ROOT = _layout_root
REPO = 'TheDoffman/timesplitters-future-perfect-pc'
BUDGET = 6 * 1024**3
PART_LIMIT = 64 * 1024**2
RAW_PART_LIMIT = 48 * 1024**2
KEY = re.compile(r'[0-9a-f]{16}\Z')
VERSION = re.compile(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[a-z0-9.]+)?\Z')
SPEC = importlib.util.spec_from_file_location('launcher_release', ROOT/'tools/release/launcher-release.py')
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def validate_dds(path):
    size = path.stat().st_size
    if not 128 <= size <= 24*1024**2:
        raise ValueError(f'Invalid DDS size: {path.name}')
    with path.open('rb') as stream:
        w = struct.unpack('<32I', stream.read(128))
    width, height = w[4], w[3]
    if (w[0:2] != (0x20534444, 124) or w[19] != 32 or w[28] or w[6]
            or not 0 < width <= 4096 or not 0 < height <= 4096
            or width & (width-1) or height & (height-1)
            or w[7] != max(width, height).bit_length()):
        raise ValueError(f'Invalid DDS header or mip chain: {path.name}')
    if w[20] == 4 and w[21] in (0x31545844, 0x33545844, 0x35545844):
        block = 8 if w[21] == 0x31545844 else 16
    elif w[20:27] == (0x41, 0, 32, 0xff0000, 0xff00, 0xff, 0xff000000):
        block = 0
    else:
        raise ValueError(f'Unsupported DDS format: {path.name}')
    expected = 128
    for _ in range(w[7]):
        expected += ((width+3)//4)*((height+3)//4)*block if block else width*height*4
        width, height = max(1, width//2), max(1, height//2)
    if expected != size:
        raise ValueError(f'Invalid DDS payload: {path.name}')


def audit(pack):
    """Audit all content before creating a release; retain only small metadata."""
    records = []
    for path in sorted(pack.iterdir()):
        if path.is_symlink() or not path.is_file():
            raise ValueError('Pack must be a flat folder of regular DDS/JSON files')
        if not KEY.fullmatch(path.stem) or path.suffix not in ('.dds', '.json'):
            raise ValueError(f'Unexpected pack file: {path.name}')
        if path.suffix != '.dds':
            if not path.with_suffix('.dds').is_file():
                raise ValueError('Orphan texture metadata')
            continue
        validate_dds(path)
        meta_path = path.with_suffix('.json')
        if meta_path.stat().st_size > 65536:
            raise ValueError('Oversized texture metadata')
        meta = json.loads(meta_path.read_text(encoding='utf-8'))
        sha = digest(path)
        if meta.get('key') != path.stem or meta.get('sha256') != sha:
            raise ValueError(f'Texture metadata does not match: {path.name}')
        # Explicit allowlist strips source paths, private prompts and local notes.
        public = {k: meta[k] for k in ('key', 'source_size', 'output_size',
                  'resampled', 'format', 'channels', 'mips') if k in meta}
        public.update(bytes=path.stat().st_size, sha256=sha)
        encoded = (json.dumps(public, sort_keys=True, separators=(',', ':'))+'\n').encode()
        records.append((path, encoded, sha))
    total = sum(p.stat().st_size+len(m) for p, m, _ in records)
    if not records or len(records) > 20000 or total > BUDGET:
        raise ValueError('Pack is empty or exceeds the 6 GiB / 20,000 texture limits')
    return records, total


def groups(records):
    # Stable key buckets keep additions from shifting unrelated download parts.
    for prefix in '0123456789abcdef':
        batch, size = [], 0
        for record in records:
            if not record[0].stem.startswith(prefix):
                continue
            n = record[0].stat().st_size+len(record[1])
            if batch and size+n > RAW_PART_LIMIT:
                yield batch
                batch, size = [], 0
            batch.append(record)
            size += n
        if batch:
            yield batch


def file_index(records):
    files = {}
    for path, metadata, sha in records:
        files[path.name] = sha
        files[path.with_suffix('.json').name] = hashlib.sha256(metadata).hexdigest()
    return files


def archive(records, destination):
    """One deterministic part at a time; no staging tree or whole-pack ZIP."""
    with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6, allowZip64=False) as output:
        for path, metadata, expected in records:
            for name, source in ((path.name, path), (path.with_suffix('.json').name, metadata)):
                info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                with output.open(info, 'w') as target:
                    if isinstance(source, Path):
                        check = hashlib.sha256()
                        with source.open('rb') as stream:
                            while data := stream.read(65536):
                                check.update(data)
                                target.write(data)
                        if check.hexdigest() != expected:
                            raise ValueError('Source texture changed while packaging')
                    else:
                        target.write(source)
    if destination.stat().st_size > PART_LIMIT:
        raise ValueError('Texture download part exceeds 64 MiB')


def validate_manifest(value):
    if (value.get('schema') not in (1, 3) or not VERSION.fullmatch(value.get('version', ''))
            or not isinstance(value.get('texture_count'), int)
            or not 0 < value['texture_count'] <= 20000
            or not isinstance(value.get('installed_bytes'), int)
            or not 0 < value['installed_bytes'] <= BUDGET
            or not isinstance(value.get('parts'), list)
            or not 0 < len(value['parts']) <= 256):
        raise ValueError('Invalid texture manifest')
    names = set()
    files = set()
    for part in value['parts']:
        name = part.get('name', '')
        if (not re.fullmatch(r'textures-[0-9]{3}\.zip', name) or name in names
                or not re.fullmatch(r'[0-9a-f]{64}', part.get('sha256', ''))
                or not isinstance(part.get('size'), int) or not 0 < part['size'] <= PART_LIMIT):
            raise ValueError('Invalid texture part')
        names.add(name)
        if value['schema'] == 3:
            index = part.get('files')
            if (not isinstance(index, dict) or not 0 < len(index) <= 40000
                    or any(not re.fullmatch(r'[0-9a-f]{16}\.(dds|json)', key)
                           or not isinstance(sha, str) or not re.fullmatch(r'[0-9a-f]{64}', sha)
                           or key in files for key, sha in index.items())
                    or any(str(Path(key).with_suffix('.json' if key.endswith('.dds') else '.dds')) not in index for key in index)):
                raise ValueError('Invalid texture file index')
            files.update(index)
    if value['schema'] == 3 and len(files) != value['texture_count'] * 2:
        raise ValueError('Texture file count does not match manifest')
    return value


def credentials():
    if os.environ.get('GITHUB_TOKEN'):
        return os.environ['GITHUB_TOKEN']
    # Reuse the maintainer's configured Git credential helper, in memory only.
    result = subprocess.run(['git', 'credential', 'fill'],
                            input='protocol=https\nhost=github.com\n\n', text=True,
                            capture_output=True, cwd=ROOT,
                            env={**os.environ, 'GIT_TERMINAL_PROMPT': '0'}, check=False)
    fields = dict(line.split('=', 1) for line in result.stdout.splitlines() if '=' in line)
    if not fields.get('password'):
        raise ValueError('Sign in to GitHub for Git, or supply GITHUB_TOKEN')
    return fields['password']


def verify_remote(manifest, api):
    validate_manifest(manifest)
    release = api.call('GET', f'/repos/{REPO}/releases/tags/textures-v{manifest["version"]}')
    if not release or release['draft']:
        raise ValueError('The configured texture release is not published')
    assets = {a['name']: a for a in release['assets']}
    for part in manifest['parts']:
        a = assets.get(part['name'], {})
        if (a.get('state') != 'uploaded' or a.get('size') != part['size']
                or a.get('digest') != 'sha256:'+part['sha256']):
            raise ValueError('Published texture asset is missing or differs: '+part['name'])
    return release['html_url']


def publish(pack, version, output, work, api):
    if not VERSION.fullmatch(version):
        raise ValueError('Invalid pack version')
    records, total = audit(pack)
    work.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(work).free < 160*1024**2:
        raise ValueError('At least 160 MiB of working space is required')
    base = f'/repos/{REPO}'
    tag = 'textures-v'+version
    release = api.call('GET', base+'/releases/tags/'+tag)
    if not release:
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        release = api.call('POST', base+'/releases', {
            'tag_name': tag, 'target_commitish': revision,
            'name': 'Optional 4K textures v'+version, 'draft': True,
            'prerelease': '-' in version, 'make_latest': 'false',
            'body': ('Optional texture alpha for TimeSplitters: Future Perfect. '
                     'Use the launcher’s Game settings → Graphics & resolution → '
                     'Download / update textures. The launcher downloads and installs '
                     'these parts automatically; do not concatenate the ZIPs.\n\n'
                     f'{len(records):,} replacements, up to 4K for selected surfaces. '
                     'This is an incomplete modern reinterpretation, not a full-game remaster. '
                     'Generated source artwork is resampled where recorded in the sanitized metadata. '
                     'Scene fidelity is not fully validated. Original textures remain the fallback.\n\n'
                     'No ISO, game executable, original game archives, personal settings or saves are included.')
        })
    assets = {a['name']: a for a in release['assets']}
    manifest = dict(schema=3, delivery='optional', published=True, version=version, texture_count=len(records),
                    installed_bytes=total, parts=[])
    for i, batch in enumerate(groups(records), 1):
        name = f'textures-{i:03}.zip'
        part = work/name
        try:
            archive(batch, part)
            record = dict(name=name, size=part.stat().st_size, sha256=digest(part), files=file_index(batch))
            found = assets.get(name)
            if found and (found.get('size') != record['size'] or found.get('digest') != 'sha256:'+record['sha256']):
                raise ValueError('This texture version already has different content. Choose a new version.')
            if not found:
                if not release['draft']:
                    raise ValueError('Published texture releases are immutable; choose a new version')
                found = api.call('POST', base+f'/releases/{release["id"]}/assets?name={name}', upload=part)
            if found.get('state') != 'uploaded' or found.get('digest') != 'sha256:'+record['sha256']:
                raise ValueError('GitHub did not verify the uploaded part')
            manifest['parts'].append(record)
            print(f'Verified {name}: {record["size"]/1024**2:.1f} MiB', flush=True)
        finally:
            part.unlink(missing_ok=True)
    validate_manifest(manifest)
    metadata = work/'texture-pack.json'
    metadata.write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    existing = assets.get(metadata.name)
    if existing and (existing.get('digest') != 'sha256:'+digest(metadata)):
        raise ValueError('This release already has a different manifest')
    if not existing:
        if not release['draft']:
            raise ValueError('Published release is missing its manifest; use a new version')
        api.call('POST', base+f'/releases/{release["id"]}/assets?name=texture-pack.json', upload=metadata)
    if release['draft']:
        api.call('PATCH', base+f'/releases/{release["id"]}', {'draft': False, 'make_latest': 'false'})
    url = verify_remote(manifest, api)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    metadata.unlink()
    return url


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('publish', 'verify'))
    parser.add_argument('--pack', type=Path, default=ROOT/'assets/textures')
    parser.add_argument('--version')
    parser.add_argument('--manifest', type=Path, default=ROOT/'config/textures/texture-download.json')
    parser.add_argument('--work', type=Path, default=ROOT/'build/texture-publication')
    parser.add_argument('--remote', action='store_true')
    args = parser.parse_args()
    if args.command == 'publish':
        if not args.version:
            parser.error('--version is required for publication')
        print(publish(args.pack, args.version, args.manifest, args.work, RELEASE.GitHub(credentials())))
    else:
        manifest = validate_manifest(json.loads(args.manifest.read_text(encoding='utf-8')))
        if args.remote:
            print(verify_remote(manifest, RELEASE.GitHub(credentials())))
        else:
            print(f'Texture manifest valid: {manifest["texture_count"]} replacements')


if __name__ == '__main__':
    main()
