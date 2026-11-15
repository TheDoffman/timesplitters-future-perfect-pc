"""Pinned offline wheel admission, ownership and extraction boundaries."""
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import stat
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('runtime_wheels_test', ROOT / 'tools/mods/hosting/runtime_wheels.py')
wheels = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wheels)


def wheel(name='demo', version='1', extra=()):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w') as archive:
        entries = [(name + '/__init__.py', b'', stat.S_IFREG | 0o644),
                   (name + '-' + version + '.dist-info/METADATA', ('Name: ' + name + '\nVersion: ' + version + '\n').encode(), stat.S_IFREG | 0o644),
                   (name + '-' + version + '.dist-info/licenses/LICENSE', b'fixture license', stat.S_IFREG | 0o644), *extra]
        for filename, content, mode in entries:
            info = zipfile.ZipInfo(filename)
            info.create_system = 3
            info.external_attr = mode << 16
            archive.writestr(info, content)
    wire = stream.getvalue()
    # ZipInfo normalizes separators on Windows; preserve the malformed on-wire
    # filename so this is the same adversarial archive on every host.
    for filename, _, _ in extra:
        if '\\' in filename:
            wire = wire.replace(filename.replace('\\', '/').encode(), filename.encode())
    return wire


def pins(wire, name='demo', version='1'):
    return {name: dict(version=version, hashes={hashlib.sha256(wire).hexdigest()})}


def test_repository_requirements_are_exact_hashed_pins():
    root = ROOT / 'tools/mods/hosting'
    result = wheels.parse_pins([(root / n).read_text() for n in ('requirements.txt', 'requirements-signing.txt')])
    assert set(result) == {'gunicorn', 'cryptography', 'cffi', 'pycparser'}
    assert all(p['hashes'] for p in result.values())


@pytest.mark.parametrize('text', ['demo>=1', 'demo==1', '--extra-index-url https://example.test',
                                  'demo==1 --hash=md5:0000', '-r ../../other', 'demo==1 \\'])
def test_unpinned_or_external_requirement_syntax_refused(text):
    with pytest.raises(ValueError):
        wheels.parse_pins([text])


def test_pinned_wheel_checks_identity_and_retains_license():
    wire = wheel()
    package, entries = wheels.inspect(wire, pins(wire))
    assert package['name'] == 'demo' and package['version'] == '1'
    assert any(n.endswith('/LICENSE') for n, _, _ in entries)
    with pytest.raises(ValueError, match='digest'):
        wheels.inspect(wire + b'changed', pins(wire))
    with pytest.raises(ValueError, match='metadata'):
        wheels.inspect(wire, pins(wire, version='2'))


@pytest.mark.parametrize('name,mode', [('../escape', stat.S_IFREG | 0o644),
                                      ('/absolute', stat.S_IFREG | 0o644),
                                      ('demo\\escape', stat.S_IFREG | 0o644),
                                      ('startup.pth', stat.S_IFREG | 0o644),
                                      ('demo.data/scripts/hook', stat.S_IFREG | 0o644),
                                      ('demo/link', stat.S_IFLNK | 0o777),
                                      ('demo/setuid', stat.S_IFREG | 0o4755),
                                      ('demo/writable', stat.S_IFREG | 0o666),
                                      ('DEMO/__INIT__.PY', stat.S_IFREG | 0o644),
                                      ('line\nfeed', stat.S_IFREG | 0o644)])
def test_structural_checks_apply_even_to_a_pinned_fixture(name, mode):
    wire = wheel(extra=[(name, b'fixture', mode)])
    with pytest.raises(ValueError):
        wheels.inspect(wire, pins(wire))


@pytest.mark.skipif(os.name != 'posix', reason='POSIX owner/mode admission and extraction')
def test_combined_extraction_validates_every_input_before_creating_destination(tmp_path):
    tmp_path = tmp_path.resolve()
    incoming = tmp_path / 'wheels'
    incoming.mkdir()
    wire = wheel()
    path = incoming / 'demo.whl'
    path.write_bytes(wire)
    path.chmod(0o644)
    destination = tmp_path / 'site-packages'
    packages, records = wheels.install(incoming, destination, pins(wire))
    assert packages['demo']['sha256'] == hashlib.sha256(wire).hexdigest()
    assert records['demo/__init__.py']['source_sha256'] == hashlib.sha256(b'').hexdigest()
    assert (destination / 'demo-1.dist-info/licenses/LICENSE').read_bytes() == b'fixture license'
    with pytest.raises(ValueError, match='new wheel'):
        wheels.install(incoming, destination, pins(wire))
    path.write_bytes(wire + b'tampered')
    with pytest.raises(ValueError, match='digest'):
        wheels.install(incoming, tmp_path / 'refused', pins(wire))
    assert not (tmp_path / 'refused').exists()


@pytest.mark.skipif(os.name != 'posix', reason='POSIX owner/mode admission and extraction')
def test_cross_wheel_file_collisions_preserve_inputs_and_create_no_output(tmp_path):
    tmp_path = tmp_path.resolve()
    incoming = tmp_path / 'wheels'
    incoming.mkdir()
    first = wheel(extra=[('shared.py', b'one', stat.S_IFREG | 0o644)])
    second = wheel('other', extra=[('SHARED.py', b'two', stat.S_IFREG | 0o644)])
    for name, wire in [('first.whl', first), ('second.whl', second)]:
        (incoming / name).write_bytes(wire)
        (incoming / name).chmod(0o644)
    with pytest.raises(ValueError, match='collision'):
        wheels.install(incoming, tmp_path / 'refused', pins(first) | pins(second, name='other'))
    assert not (tmp_path / 'refused').exists()


def test_archive_and_expansion_budgets_fail_before_install(monkeypatch):
    wire = wheel()
    monkeypatch.setattr(wheels, 'MAX_EXPANDED', 1)
    with pytest.raises(ValueError, match='expanded'):
        wheels.inspect(wire, pins(wire))
    monkeypatch.setattr(wheels, 'MAX_ARCHIVE', 1)
    with pytest.raises(ValueError, match='archive'):
        wheels.inspect(wire, pins(wire))
