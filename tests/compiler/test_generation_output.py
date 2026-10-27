"""Original generated-output persistence and declaration cases."""


from pathlib import Path
import json
import os
import sys
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from generation_output import write_if_changed, write_summary, print_stats, generate_header


def test_unchanged_crlf_preserves_exact_bytes_and_timestamp(tmp_path):
    path = tmp_path / 'generated.c'
    path.write_bytes(b'int a;\r\n')
    os.utime(path, ns=(1_000_000_000, 1_000_000_000))
    assert not write_if_changed(path, 'int a;\n')
    assert path.read_bytes() == b'int a;\r\n'
    assert path.stat().st_mtime_ns == 1_000_000_000
    assert write_if_changed(path, 'int b;\n')
    assert path.read_text() == 'int b;\n'


def test_failed_publication_preserves_existing_file_and_cleans_temporary(tmp_path, monkeypatch):
    path = tmp_path / 'generated.c'
    path.write_text('previous')
    def fail(*args):
        raise OSError('Simulated replace failure')
    monkeypatch.setattr(Path, 'replace', fail)
    with pytest.raises(OSError, match='replace failure'):
        write_if_changed(path, 'changed')
    assert path.read_text() == 'previous'
    assert list(tmp_path.iterdir()) == [path]


def test_nested_report_and_summary_counts(tmp_path, capsys):
    stats = {'audio': dict(total=3, translated=2, failed=1, total_lines=80),
             'other': dict(total=1, translated=1, failed=0, total_lines=5)}
    path = write_summary(stats, tmp_path / 'nested/report')
    assert json.loads(Path(path).read_text()) == stats
    print_stats(stats)
    assert 'Total: 3/4 functions; 1 failed; 85 lines' in capsys.readouterr().out


def test_header_signatures_and_comment_safety(tmp_path):
    path = tmp_path / 'header.h'
    functions = [(4, {'name': 'compute'}), (2, {}), (8, {'name': 'object_method'})]
    abi = {4: dict(estimated_params=2, return_hint='float_sse'),
           8: dict(estimated_params=1, calling_convention='thiscall', return_hint='int_zero')}
    assert generate_header(functions, path, abi, 'Title */\nunsafe') == path
    text = path.read_text()
    assert 'void sub_00000002(void);' in text
    assert 'float compute(uint32_t a1, uint32_t a2);' in text
    assert 'int object_method(void *this_ptr, uint32_t a1);' in text
    assert text.startswith('/* Title * / unsafe:')


@pytest.mark.parametrize('functions,abi', [([(1, {'name': 'broken-name'})], {}),
    ([(1, {'name': 'void'})], {}),
    ([(1, {'name': 'same'}), (2, {'name': 'same'})], {}),
    ([(1, {})], {1: {'estimated_params': True}}), ([(1, {})], {1: {'estimated_params': 257}})])
def test_invalid_header_preserves_old_output(tmp_path, functions, abi):
    path = tmp_path / 'header.h'
    path.write_text('old')
    with pytest.raises(ValueError):
        generate_header(functions, path, abi)
    assert path.read_text() == 'old'
