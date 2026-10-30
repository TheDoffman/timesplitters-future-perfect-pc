"""Receipt invalidation contracts for the project analysis runner."""


import json
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import analysis_cache
from analysis_cache import AnalysisCache


def fixture(tmp_path, analysis=True):
    xbe = tmp_path / 'input.xbe'
    xbe.write_bytes(b'fixture executable bytes')
    metadata = tmp_path / 'analysis.json' if analysis else None
    if metadata:
        metadata.write_text('{}')
    output = tmp_path / 'output'
    output.mkdir()
    for name in AnalysisCache.REQUIRED:
        (output / name).write_text('[]')
    (output / 'asm').mkdir()
    (output / 'asm/code.asm').write_text('entry: ret\n')
    cache = AnalysisCache(output)
    cache.save(xbe, metadata, False, 1.25, ['code'], [0x1234], {0x1234})
    return cache, (xbe, metadata, False, ['code'], [0x1234], {0x1234})


def test_round_trip_and_order_independent_option_identity(tmp_path):
    cache, args = fixture(tmp_path)
    assert cache.is_valid(*args)
    assert cache.get_last_run_time() == 1.25
    assert cache._opts_key(False, ['b', 'a', 'a'], [2, 1, 1], {1}) == cache._opts_key(False, ['a', 'b'], [1, 2], [1])
    assert not list(cache.output_dir.glob('.disasm-receipt-*.tmp'))
    cache.invalidate()
    assert not cache.is_valid(*args) and cache.get_last_run_time() is None
    cache.invalidate()


@pytest.mark.parametrize('index,replacement', [(2, True), (3, ['other']), (4, [0x5678]), (5, set())])
def test_every_analysis_option_affects_validity(tmp_path, index, replacement):
    cache, args = fixture(tmp_path)
    changed = list(args)
    changed[index] = replacement
    assert not cache.is_valid(*changed)


@pytest.mark.parametrize('name', ['summary.json', 'functions.json', 'xrefs.json', 'strings.json', 'labels.json', 'asm/code.asm'])
def test_changed_or_missing_output_invalidates_receipt(tmp_path, name):
    cache, args = fixture(tmp_path)
    path = cache.output_dir / name
    original = path.read_bytes()
    path.write_bytes(original + b' ')
    assert not cache.is_valid(*args)
    path.write_bytes(original)
    assert cache.is_valid(*args)
    path.unlink()
    assert not cache.is_valid(*args)


def test_input_and_source_changes_invalidate_receipt(tmp_path, monkeypatch):
    cache, args = fixture(tmp_path)
    moved = tmp_path / 'another.xbe'
    moved.write_bytes(args[0].read_bytes())
    assert not cache.is_valid(moved, *args[1:])
    for path in args[:2]:
        original = path.read_bytes()
        path.write_bytes(original + b' ')
        assert not cache.is_valid(*args)
        path.write_bytes(original)
    assert cache.is_valid(*args)
    monkeypatch.setattr(analysis_cache, 'source_fingerprint', lambda: 'different source')
    assert not cache.is_valid(*args)


def test_absent_analysis_metadata_is_an_explicit_input(tmp_path):
    cache, args = fixture(tmp_path, analysis=False)
    assert cache.is_valid(*args)
    metadata = tmp_path / 'later.json'
    metadata.write_text('{}')
    assert not cache.is_valid(args[0], metadata, *args[2:])


@pytest.mark.parametrize('content', ['{', '[]', '{"version":1}', '{"version":2,"artifacts":[]}'])
def test_malformed_or_old_receipts_are_misses(tmp_path, content):
    cache, args = fixture(tmp_path)
    cache.cache_file.write_text(content)
    assert not cache.is_valid(*args)


@pytest.mark.parametrize('relative', ['../outside.json', '/absolute.json', 'C:/outside.json', 'asm\\outside.json'])
def test_receipt_cannot_name_files_outside_its_output(tmp_path, relative):
    cache, args = fixture(tmp_path)
    receipt = json.loads(cache.cache_file.read_text())
    receipt['artifacts'][relative] = 'invalid'
    cache.cache_file.write_text(json.dumps(receipt))
    assert not cache.is_valid(*args)


def test_failed_save_preserves_last_complete_receipt(tmp_path):
    cache, args = fixture(tmp_path)
    prior = cache.cache_file.read_bytes()
    (cache.output_dir / 'functions.json').unlink()
    with pytest.raises(FileNotFoundError):
        cache.save(args[0], args[1], False, 9)
    assert cache.cache_file.read_bytes() == prior
    assert not cache.is_valid(*args)
