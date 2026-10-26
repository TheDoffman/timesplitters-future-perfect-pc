"""Reconstruct the pinned runtime from tracked source, without network access.

Keep the original Git tree/commit identity so patch migration, submodule pins,
source packaging and build provenance work with both old and fresh checkouts.
Only the pinned commit is materialized; its parent history is intentionally absent.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = ROOT / 'vendor/ts3FPrecomp'


def git(root, *args, data=None):
    return subprocess.check_output(
        ['git', '-C', str(root), '-c', 'core.autocrlf=false', *args], input=data,
        stderr=subprocess.PIPE)


def object_id(kind, data):
    return hashlib.sha1(f'{kind} {len(data)}\0'.encode() + data).hexdigest()


def tree_id(entries):
    tree = {}
    for entry in entries:
        node = tree
        parts = entry['path'].split('/')
        for part in parts[:-1]:
            node = node.setdefault(part, {})
            if not isinstance(node, dict):
                raise ValueError('Conflicting runtime file/directory paths')
        if parts[-1] in node:
            raise ValueError('Conflicting runtime file/directory paths')
        node[parts[-1]] = (entry['mode'], entry['oid'])

    def digest(node):
        data = bytearray()
        for name, value in sorted(node.items(), key=lambda item:
                                  (item[0] + ('/' if isinstance(item[1], dict) else '')).encode()):
            mode, oid = ('40000', digest(value)) if isinstance(value, dict) else value
            data.extend(f'{mode} {name}\0'.encode() + bytes.fromhex(oid))
        return object_id('tree', bytes(data))
    return digest(tree)


def write_object(repository, kind, data):
    raw = f'{kind} {len(data)}\0'.encode() + data
    oid = hashlib.sha1(raw).hexdigest()
    path = repository / '.git/objects' / oid[:2] / oid[2:]
    path.parent.mkdir(exist_ok=True)
    if not path.exists():
        path.write_bytes(zlib.compress(raw))
    return oid


def read_snapshot(snapshot):
    snapshot = Path(snapshot)
    record = json.loads((snapshot / 'snapshot.json').read_text(encoding='utf-8'))
    if record.get('version') != 1:
        raise ValueError('Unsupported runtime snapshot version')
    commit = base64.b64decode(record['commit_base64'], validate=True)
    revision = record['revision']
    if object_id('commit', commit) != revision:
        raise ValueError('Runtime commit identity mismatch')
    if not commit.startswith(('tree ' + record['tree'] + '\n').encode()):
        raise ValueError('Runtime tree identity mismatch')
    entries = record['entries']
    seen = set()
    for entry in entries:
        name = entry['path']
        path = PurePosixPath(name)
        if (not name or path.is_absolute() or str(path) != name
                or any(part.casefold() in ('.', '..', '.git') for part in path.parts)
                or any(char in name for char in '\\:\0\r\n\t')
                or name.casefold() in seen):
            raise ValueError(f'Invalid runtime source path: {name!r}')
        seen.add(name.casefold())
        if entry['mode'] not in ('100644', '100755', '160000'):
            raise ValueError(f'Unsupported runtime source mode: {name}')
        if not re.fullmatch(r'[a-f0-9]{40}', entry['oid']):
            raise ValueError(f'Invalid runtime object identity: {name}')
        if entry['mode'] == '160000':
            continue
        source = snapshot / 'source' / name
        if source.is_symlink() or not source.resolve().is_relative_to((snapshot / 'source').resolve()):
            raise ValueError(f'Runtime source escapes snapshot: {name}')
        data = source.read_bytes()
        if (hashlib.sha256(data).hexdigest() != entry['sha256']
                or object_id('blob', data) != entry['oid']):
            raise ValueError(f'Runtime source hash mismatch: {name}')
    expected = {e['path'] for e in entries if e['mode'] != '160000'}
    actual = {p.relative_to(snapshot / 'source').as_posix()
              for p in (snapshot / 'source').rglob('*') if p.is_file()}
    if actual != expected:
        raise ValueError('Unexpected or missing files in runtime source snapshot')
    if tree_id(entries) != record['tree']:
        raise ValueError('Runtime source manifest does not reconstruct the pinned tree')
    return record, commit


def prepare(destination, snapshot=SNAPSHOT, expected_revision=None):
    record, commit = read_snapshot(snapshot)
    revision = record['revision']
    if expected_revision and revision != expected_revision:
        raise ValueError('Runtime snapshot differs from the shared pin')
    destination = Path(destination).absolute()
    if destination.is_symlink():
        raise ValueError('Runtime destination must not be a symbolic link')
    if destination.exists():
        # Reuse a real existing checkout without resetting patches, generated
        # code, dependencies or local edits. Never accept the enclosing repo.
        if not (destination / '.git').exists():
            raise ValueError('Runtime destination exists but is not a Git checkout; no changes made')
        actual = git(destination, 'rev-parse', 'HEAD').decode().strip()
        if actual != revision:
            raise ValueError('Runtime checkout differs from the shared pin; no changes made')
        if git(destination, 'rev-parse', 'HEAD^{tree}').decode().strip() != record['tree']:
            raise ValueError('Runtime checkout tree differs from the snapshot; no changes made')
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Same-volume staging makes publication an atomic rename. A failed import
    # never leaves a half-created runtime at the requested destination.
    stage = Path(tempfile.mkdtemp(prefix='.tsfp-runtime-', dir=destination.parent))
    try:
        git(stage, 'init', '-q', '--object-format=sha1')
        git(stage, 'config', 'core.autocrlf', 'false')
        index = bytearray()
        for entry in record['entries']:
            name, mode, oid = entry['path'], entry['mode'], entry['oid']
            if mode != '160000':
                data = (Path(snapshot) / 'source' / name).read_bytes()
                # Verify again when consuming bytes, not only before staging.
                if write_object(stage, 'blob', data) != oid:
                    raise ValueError(f'Runtime source changed during import: {name}')
                target = stage / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                target.chmod(0o755 if mode == '100755' else 0o644)
            else:
                (stage / name).mkdir(parents=True, exist_ok=True)
            index.extend(f'{mode} {oid}\t{name}\0'.encode())
        git(stage, 'update-index', '-z', '--index-info', data=bytes(index))
        if git(stage, 'write-tree').decode().strip() != record['tree']:
            raise ValueError('Reconstructed runtime tree differs from the pinned source')
        if write_object(stage, 'commit', commit) != revision:
            raise ValueError('Reconstructed runtime commit differs from the pin')
        (stage / '.git/shallow').write_text(revision + '\n', encoding='ascii')
        git(stage, 'update-ref', '--no-deref', 'HEAD', revision)
        # Direct dependency URLs in .gitmodules work without a remote. There
        # deliberately is no origin pointing at the former runtime repository.
        if destination.exists():
            raise ValueError('Runtime destination appeared during import; no changes made')
        stage.rename(destination)
        return True
    finally:
        if stage.exists():
            # Only this operation's disposable, resolved sibling is removed.
            if stage.resolve().parent != destination.parent.resolve() or not stage.name.startswith('.tsfp-runtime-'):
                raise RuntimeError('Unexpected runtime staging path')
            shutil.rmtree(stage)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, default=ROOT / 'external/ts3FPrecomp')
    parser.add_argument('--verify-only', action='store_true', help='Verify tracked source without creating a runtime')
    args = parser.parse_args()
    pin = re.search(r"\$revision = '([a-f0-9]{40})'", (ROOT / 'tsfp.ps1').read_text())[1]
    if args.verify_only:
        record, _ = read_snapshot(SNAPSHOT)
        if record['revision'] != pin:
            raise ValueError('Runtime snapshot differs from the shared pin')
        print(f'Verified bundled runtime {pin}')
    else:
        created = prepare(args.destination, expected_revision=pin)
        print(f'{"Prepared" if created else "Reused"} runtime {pin}: {args.destination}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'Runtime preparation failed: {error}') from error
