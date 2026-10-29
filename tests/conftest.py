"""Shared import paths for responsibility-based test folders."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from project_paths import configure

configure()

pytest_plugins = ['suite_timing']

# Parameterized fixtures share helpers across responsibility groups.
for folder in sorted(Path(__file__).parent.iterdir()):
    if folder.is_dir() and not folder.name.startswith('_'):
        sys.path.append(str(folder))

# Keep compiled fixtures together when the caller enables parallel testing.
from parallel_groups import pytest_collection_modifyitems
