"""Verify/import the supported Xbox disc without the external recomp toolkit."""

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
import sys
from pathlib import Path
from disc_image import DiscError, DiscImage, extract


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    if sys.version_info < (3, 12):
        parser.error('Python 3.12 or newer is required')
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    if not args.verify_only and args.output is None:
        parser.error('--output is required for import')
    try:
        with DiscImage(args.iso) as image:
            if args.verify_only:
                image.verify_supported()
                result = {'verified': True, 'partition_offset': image.base,
                          'files': sum(not e.directory for e in image.entries)}
            else:
                result = extract(image, args.output)
        print(json.dumps(result, indent=2))
    except (DiscError, OSError) as error:
        parser.exit(1, f'Game import failed: {error}\n')


if __name__ == '__main__':
    main()
