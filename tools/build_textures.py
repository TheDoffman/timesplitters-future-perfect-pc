"""Resolve one audited texture pack for source builds and distribution ZIPs.

Git tracks the release manifest, not the artwork. A valid local artist pack wins;
otherwise install the pinned release once. Never replace a nonempty local pack.
"""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import shutil
import stat
import urllib.request
import urllib.error
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('texture_release', ROOT/'tools/texture-release.py')
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)
RECEIPT = '.build-textures.json'


def regular(path):
    if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
        raise ValueError(f'Linked texture path is not supported: {path}')


def fetch(url, expected_size):
    # One bounded part in RAM; no retained ZIP or full staging copy on disk.
    request = urllib.request.Request(url, headers={'User-Agent': 'TSFP-source-build'})
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            if not response.geturl().startswith('https://'):
                raise ValueError('Texture download redirected away from HTTPS')
            content = response.read(expected_size + 1)
    except urllib.error.HTTPError as error:
        raise ValueError(f'Pinned texture release unavailable (HTTP {error.code}); publish the manifest\'s release or use an existing audited local pack. For a lightweight build use -SkipTextures (Windows) or --without-textures (Mac).') from error
    if len(content) != expected_size:
        raise ValueError('Texture download size mismatch')
    return content


def inspect_part(content, seen, remaining_bytes):
    """Validate paths and expansion limits before extracting any member."""
    archive = zipfile.ZipFile(io.BytesIO(content))
    rows = archive.infolist()
    if not rows or len(rows) > 40000:
        raise ValueError('Invalid texture part entry count')
    total = 0
    names = set()
    for item in rows:
        name = item.filename
        mode = item.external_attr >> 16
        if (not re.fullmatch(r'[0-9a-f]{16}\.(dds|json)', name)
                or name in seen or name in names
                or stat.S_IFMT(mode) not in (0, stat.S_IFREG)
                or item.flag_bits & 1):
            raise ValueError('Unsafe or duplicate texture archive member')
        limit = 65536 if name.endswith('.json') else 24*1024**2
        if not 0 < item.file_size <= limit:
            raise ValueError('Oversized texture member')
        names.add(name)
        total += item.file_size
    if total > remaining_bytes or total > RELEASE.RAW_PART_LIMIT:
        raise ValueError('Texture archive exceeds installed size budget')
    return archive, rows, total


def ensure_pack(root=ROOT, pack=None, manifest_path=None, downloader=fetch):
    root = Path(root)
    pack = Path(pack) if pack is not None else root/'private/texture-remaster/pack'
    manifest_path = Path(manifest_path) if manifest_path else ROOT/'config/texture-pack.json'
    # Check before resolving so a directory link cannot hide its identity.
    for parent in (pack, *pack.parents):
        regular(parent)
    receipt = pack.parent/RECEIPT
    lock = pack.parent/'.build-textures.lock'
    pack.parent.mkdir(parents=True, exist_ok=True)
    regular(lock)
    try:
        guard = lock.open('x')
    except FileExistsError as error:
        raise ValueError('Texture setup is already running or was interrupted; inspect .build-textures.lock before retrying') from error
    created = []
    try:
        if pack.exists() and any(pack.iterdir()):
            records, size = RELEASE.audit(pack)
            identity = {'source': 'local', 'texture_count': len(records), 'installed_bytes': size}
            if receipt.is_file():
                regular(receipt)
                saved = json.loads(receipt.read_text(encoding='utf-8'))
                hashes = {p.name: sha for p, _, sha in records}
                if saved.get('dds_sha256') == hashes:
                    identity.update(source='release', version=saved['version'])
            return pack, records, identity
        manifest = RELEASE.validate_manifest(json.loads(manifest_path.read_text(encoding='utf-8')))
        if shutil.disk_usage(pack.parent).free < manifest['installed_bytes'] + 64*1024**2:
            raise ValueError('Not enough free disk space for the texture pack')
        pack.mkdir(exist_ok=True)
        seen, total = set(), 0
        base = f'https://github.com/{RELEASE.REPO}/releases/download/textures-v{manifest["version"]}/'
        for part in manifest['parts']:
            print(f'Installing textures: {part["name"]}', flush=True)
            content = downloader(base+part['name'], part['size'])
            if len(content) != part['size'] or hashlib.sha256(content).hexdigest() != part['sha256']:
                raise ValueError('Texture part checksum/size mismatch')
            archive, rows, amount = inspect_part(content, seen, manifest['installed_bytes']-total)
            with archive:
                for item in rows:
                    target = pack/item.filename
                    # Exclusive create prevents overwriting concurrent/unrelated files.
                    with target.open('xb') as output:
                        created.append(target)
                        with archive.open(item) as source:
                            shutil.copyfileobj(source, output, 65536)
                    seen.add(item.filename)
            total += amount
        records, size = RELEASE.audit(pack)
        if total != manifest['installed_bytes'] or size != total or len(records) != manifest['texture_count']:
            raise ValueError('Installed texture pack does not match the manifest')
        regular(receipt)
        saved = {'version': manifest['version'], 'dds_sha256': {p.name: sha for p, _, sha in records}}
        receipt.write_text(json.dumps(saved, separators=(',', ':'))+'\n', encoding='utf-8')
        return pack, records, {'source': 'release', 'version': manifest['version'],
                               'texture_count': len(records), 'installed_bytes': size}
    except BaseException:
        # Only files exclusively created by this attempt; never recurse or touch
        # a preexisting artist pack, saves, settings or a different resolved root.
        for path in reversed(created):
            regular(path)
            if path.parent.resolve() != pack.resolve():
                raise ValueError('Refusing unexpected texture cleanup path')
            path.unlink(missing_ok=True)
        raise
    finally:
        guard.close()
        lock.unlink()


