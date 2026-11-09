"""Clean-room input checks use their declared fixture, preserving other state."""
import importlib.util
from pathlib import Path

import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('linux_clean_package',ROOT/'tools/testing/test-linux-clean-package.py')
clean=importlib.util.module_from_spec(spec);spec.loader.exec_module(clean)


def test_legacy_input_fixture_replaces_only_disposable_controls(tmp_path,monkeypatch):
    project=tmp_path/'project';fixture=project/'tests/fixtures/pc-controls.json'
    fixture.parent.mkdir(parents=True);fixture.write_bytes((ROOT/'tests/fixtures/pc-controls.json').read_bytes())
    monkeypatch.setattr(clean,'ROOT',project)
    home=tmp_path/'disposable home';config=home/'.config/timesplitters-future-perfect/pc-controls.json'
    config.parent.mkdir(parents=True);config.write_text('{"keyboard_preset":1}')
    unrelated=config.with_name('runtime.conf');unrelated.write_bytes(b'fullscreen = 1\n')
    assert clean.seed_input_fixture(home)==config
    assert config.read_bytes()==fixture.read_bytes()
    assert unrelated.read_bytes()==b'fullscreen = 1\n'


def test_input_fixture_refuses_linked_destination(tmp_path):
    home=tmp_path/'home';config=home/'.config/timesplitters-future-perfect/pc-controls.json'
    config.parent.mkdir(parents=True);outside=tmp_path/'preserve.json';outside.write_bytes(b'unchanged')
    try:config.symlink_to(outside)
    except OSError:pytest.skip('File symlinks unavailable on this host')
    with pytest.raises(ValueError,match='Linked'):
        clean.seed_input_fixture(home)
    assert outside.read_bytes()==b'unchanged'
