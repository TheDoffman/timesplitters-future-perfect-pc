"""Parameterized source selection follows folders without ambiguous fallbacks."""
from pathlib import Path

import pytest

from project_paths import source_file


def test_grouped_and_flat_sources_resolve_without_changing_bytes(tmp_path):
    nested = tmp_path / 'tests/input/controls.c'
    nested.parent.mkdir(parents=True)
    nested.write_bytes(b'fixture')
    assert source_file(tmp_path, 'tests', 'controls.c') == nested
    assert source_file(tmp_path, 'tests', 'input/controls.c') == nested
    flat = tmp_path / 'tests/controls.c'
    nested.rename(flat)
    assert source_file(tmp_path, 'tests', 'controls.c') == flat
    assert flat.read_bytes() == b'fixture'


def test_duplicate_or_missing_dynamic_source_is_an_error(tmp_path):
    for group in ('input', 'gameplay'):
        target = tmp_path / 'tests' / group / 'same.c'
        target.parent.mkdir(parents=True)
        target.write_bytes(b'fixture')
    with pytest.raises(FileNotFoundError, match='found 2'):
        source_file(tmp_path, 'tests', 'same.c')
    with pytest.raises(FileNotFoundError, match='found 0'):
        source_file(tmp_path, 'tests', 'missing.c')


def test_current_repository_paths_are_grouped():
    root = Path(__file__).resolve().parents[2]
    assert source_file(root, 'tools', 'translation_integer.py') == root / 'tools/compiler/translation_integer.py'
    assert source_file(root, 'src', 'pc_controls.c') == root / 'src/input/pc_controls.c'
    assert source_file(root, 'tests', 'native_timing.c') == root / 'tests/gameplay/native_timing.c'
