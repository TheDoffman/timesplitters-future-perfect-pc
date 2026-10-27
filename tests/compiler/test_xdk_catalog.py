"""Original protocol and failure fixtures for the project symbol adapter."""

from project_paths import source_file as repo_file

import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import xdk_catalog as catalog
from test_xbe_image import fixture


def test_protocol_keeps_register_stack_width_and_alias_names():
    record = catalog.parse_line('SAMPLE__FUN__fastcall__Thing_Do(ecx owner, psh2 time, psh flags) = 0x00012340')
    assert record == dict(library='SAMPLE', kind='function', address=0x12340, name='Thing_Do',
                          call='fastcall', stack_bytes=12, params=[dict(type='ecx', name='owner'),
                          dict(type='psh2', name='time'), dict(type='psh', name='flags')])
    assert catalog.parse_line('LIB__VAR__Device__field = 0x00000010')['name'] == 'Device__field'
    assert catalog.parse_line('LIB__UNK__Unresolved = 0x1234')['kind'] == 'unknown'
    assert catalog.parse_line('LIB__Plain = 0x1234')['name'] == 'Plain'
    assert catalog.parse_line('LIB__FUN__stdcall__NoArgs() = 0x1234')['stack_bytes'] == 0


@pytest.mark.parametrize('line', ['notice', 'LIB__VAR__X = 0x100000000', 'LIB__VAR__X = 0x-1',
    'TOOLONGLIB__VAR__X = 0x1', 'LIB__FUN__stdcall__broken = 0x1', 'LIB__VAR__ = 0x1'])
def test_protocol_rejects_bad_records(line):
    assert catalog.parse_line(line) is None
    assert catalog.parse_output(line + '\nLIB__VAR__Good = 0x1')[0]['name'] == 'Good'


def test_cli_selection_is_exact_and_release_first(tmp_path, monkeypatch):
    monkeypatch.setattr(catalog, 'REPO', tmp_path)
    monkeypatch.delenv('XBSDB_CLI', raising=False)
    base = tmp_path / 'third_party/XbSymbolDatabase/build'
    exe = 'XbSymbolDatabaseCLI.exe' if os.name == 'nt' else 'XbSymbolDatabaseCLI'
    for name in ('Debug/' + exe, 'Release/' + exe, 'XbSymbolDatabaseCLI.slnx', exe + '.recipe'):
        path = base / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    assert catalog.find_cli() == str(base / 'Release' / exe)
    monkeypatch.setenv('XBSDB_CLI', 'environment')
    assert catalog.find_cli() == 'environment' and catalog.find_cli('explicit') == 'explicit'


def test_supplements_validate_and_do_not_override_or_duplicate(tmp_path, monkeypatch):
    xbe = tmp_path / 'sample.xbe'
    xbe.write_bytes(fixture())
    monkeypatch.setattr(catalog, 'REPO', tmp_path)
    path = tmp_path / 'config/extra_symbols/12345678.json'
    path.parent.mkdir(parents=True)
    entry = dict(address=0x11000, library='OWN', kind='function', name='Call', call='stdcall', stack_bytes=8)
    path.write_text(json.dumps(dict(symbols=[entry])))
    assert catalog.title_id(xbe) == 0x12345678
    extra = catalog.load_extra(xbe)
    assert extra == [entry]
    assert catalog.merge_symbols([], extra * 2) == extra
    detected = dict(entry, name='Scanned')
    assert catalog.merge_symbols([detected], extra) == [detected]
    path.write_text(json.dumps(dict(symbols=[dict(entry, address=True)])))
    with pytest.raises(ValueError):
        catalog.load_extra(xbe)
    with pytest.raises(FileNotFoundError):
        catalog.load_extra(xbe, tmp_path / 'missing.json')
    xbe.write_bytes(b'bad')
    assert catalog.title_id(xbe) is None


def test_cli_failure_preserves_output_and_success_publishes(tmp_path, monkeypatch):
    xbe, output = tmp_path / 'sample.xbe', tmp_path / 'symbols.json'
    xbe.write_bytes(fixture())
    output.write_text('preserve')
    monkeypatch.setattr(catalog, 'REPO', tmp_path)
    calls = []
    def scanner(argv, **kwargs):
        calls.append(argv)
        return SimpleNamespace(returncode=0, stdout='LIB__VAR__Register = 0x11000', stderr='')
    monkeypatch.setattr(catalog.subprocess, 'run', scanner)
    args = [str(xbe), '--cli', 'test scanner', '--out', str(output)]
    assert catalog.main(args) == 0
    assert calls == [['test scanner', str(xbe.resolve()), '-e']]
    result = output.read_bytes()
    assert json.loads(result)['symbols'][0]['name'] == 'Register'
    for process in (SimpleNamespace(returncode=7, stdout='', stderr='failed'),
                    SimpleNamespace(returncode=0, stdout='no records', stderr='')):
        monkeypatch.setattr(catalog.subprocess, 'run', lambda *args, **kwargs: process)
        assert catalog.main(args) == 1
        assert output.read_bytes() == result
    def timeout(*args, **kwargs):
        raise subprocess.TimeoutExpired('fixture', 300)
    monkeypatch.setattr(catalog.subprocess, 'run', timeout)
    assert catalog.main(args) == 1 and output.read_bytes() == result
    assert not list(tmp_path.glob('*.tmp'))
    original = xbe.read_bytes()
    assert catalog.main([str(xbe), '--out', str(xbe)]) == 1
    assert xbe.read_bytes() == original


def test_adapter_imports_and_parses_without_toolkit(tmp_path):
    import shutil
    for name in ('xdk_catalog.py', 'analysis_output.py', 'xbe_image.py', 'xbe_kernel_exports.def'):
        shutil.copyfile(repo_file(ROOT, 'tools', name), tmp_path / name)
    script = '''import sys
sys.path.insert(0,sys.argv[1])
import xdk_catalog
assert xdk_catalog.parse_line("SDK__FUN__stdcall__Sample(psh count) = 0x1234")["stack_bytes"] == 4
assert not any(name == 'tools' or name.startswith('tools.') for name in sys.modules)
'''
    result = subprocess.run([sys.executable, '-I', '-c', script, str(tmp_path)],
                            cwd=tmp_path, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr


def test_production_entry_uses_project_adapter_and_existing_protocol_consumers():
    runtime = ROOT / 'external/ts3FPrecomp'
    if not (runtime / '.git').exists():
        pytest.skip('Pinned runtime unavailable')
    script = '''import tools.xdk_symbols.__main__ as adapter
assert adapter.__name__ == 'xdk_catalog'
assert adapter.parse_line.__module__ == 'xdk_catalog'
assert adapter.find_cli() is None or 'XbSymbolDatabaseCLI' in adapter.find_cli()
'''
    environment = dict(os.environ, TSFP_PROJECT_ROOT=str(ROOT))
    result = subprocess.run([sys.executable, '-c', script], cwd=runtime, env=environment,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([sys.executable, '-m', 'pytest', 'tools/xdk_symbols', '-q'],
                            cwd=runtime, env=environment, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
