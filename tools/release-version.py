"""Prepare an explicitly chosen game version; never publish or create a tag."""
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
        channel = set_version(Path(__file__).resolve().parents[1], args.version)
    except ValueError as error:
        parser.error(str(error))
    print(f"Prepared VERSION {args.version} ({channel['name']}). Review and commit it before publishing.")


if __name__ == '__main__':
    main()
