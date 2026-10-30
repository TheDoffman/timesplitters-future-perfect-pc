"""Shared paths for direct development commands and their Python helpers.

Commands remain directly executable with Python. Internal helper module names
stay stable while their files live in responsibility folders.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
TOOL_GROUPS = ('compiler', 'build', 'testing', 'analysis', 'textures', 'network', 'release', 'ci')


def configure():
    for path in (ROOT / 'tools', *(ROOT / 'tools' / group for group in TOOL_GROUPS),
                 ROOT / 'tests/support'):
        value = str(path)
        if value not in sys.path:
            sys.path.append(value)


def source_file(root, area, name):
    """Resolve a dynamically selected fixture/helper by its unique basename.

    Literal paths should name the owning folder directly. This helper is for
    existing parameterized test matrices and packaging source lists.
    """
    root = Path(root)
    name = str(name)
    direct = root / area / name
    if direct.is_file():
        return direct
    if Path(name).name != name or area not in ('tests', 'tools', 'src'):
        return direct
    matches = sorted(p for p in (root / area).glob('*/' + name) if p.is_file())
    if len(matches) != 1:
        raise FileNotFoundError(f'Expected one {area} source named {name}, found {len(matches)}')
    return matches[0]
