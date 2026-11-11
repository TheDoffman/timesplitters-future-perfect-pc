"""Private observability must not mutate games or disclose credentials."""
import http.client
import json
import socket
import subprocess
import threading
import time

import pytest

from lobby_server import Directory, Server, RelayServer
from online_dashboard import Dashboard, Monitor
from online_migration import SCHEMA
from test_online import VERSION


def room_fixture():
    now=[100.]
    directory=Directory(clock=lambda:now[0],relay_address=('127.0.0.1',12345))
    service=directory.arcade
    _,host=service.request('POST','/v3/lobbies',{},dict(compatibility=VERSION,name='<b>Private room</b>',
        mode='arcade',migration_schema=SCHEMA,public=True),'127.0.0.1','')
    _,guest=service.request('POST','/v3/lobbies/'+host['code']+'/join',{},dict(compatibility=VERSION,
        migration_schema=SCHEMA),'127.0.0.1','')
    room=service.rooms[host['code']]
    service.update_native(room,1,dict(native_counter=5,active=True,native_peer=0,epoch=1))
    service.update_native(room,2,dict(native_counter=6,active=True,native_peer=1,epoch=room['epoch']))
    payload=bytearray(208);payload[48]=2;payload[50]=6
    digest=room['migration'].publish(1,room['epoch'],1,60,bytes(payload))
    room['migration'].acknowledge(2,room['epoch'],1,digest)
    room['members'][1].update(endpoint=('192.168.0.4',54321),seen=now[0])
    room['members'][2].update(endpoint=('8.8.8.8',54322),seen=now[0])
    return directory,room,now,host,guest,digest


def test_snapshot_is_read_only_and_contains_no_tokens_keys_payloads_or_addresses():
    directory,room,now,host,guest,digest=room_fixture()
    monitor=Monitor(directory,clock=lambda:now[0])
    monitor.sample()
    state=monitor.snapshot();text=json.dumps(state)
    for private in (host,guest):
        for key in ('peer_token','peer_key','relay_key','room'):
            assert private[key] not in text
    assert digest not in text and '192.168.0.4' not in text and '8.8.8.8' not in text
    snapshot=state['rooms'][0]
    assert snapshot['backup']==dict(sequence=1,age=0,acknowledgements=1,bots=6)
    assert [peer['route'] for peer in snapshot['peers']]==['LAN','Public']
    seen=room['migration'].members[2].seen
    now[0]+=10;monitor.sample()
    assert room['migration'].members[2].seen==seen
    assert set(room['migration'].members)=={1,2}  # Dashboard does not poll/retire.
    assert monitor.snapshot()['rooms'][0]['peers'][1]['lease_remaining']==0
    now[0]+=5
    assert monitor.snapshot()['sample_age']==5  # Independent of browser wall clock.


def test_monitor_tracks_actual_host_and_phase_transitions_without_request_details():
    directory,room,now,host,guest,digest=room_fixture()
    monitor=Monitor(directory,clock=lambda:now[0]);monitor.sample()
    room['migration'].leave(1,room['epoch']);directory.arcade.sync(room);monitor.sample()
    room['migration'].ready(2,room['epoch'],digest);directory.arcade.sync(room);monitor.sample()
    events=monitor.snapshot()['events']
    assert any(e['kind']=='phase' and 'preparing' in e['message'] for e in events)
    assert monitor.snapshot()['rooms'][0]['host']==2
    monitor.http('PUT',403,.1);monitor.failure('timeout');monitor.sample()
    assert monitor.snapshot()['http']['errors']==1
    assert monitor.snapshot()['http']['failures']['timeout']==1


def test_history_and_event_storage_are_bounded():
    directory=Directory();monitor=Monitor(directory)
    for _ in range(300):
        monitor.http('PUT',409,.01);monitor.sample()
    state=monitor.snapshot()
    assert len(state['events'])==100 and len(state['samples'])==180
    assert state['http']['requests']==300 and state['http']['p95_ms']==10


def test_legacy_sessions_are_labelled_without_inventing_native_progress():
    directory=Directory()
    directory.request('POST','/v2/lobbies',{},dict(compatibility=VERSION,name='Story',mode='story',
        transport='direct',port=30821,public=True),'127.0.0.1','')
    monitor=Monitor(directory);monitor.sample()
    row=monitor.snapshot()['rooms'][0]
    assert row['protocol']=='Legacy v2' and not row['migration_supported']
    assert row['peers'][0]['active'] is None and row['backup'] is None