def zip_entries(records):
    """Validated public metadata; source paths/prompts never enter a package."""
    for path, metadata, sha in records:
        yield 'Textures/'+path.name, path, sha
        yield 'Textures/'+path.with_suffix('.json').name, metadata, hashlib.sha256(metadata).hexdigest()


def write_zip(archive, prefix, records):
    for name, source, expected in zip_entries(records):
        digest = hashlib.sha256()
        with archive.open(prefix+name, 'w', force_zip64=True) as output:
            with (source.open('rb') if isinstance(source, Path) else io.BytesIO(source)) as stream:
                while block := stream.read(65536):
                    output.write(block)
                    digest.update(block)
        if digest.hexdigest() != expected:
            raise ValueError('Texture changed during packaging')


def verify_download(pack, manifest_path, downloader=fetch):
    """Read back every published part against the local audit, without extracting."""
    records, total = RELEASE.audit(Path(pack))
    expected = {name.removeprefix('Textures/'): sha for name, _, sha in zip_entries(records)}
    manifest = RELEASE.validate_manifest(json.loads(Path(manifest_path).read_text(encoding='utf-8')))
    if len(records) != manifest['texture_count'] or total != manifest['installed_bytes']:
        raise ValueError('Local pack does not match the published size/count')
    seen, expanded, downloaded = set(), 0, 0
    base = f'https://github.com/{RELEASE.REPO}/releases/download/textures-v{manifest["version"]}/'
    for part in manifest['parts']:
        content = downloader(base+part['name'], part['size'])
        if len(content) != part['size'] or hashlib.sha256(content).hexdigest() != part['sha256']:
            raise ValueError('Published texture part checksum/size mismatch')
        archive, rows, amount = inspect_part(content, seen, total-expanded)
        with archive:
            for item in rows:
                with archive.open(item) as stream:
                    sha = hashlib.file_digest(stream, 'sha256').hexdigest()
                if expected.get(item.filename) != sha:
                    raise ValueError('Published member differs from local audit: '+item.filename)
                seen.add(item.filename)
        expanded += amount
        downloaded += len(content)
        print('Verified public download: '+part['name'], flush=True)
    if seen != expected.keys() or expanded != total:
        raise ValueError('Published pack is incomplete')
    return {'version': manifest['version'], 'texture_count': len(records),
            'download_bytes': downloaded, 'installed_bytes': total}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-root', type=Path, default=ROOT)
    parser.add_argument('--pack', type=Path)
    parser.add_argument('--verify-download', action='store_true', help='Read back published parts against the existing local pack, without copying or installing')
    args = parser.parse_args()
    if args.verify_download:
        result = verify_download(args.pack or args.runtime_root/'private/texture-remaster/pack', ROOT/'config/texture-pack.json')
        print(json.dumps(result, indent=2))
        return
    path, _, identity = ensure_pack(args.runtime_root, args.pack)
    print(json.dumps({'pack': str(path), **identity}, indent=2))


if __name__ == '__main__':
    main()
