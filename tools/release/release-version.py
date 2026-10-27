"""Prepare an explicitly chosen game version; never publish or create a tag."""

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
from pathlib import Path
from release_channels import identity


def set_version(root, version):
    if version.startswith('v'):
        raise ValueError('Use the version number without the v prefix.')
    channel = identity(version, publishing=True)
    (Path(root)/'VERSION').write_text(version+'\n', encoding='utf-8', newline='\n')
    return channel


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version', help='X.Y.Z, X.Y.Z-beta.N or X.Y.Z-story-coop.N')
    args = parser.parse_args()
    try:
        channel = set_version(_layout_root, args.version)
    except ValueError as error:
        parser.error(str(error))
    print(f"Prepared VERSION {args.version} ({channel['name']}). Review and commit it before publishing.")


if __name__ == '__main__':
    main()
