"""Audit repository textures and stream them into game packages without staging copies."""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('texture_release', ROOT/'tools/texture-release.py')
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)


def regular(path):
    if path.is_symlink() or (hasattr(path, 'is_junction') and path.is_junction()):
        raise ValueError(f'Linked texture path is not supported: {path}')


def ensure_pack(root=ROOT, pack=None):
    """Use the committed pack by default; artist overrides are always explicit."""
    pack = Path(pack) if pack is not None else Path(root)/'assets/textures'
    for parent in (pack, *pack.parents):
        regular(parent)
    if not pack.is_dir():
        raise ValueError('Repository texture pack is missing. Run git lfs install and git lfs pull, or explicitly select --without-textures / -SkipTextures.')
    for path in pack.glob('*.dds'):
        regular(path)
        with path.open('rb') as stream:
            if stream.read(43).startswith(b'version https://git-lfs.github.com/spec/v1'):
                raise ValueError('Texture files are Git LFS pointers. Run git lfs pull before building or packaging.')
    records, size = RELEASE.audit(pack)
    identity = {'source': 'repository' if pack == Path(root)/'assets/textures' else 'local',
                'texture_count': len(records), 'installed_bytes': size}
    return pack, records, identity


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-root', type=Path, default=ROOT)
    parser.add_argument('--pack', type=Path)
    args = parser.parse_args()
    path, _, identity = ensure_pack(args.runtime_root, args.pack)
    print(json.dumps({'pack': str(path), **identity}, indent=2))


if __name__ == '__main__':
    main()
