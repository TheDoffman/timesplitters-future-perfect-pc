"""Stage audited compiled replacements in Git LFS, stripping private metadata.

Use --pointers-only on an art workstation to avoid a second full working copy.
The DDS bytes still enter the local LFS object store and upload with git push.
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
import json
from pathlib import Path
import shutil
import subprocess
from build_textures import ROOT, RELEASE, regular


def stage(source, root=ROOT, pointers_only=False):
    root = Path(root)
    for path in (source, *source.parents):
        regular(path)
    records, size = RELEASE.audit(source)
    target = root/'assets/textures'
    for path in (target, *target.parents):
        regular(path)
    target.mkdir(parents=True, exist_ok=True)
    expected = {p.stem+s for p, _, _ in records for s in ('.dds', '.json')}
    if any(p.name not in expected for p in target.iterdir()):
        raise ValueError('Repository contains extra files; review removed keys before staging')
    if shutil.disk_usage(target).free < size*(1 if pointers_only else 2)+64*1024**2:
        raise ValueError('Insufficient free space for Git LFS texture storage')
    subprocess.run(['git', 'lfs', 'install', '--local'], cwd=root, check=True)
    for i, (path, metadata, sha) in enumerate(records, 1):
        destination = target/path.name
        regular(destination)
        regular(destination.with_suffix('.json'))
        with path.open('rb') as stream:
            # The checkout may contain a small LFS pointer. Passing its filename
            # makes LFS use that file's size for the different DDS on stdin,
            # truncating the clean input. Read the complete stream without a hint.
            result = subprocess.run(['git', 'lfs', 'clean'],
                                    stdin=stream, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    cwd=root, check=True)
        expected_pointer = f'version https://git-lfs.github.com/spec/v1\noid sha256:{sha}\nsize {path.stat().st_size}\n'.encode()
        if result.stdout != expected_pointer:
            raise ValueError('Git LFS content hash differs from the audited texture')
        if pointers_only:
            destination.write_bytes(result.stdout)
        else:
            shutil.copyfile(path, destination)
        destination.with_suffix('.json').write_bytes(metadata)
        if i % 100 == 0 or i == len(records):
            print(f'Staged {i}/{len(records)} textures in Git LFS', flush=True)
    (root/'config/textures/texture-pack.json').write_text(json.dumps({
        'schema': 2, 'delivery': 'optional', 'path': 'assets/textures',
        'texture_count': len(records), 'installed_bytes': size}, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--pointers-only', action='store_true')
    args = parser.parse_args()
    stage(args.source, pointers_only=args.pointers_only)
