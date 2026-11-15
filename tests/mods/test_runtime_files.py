"""Trusted runtime literal-read admission; no executable code or service starts."""
import hashlib
import json
import os
from pathlib import Path
import stat
import sys
import tempfile

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))
from mod_package import ModError
import mod_runtime_files as runtime
import mod_scan_sandbox as sandbox


def test_manifest_rejects_ambiguous_numbers_and_duplicate_fields():
    for wire in [b'{"schema":1,"schema":1}', b'{"schema":1.0}', b'{"schema":NaN}', b'{"schema":true}', b'[]']:
        with pytest.raises(ModError):
            runtime.manifest(wire)


def test_bundled_profile_grants_exact_literals_without_brew_or_parent_reads(monkeypatch):
    library = Path('/approved/lib/0000000000000000-library.dylib')
    certificate = Path('/approved/Certs/clamav/clamav.crt')
    monkeypatch.setattr(sandbox, 'runtime_access', lambda _: dict(images=[library], certificate=certificate))
    profile = sandbox.sandbox_profile(Path('/approved/bin/clamscan'), Path('/content'), Path('/database'), Path('/scratch'))
    assert '(literal ' + json.dumps(str(library)) + ')' in profile
    assert 'Cellar' not in profile and '/opt/homebrew' not in profile
    assert '(subpath "/approved")' not in profile and '(subpath "/approved/lib")' not in profile
    assert '(deny process-info*)' in profile
    assert '(allow network' not in profile and '(allow process-fork' not in profile
    mapping = profile.split('(allow file-map-executable ', 1)[1].split('(allow file-write*', 1)[0]
    assert json.dumps(str(certificate)) not in mapping


@pytest.fixture
def bundle():
    if os.name != 'posix':
        pytest.skip('POSIX ownership, nofollow and ancestor admission')
    # Production deliberately refuses writable ancestors, including /tmp. Use
    # a new owner-private test directory under the current user's home.
    with tempfile.TemporaryDirectory(prefix='.tsfp-runtime-files-', dir=Path.home()) as temporary:
        root = Path(temporary).resolve()
        (root / 'bin').mkdir()
        (root / 'lib').mkdir()
        (root / 'Certs/clamav').mkdir(parents=True)
        entries = {'bin/clamscan': b'fixture executable', 'lib/0123456789abcdef-library.dylib': b'fixture library',
                   'Certs/clamav/clamav.crt': b'fixture public certificate'}
        records = {}
        for name, wire in entries.items():
            path = root / name
            path.write_bytes(wire)
            path.chmod(0o755 if name.startswith('bin/') else 0o644)
            records[name] = dict(bytes=len(wire), sha256=hashlib.sha256(wire).hexdigest(), mode=stat.S_IMODE(path.stat().st_mode))
        value = dict(schema=1, producer='tsfp-host-runtime-v1', files=records)
        marker = root / 'runtime.json'
        marker.write_text(json.dumps(value))
        marker.chmod(0o600)
        yield root, value


def test_verified_literals_do_not_include_the_manifest_or_unknown_neighbors(bundle):
    root, _ = bundle
    canary = root / 'lib/unrecorded-secret'
    canary.write_text('must not be granted')
    result = runtime.runtime_access(root / 'bin/clamscan')
    assert set(result['images']) == {root / 'bin/clamscan', root / 'lib/0123456789abcdef-library.dylib'}
    assert result['certificate'] == root / 'Certs/clamav/clamav.crt'
    assert canary not in result['images'] and root / 'runtime.json' not in result['images']


@pytest.mark.parametrize('change', ['bytes', 'permissions', 'symlink', 'hardlink'])
def test_changed_or_linked_library_is_withheld(bundle, change):
    root, _ = bundle
    library = root / 'lib/0123456789abcdef-library.dylib'
    if change == 'bytes':
        library.write_bytes(b'tampered')
    elif change == 'permissions':
        library.chmod(0o666)
    elif change == 'symlink':
        library.rename(root / 'original')
        library.symlink_to(root / 'original')
    else:
        os.link(library, root / 'original')
    with pytest.raises((ModError, OSError)):
        runtime.runtime_access(root / 'bin/clamscan')
    assert library.exists()


@pytest.mark.parametrize('change', ['invalid-json', 'linked-marker', 'writable-parent', 'linked-parent', 'escape', 'wrong-producer'])
def test_damaged_bundle_never_falls_back_to_legacy_permissions(bundle, change):
    root, value = bundle
    marker = root / 'runtime.json'
    if change == 'invalid-json':
        marker.write_bytes(b'{')
    elif change == 'linked-marker':
        marker.rename(root / 'original-marker')
        marker.symlink_to(root / 'original-marker')
    elif change == 'writable-parent':
        (root / 'lib').chmod(0o777)
    elif change == 'linked-parent':
        (root / 'lib').rename(root / 'original-lib')
        (root / 'lib').symlink_to(root / 'original-lib', target_is_directory=True)
    elif change == 'escape':
        value['files']['lib/../secret'] = next(iter(value['files'].values()))
        marker.write_text(json.dumps(value))
    else:
        value['producer'] = 'unknown'
        marker.write_text(json.dumps(value))
    with pytest.raises((ModError, OSError)):
        runtime.runtime_access(root / 'bin/clamscan')
    assert marker.exists()


def test_missing_receipt_adds_no_runtime_literal_permissions(bundle):
    root, _ = bundle
    (root / 'runtime.json').unlink()
    assert runtime.runtime_access(root / 'bin/clamscan') is None


@pytest.mark.parametrize('change', ['missing', 'tampered', 'linked'])
def test_bundled_certificate_is_required_and_digest_bound(bundle, change):
    root, _ = bundle
    certificate = root / 'Certs/clamav/clamav.crt'
    if change == 'missing':
        certificate.unlink()
    elif change == 'tampered':
        certificate.write_bytes(b'wrong certificate')
    else:
        certificate.rename(root / 'original-certificate')
        certificate.symlink_to(root / 'original-certificate')
    with pytest.raises((ModError, OSError)):
        runtime.runtime_access(root / 'bin/clamscan')
