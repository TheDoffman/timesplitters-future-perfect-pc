"""Disposable signing identities; no fixture key is used by a release or server."""
import sys
import os
import tempfile
import copy
import json
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


@pytest.fixture
def workshop_routing():
    """In-memory fake Tailscale status/actions; journal/route code stays real."""
    import mod_host_routing as routes
    host='workshop.example.test';origin='https://'+host+':8443'
    original=dict(TCP={'443':dict(HTTPS=True),'8443':dict(HTTPS=True)},
        Web={host+':443':dict(Handlers={'/':dict(Proxy='http://127.0.0.1:18789')}),
             host+':8443':dict(Handlers={'/':dict(Proxy=routes.TARGET)})},AllowFunnel={host+':8443':True})
    value=copy.deepcopy(original)
    def invoke(action):
        if action=='status':return json.dumps(value).encode()
        if action=='suspend':
            for field,key in [('TCP','8443'),('Web',host+':8443'),('AllowFunnel',host+':8443')]:value[field].pop(key,None)
        else:
            assert action=='restore';value.clear();value.update(copy.deepcopy(original))
        return b''
    return dict(baseline=routes.snapshot(invoke('status'),origin),invoke=invoke,value=value)


@pytest.fixture
def public_upgrade_report():
    """Typed child evidence only; never a real TLS or service-health assertion."""
    def report(journal):
        state=journal.state();profile=state['private_baseline']
        checkpoint=dict(catalog_version=state['catalog_floor'],metadata_sha256='1'*64,
                        revocations_sha256='2'*64,revocation_count=5)
        return dict(schema=1,phase='public-services-verified',release=journal.base[state['config']],uid=journal.base['uid'],
            origin=profile['origin'],trusted_root_sha256=profile['trusted_root_sha256'],loopback=dict(checkpoint),public=dict(checkpoint),
            release_count=0,revocations_preserved=True,role_health_verified=True,transport='public-https',public_endpoint_verified=True)
    return report
