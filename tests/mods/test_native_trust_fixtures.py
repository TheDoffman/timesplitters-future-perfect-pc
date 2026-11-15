"""The public golden vectors exercise the exact Python producer/native protocol."""
import base64
import json
from pathlib import Path

import pytest

from mod_package import ModError, digest, inspect_package
from mod_trust import trusted_root, verify_catalog


def test_public_native_vectors_match_producer_and_original_asset(tmp_path):
    fixtures = json.loads((Path(__file__).resolve().parents[1] / 'launcher/workshop-trust-fixtures.json').read_bytes())
    wire = lambda name: base64.b64decode(fixtures[name], validate=True)
    now = fixtures['unix']
    first = trusted_root(wire('root1'), now=now)
    second = trusted_root(wire('root1'), [wire('root2')], now=now)
    previous = None
    for version in range(1, 5):
        _, previous = verify_catalog(wire('metadata' + str(version)), wire('catalog' + str(version)),
                                     first, previous, now)
    with pytest.raises(ModError, match='forgot'):
        verify_catalog(wire('metadata6'), wire('catalog6'), second, previous, now)
    verify_catalog(wire('metadata5'), wire('catalog5'), second, now=now)
    package = tmp_path / 'synthetic-texture.zip'
    package.write_bytes(wire('package'))
    assert digest(package.read_bytes()) == fixtures['package_sha256']
    assert inspect_package(package)['id'] == 'community-demo'
    assert all('PRIVATE KEY' not in str(value) for value in fixtures.values())


def test_port_bound_vectors_reject_cross_authority_metadata_and_rotation():
    fixtures = json.loads((Path(__file__).resolve().parents[1] / 'launcher/workshop-trust-fixtures.json').read_bytes())
    wire = lambda name: base64.b64decode(fixtures[name], validate=True)
    now = fixtures['unix']
    for suffix in ('8443', '10000'):
        root = trusted_root(wire('port_root' + suffix), now=now)
        assert root['origin'] == fixtures['origin'] + ':' + suffix
        verify_catalog(wire('port_metadata' + suffix), wire('port_catalog'), root, now=now)
    root = trusted_root(wire('port_root8443'), now=now)
    with pytest.raises(ModError):
        verify_catalog(wire('port_metadatawrongdefault'), wire('port_catalog'), root, now=now)
    with pytest.raises(ModError):
        trusted_root(wire('port_root8443'), [wire('port_root10000')], now=now)
    with pytest.raises(ModError):
        trusted_root(wire('port_root_invalid444'), now=now)