def test_dashboard_only_serves_whitelisted_read_only_loopback_routes():
    directory,room,*_=room_fixture()
    dashboard=Dashboard(directory,port=0);dashboard.start()
    assert dashboard.server.server_address[0]=='127.0.0.1'
    def get(path,headers=None,method='GET'):
        connection=http.client.HTTPConnection('127.0.0.1',dashboard.server.server_port,timeout=2)
        try:
            connection.request(method,path,headers=headers or {})
            response=connection.getresponse()
            return response.status,dict(response.getheaders()),response.read()
        finally:connection.close()
    try:
        status,headers,data=get('/api/status')
        assert status==200 and json.loads(data)['rooms'][0]['name']=='<b>Private room</b>'
        assert headers['X-Frame-Options']=='DENY' and headers['Cache-Control']=='no-store'
        assert "frame-ancestors 'none'" in headers['Content-Security-Policy']
        assert get('/api/status',{'Host':'attacker.invalid'})[0]==403
        assert get('/api/status',{'Host':'127.0.0.1:59999'})[0]==200  # Different SSH local port.
        assert get('/api/status',{'Host':'127.0.0.1:99999'})[0]==403
        assert get('/api/status',{'Sec-Fetch-Site':'cross-site'})[0]==403
        assert get('/../../certificate/private-key.pem')[0]==404
        assert get('/api/status',method='DELETE')[0]==501
        for path in ('/','/dashboard.css','/dashboard.js'):
            assert get(path)[0]==200
        assert room['host_peer']==1 and len(room['members'])==2
    finally:dashboard.close()


def test_real_directory_errors_and_relay_traffic_feed_the_monitor():
    directory=Directory();relay=RelayServer(directory)
    monitor=Monitor(directory,relay)
    with Server(('127.0.0.1',0),directory) as server:
        server.monitor=monitor
        thread=threading.Thread(target=server.serve_forever);thread.start()
        try:
            connection=http.client.HTTPConnection('127.0.0.1',server.server_port,timeout=2)
            connection.request('GET','/unknown');response=connection.getresponse();response.read();connection.close()
            relay.handle(b'invalid packet',('127.0.0.1',12345));monitor.sample()
            state=monitor.snapshot()
            assert state['http']['statuses']=={'404':1}
            assert state['counters']['dropped']==1 and state['counters']['received_bytes']==14
        finally:server.shutdown();thread.join();relay.close()


def test_installer_copies_dashboard_and_preserves_enabled_port_and_certificate(tmp_path):
    from install_online_service import prepare,SERVER_FILES
    with socket.socket() as tcp,socket.socket() as admin,socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
        for sock in (tcp,admin,udp):sock.bind(('127.0.0.1',0))
        port,admin_port,udp_port=(s.getsockname()[1] for s in (tcp,admin,udp))
    root,plist=prepare(tmp_path/'service','127.0.0.1',directory_port=port,relay_port=udp_port,dashboard_port=admin_port)
    key=(root/'certificate/private-key.pem').read_bytes()
    _,upgraded=prepare(root,'127.0.0.1',directory_port=port,relay_port=udp_port)
    assert upgraded==plist and (root/'certificate/private-key.pem').read_bytes()==key
    assert all((root/'server'/name).is_file() for name in SERVER_FILES)
    process=subprocess.Popen(plist['ProgramArguments'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        deadline=time.monotonic()+7
        while True:
            try:
                connection=http.client.HTTPConnection('127.0.0.1',admin_port,timeout=1)
                connection.request('GET','/api/status');response=connection.getresponse();body=response.read();connection.close()
                assert response.status==200 and json.loads(body)['directory_port']==port
                break
            except OSError:
                if time.monotonic()>deadline or process.poll() is not None:
                    raise
                time.sleep(.05)
    finally:
        process.terminate();process.communicate(timeout=5)
    with pytest.raises(ValueError,match='distinct'):
        prepare(tmp_path/'invalid','127.0.0.1',directory_port=port,dashboard_port=port)
