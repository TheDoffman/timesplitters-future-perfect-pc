"""Player-side connection contracts without a server installation."""
import hmac
from pathlib import Path
import sys

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from online_client import Client, Session
from online_transport import WIRE, WIRE_MAGIC

VERSION = 'a'*64


def test_latency_requires_authenticated_matching_nonce_and_expires():
    from online_transport import Bridge, PONG
    clock = [100.0]
    host = ('127.0.0.1', 30821)
    key = bytes.fromhex('b'*64)
    bridge = Bridge(VERSION, 'c'*32, 2, key.hex(), host, clock=lambda: clock[0])
    nonce = b'12345678'
    try:
        bridge.pending_pings[1] = (nonce, clock[0])
        clock[0] += 1.25
        bridge.ready.set()
        bridge.last_hello = clock[0]
        bridge.poll(0)
        assert bridge.pending_pings[1][0] == nonce  # Slow replies survive the next probe interval.
        def reply(sequence, payload, signing_key=key):
            body = WIRE.pack(WIRE_MAGIC, bytes.fromhex(VERSION), bridge.room, sequence, 1, 2, 0, 0)+bytes((PONG,))+payload
            return body+hmac.digest(signing_key, body, 'sha256')
        bridge.from_network(reply(1, nonce, bytes(32)), host)
        assert bridge.latency_snapshot() == (-1, 0)
        assert bridge.quality_snapshot()['peers'] == []
        bridge.from_network(reply(2, b'wrong123'), host)
        assert bridge.latency_snapshot() == (-1, 0)
        bridge.from_network(reply(3, nonce), host)
        milliseconds, peers = bridge.latency_snapshot()
        assert peers == 1 and milliseconds == pytest.approx(1250)
        assert bridge.quality_snapshot()['peers'][0]['samples'] == 1
        clock[0] += 6
        bridge.from_network(reply(3, nonce), host)  # Replay cannot refresh a stale sample.
        assert bridge.latency_snapshot() == (-1, 0)
        bridge.from_network(reply(4, nonce), host)  # Unsolicited pong cannot refresh it either.
        assert bridge.latency_snapshot() == (-1, 0)
    finally:
        bridge.close()


def test_quality_distinguishes_timed_out_probes_stale_rtt_and_epoch_reset():
    from online_arcade_client import NativeArcade
    from online_transport import Bridge, PONG
    clock = [100.0]
    host = ('127.0.0.1', 30821)
    key = bytes.fromhex('b'*64)
    bridge = Bridge(VERSION, 'c'*32, 2, key.hex(), host, clock=lambda: clock[0])
    try:
        bridge.ready.set()
        bridge.poll(0)
        nonce = bridge.pending_pings[1][0]
        clock[0] += .25
        packet = WIRE.pack(WIRE_MAGIC, bytes.fromhex(VERSION), bridge.room, 1, 1, 2, 0, 0)+bytes((PONG,))+nonce
        bridge.from_network(packet+hmac.digest(key, packet, 'sha256'), host)
        clock[0] += 1
        bridge.poll(0)
        clock[0] += 6
        bridge.poll(0)
        report = bridge.quality_snapshot()
        assert report['window_seconds'] == 60
        assert report['peers'] == [dict(peer=1, rtt_ms=None, rtt_age=7.0, samples=2, missed=1)]
        native = NativeArcade(bridge)
        native.status = dict(epoch=1, host_peer=1, phase='active')
        assert native.body(VERSION)['connection_quality'] == report
        assert key.hex() not in str(report) and bridge.local_token.hex() not in str(report)
        clock[0] += 61
        assert bridge.quality_snapshot()['peers'] == []
        bridge.quality_record(1, 20)
        bridge.quality_reset()
        assert bridge.quality_snapshot()['peers'] == []
    finally:
        bridge.close()

def test_remote_plaintext_and_invalid_pin_rejected():
    with pytest.raises(ValueError, match='HTTPS'):
        Client('http://203.0.113.7:30820')
    with pytest.raises(ValueError, match='fingerprint'):
        Client('https://example.test:30820', 'short')

@pytest.mark.parametrize('host', [True, False])
def test_story_rejects_old_directory_and_cleans_registration(host):
    class OldDirectory:
        def __init__(self):
            self.deleted = False
        def request(self, method, path, body=None, token=None):
            if method == 'DELETE':
                self.deleted = True
                assert token == ('owner-fixture' if host else 'peer-fixture')
                return {}
            return dict(code='A'*16, room='0'*32, address='127.0.0.1', port=30821,
                        owner_token='owner-fixture', peer_key='peer-fixture', peer_id=2,
                        capacity=16)
    client = OldDirectory()
    with pytest.raises(ValueError, match='does not support'):
        if host:
            Session.create(client, VERSION, 'Story', 0, False, mode='story')
        else:
            Session.join(client, VERSION, 'A'*16, mode='story')
    assert client.deleted
