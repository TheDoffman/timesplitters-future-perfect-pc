"""Disposable signing identities; no fixture key is used by a release or server."""
import sys
import os
import tempfile
from pathlib import Path

import pytest
from cryptography.hazmat.primitives.asymmetric import rsa

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools/mods'))


@pytest.fixture(scope='session')
def trust_keys():
    return tuple(rsa.generate_private_key(public_exponent=65537, key_size=3072) for _ in range(6))


@pytest.fixture
def private_posix_path():
    """Exercise real ancestor checks without pytest's shared /tmp ancestor."""
    if os.name!='posix':pytest.skip('Actual private POSIX fixture')
    with tempfile.TemporaryDirectory(prefix='.tsfp-private-test-',dir=Path.home()) as directory:
        path=Path(directory);path.chmod(0o700)
        yield path
