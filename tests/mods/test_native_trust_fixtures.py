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
