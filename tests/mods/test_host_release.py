"""Immutable release admission/copy tests; never change accounts or start services."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import tempfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('host_release_test', ROOT / 'tools/mods/hosting/host_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
spec = importlib.util.spec_from_file_location('host_installer_test', ROOT / 'tools/mods/hosting/install-release-mac.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


def record(wire, mode=0o644):
    return dict(bytes=len(wire), mode=mode, sha256=hashlib.sha256(wire).hexdigest())


@pytest.mark.parametrize('value', ['../escape', '/absolute', 'a//b', 'a/./b', 'a\\b', 'C:escape', 'line\nfeed'])
def test_unsafe_component_paths_refused(value):
    with pytest.raises(ValueError):
        release.normalize({value: record(b'x')})


@pytest.mark.parametrize('files', [
    {'A': record(b'x'), 'a': record(b'y')},
    {'parent': record(b'x'), 'parent/child': record(b'y')},
    {'link': dict(symlink='../outside')},
    {'link': dict(symlink='/outside')},
    {'file': dict(bytes=1, mode=0o4755, sha256='0' * 64)},
    {'file': dict(bytes=True, mode=0o644, sha256='0' * 64)},
])
def test_ambiguous_or_escaping_inventory_refused(files):
    with pytest.raises(ValueError):
        release.normalize(files)


def test_metadata_duplicate_and_noninteger_values_refused():
    for wire in [b'{"a":1,"a":2}', b'{"a":NaN}', b'{"a":1.5}']:
        with pytest.raises(ValueError):
            release.decode(wire, 4096)


def test_root_admission_precedes_account_or_bundle_reads(monkeypatch):
    monkeypatch.setattr(installer.sys, 'platform', 'darwin')
    monkeypatch.setattr(installer.os, 'geteuid', lambda: 501, raising=False)
    monkeypatch.setattr(installer.identity, 'existing', lambda: pytest.fail('Read privileged identity before root admission'))
    with pytest.raises(ValueError, match='administrator sudo'):
        installer.install({})


@pytest.fixture
def bundle():
    if os.name != 'posix' or os.getuid() == 0:
        pytest.skip('Unprivileged POSIX input ownership and descriptor-relative I/O')
    with tempfile.TemporaryDirectory(prefix='.tsfp-host-release-', dir=Path.home()) as temporary:
        base = Path(temporary).resolve()
        runtime = base / 'runtime'
        source = base / 'source'
        runtime.mkdir(mode=0o700)
        source.mkdir(mode=0o700)
        runtime_files = {}
        python = 'Frameworks/Python.framework/Versions/3.12/bin/python3.12'
        for key in [python, 'bin/clamscan', 'bin/freshclam', 'bin/caddy', 'Certs/clamav/clamav.crt']:
            wire = ('fixture ' + key).encode()
            path = runtime / key
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(wire)
            mode = 0o644 if key.startswith('Certs/') else 0o755
            path.chmod(mode)
            runtime_files[key] = record(wire, mode)
        runtime_meta = dict(schema=1, producer='tsfp-host-runtime-v1', python=python, files=runtime_files,
                            applications=dict(packages={name: {} for name in ('gunicorn', 'cryptography', 'cffi', 'pycparser')}))
        raw = json.dumps(runtime_meta).encode()
        (runtime / 'runtime.json').write_bytes(raw)
        (runtime / 'runtime.json').chmod(0o600)
        source_files = {}
        keys = ['tools/mods/mod_api.py', 'tools/mods/mod_runtime_files.py', 'tools/mods/scan_guardian_mac.c',
                'tools/mods/scan_child.py', 'build/scan-guardian/scan-guardian-mac']
        for key in keys:
            wire = ('fixture ' + key).encode()
            path = source / key
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(wire)
            mode = 0o755 if key.endswith('/scan-guardian-mac') else 0o644
            path.chmod(mode & ~0o200)
            source_files[key] = record(wire, mode)
        guardian = dict(schema=1, producer='tsfp-scan-guardian-v1',
                        source_sha256=source_files['tools/mods/scan_guardian_mac.c']['sha256'],
                        sha256=source_files['build/scan-guardian/scan-guardian-mac']['sha256'])
        wire = json.dumps(guardian).encode()
        key = 'build/scan-guardian/receipt.json'
        (source / key).write_bytes(wire)
        (source / key).chmod(0o444)
        source_files[key] = record(wire)
        data = dict(schema=1, producer='tsfp-host-release-v1', owner_uid=os.getuid(), source_revision='a' * 40,
                    runtime_root=str(runtime), runtime_sha256=hashlib.sha256(raw).hexdigest(),
                    source_root=str(source), source_files=source_files)
        descriptor = base / 'release.json'
        descriptor.write_text(json.dumps(data))
        descriptor.chmod(0o600)
        yield base, descriptor, data


def test_real_input_plan_verifies_all_components_without_writing_root(bundle):
    base, descriptor, _ = bundle
    plan = release.plan(descriptor, hashlib.sha256(descriptor.read_bytes()).hexdigest())
    assert plan['uid'] == os.getuid() and len(plan['components']) == 2
    assert not (base / 'target').exists()


@pytest.mark.parametrize('change', ['tamper', 'unrecorded', 'symlink', 'hardlink', 'writable-parent'])
def test_changed_component_is_preserved_and_refused(bundle, change):
    base, descriptor, _ = bundle
    path = base / 'runtime/bin/clamscan'
    if change == 'tamper':
        path.write_bytes(b'changed')
    elif change == 'unrecorded':
        (base / 'runtime/unknown').write_bytes(b'preserve')
    elif change == 'symlink':
        path.rename(base / 'original')
        path.symlink_to(base / 'original')
    elif change == 'hardlink':
        os.link(path, base / 'original')
    else:
        path.parent.chmod(0o777)
    with pytest.raises((ValueError, OSError)):
        release.plan(descriptor, hashlib.sha256(descriptor.read_bytes()).hexdigest())
    assert path.exists()


def test_exclusive_copy_cannot_replace_existing_release_files(bundle):
    base, descriptor, _ = bundle
    plan = release.plan(descriptor, hashlib.sha256(descriptor.read_bytes()).hexdigest())
    _, source, records = plan['components'][1]
    target = base / 'target'

    def create(path):
        path.mkdir(mode=0o755)
        path.chmod(0o755)

    release.copy_component(source, target, records, os.getuid(), create)
    release.verify(target, records, os.getuid(), directory_mode=0o755)
    with pytest.raises(FileExistsError):
        release.copy_component(source, target, records, os.getuid(), create)
    assert (target / 'tools/mods/mod_api.py').read_bytes().startswith(b'fixture')


def test_wrong_descriptor_digest_fails_before_component_copy(bundle):
    _, descriptor, _ = bundle
    with pytest.raises(ValueError, match='descriptor digest'):
        release.plan(descriptor, '0' * 64)
