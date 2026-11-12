"""The in-game host action starts the former player-owned directory directly."""
from types import SimpleNamespace
import socket

import pytest

import online_menu as menu
from online_client import Client, DirectoryError, Session
from online_private import Directory, LobbyError, LocalDirectory, certificate, start_directory


VERSION = 'a' * 64


def request(action, sequence=1, **values):
    data = dict(sequence=sequence, action=action, mode=1, source=1, public=True,
                port=0, name='My Lobby', code='', address='192.0.2.1', fingerprint='')
    if action == menu.JOIN:data['address'] = ''
    data.update(values)
    return data


def wait(controller):
    import time
    deadline = time.monotonic() + 20
    while controller.status == menu.BUSY and time.monotonic() < deadline:
        time.sleep(.01)
    assert controller.status != menu.BUSY
    assert controller.status != menu.ERROR, controller.message


def udp_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def test_native_host_starts_directory_before_any_lobby_and_retains_other_hosts(tmp_path, monkeypatch):
    monkeypatch.setattr(menu, 'ROOT', tmp_path)
    monkeypatch.delenv('TSFP_PRIVATE_SERVER_COMMAND', raising=False)
    parent = SimpleNamespace(exited=lambda: False)
    owner = menu.Controller(VERSION, menu.private_launcher(tmp_path/'result', parent))
    friend = menu.Controller(VERSION)
    guest = menu.Controller(VERSION)
    directory = None
    try:
        owner.begin(request(menu.PRIVATE_HOST)); wait(owner)
        directory = owner.private_process
        assert isinstance(directory, LocalDirectory) and directory.poll() is None
        assert owner.status == menu.PRIVATE_READY and owner.private_direct and owner.private_relay
        assert owner.session is None and directory.server.directory.online == {}
        shared=menu.STATE.unpack(owner.snapshot()[:menu.STATE.size])[10].split(b'\0',1)[0].decode()
        assert menu.parse_server_details(shared)==(owner.private,directory.lan_addresses)
        assert menu.parse_server_invitation(menu.server_invitation(*owner.private)) == owner.private
        client = Client(directory.address, directory.fingerprint)
        assert client.request('GET', '/v2/lobbies?compatibility='+VERSION)['private_directory'] is True

        owner.begin(request(menu.CREATE, 2, port=udp_port())); wait(owner)
        assert isinstance(owner.session, Session) and owner.session.bridge.relay is not None
        assert owner.session.lobby['capacity'] == 16
        code = owner.session.lobby['code']

        invitation = menu.server_invitation(directory.public_url, directory.fingerprint)
        friend.begin(request(menu.PRIVATE_CONNECT, code=invitation)); wait(friend)
        assert friend.private_direct and friend.private_relay and friend.private == owner.private
        assert friend.directory(1).url.geturl() == directory.address
        friend.begin(request(menu.CREATE, 2, mode=2, port=udp_port(), name='Story team')); wait(friend)
        assert friend.session.lobby['capacity'] == 4

        guest.private = (directory.address, directory.fingerprint)
        guest.begin(request(menu.JOIN, code=code)); wait(guest)
        assert guest.session.bridge.ready.is_set()
        guest.begin(request(menu.LEAVE, 2)); wait(guest)
        assert directory.poll() is None

        owner.begin(request(menu.LEAVE, 3)); wait(owner)
        assert directory.poll() is None and owner.session is None
        owner.begin(request(menu.BROWSE, 4)); wait(owner)
        assert [row['name'] for row in owner.lobbies] == ['Story team']
        assert owner.private_process is directory
    finally:
        guest.close()
        friend.close()
        owner.close()
    assert directory is not None and directory.poll() == 0


def test_directory_certificate_is_reused_and_incomplete_pair_is_preserved(tmp_path):
    folder = tmp_path/'certificate'
    cert, key, pin = certificate(folder)
    original = cert.read_bytes(), key.read_bytes()
    assert certificate(folder) == (cert, key, pin)
    assert (cert.read_bytes(), key.read_bytes()) == original
    key.unlink()
    with pytest.raises(ValueError, match='incomplete'):
        certificate(folder)
    assert cert.read_bytes() == original[0] and not key.exists()


def test_default_host_detects_address_and_can_isolate_its_state(tmp_path, monkeypatch):
    import online_private
    state = tmp_path/'isolated-certificates'
    monkeypatch.setattr(online_private, 'fetch_public_ipv4', lambda: '192.0.2.1')
    monkeypatch.setenv('TSFP_PRIVATE_DIRECTORY_STATE', str(state))
    directory = start_directory(tmp_path, request(menu.PRIVATE_HOST, address=''))
    try:
        assert directory.public_url.startswith('https://192.0.2.1:')
        assert (state/'certificate.pem').is_file()
        assert not (tmp_path/'private/online-directory').exists()
    finally:
        directory.close()


@pytest.mark.parametrize('mode,capacity', [('arcade', 16), ('story', 4)])
def test_directory_credentials_capacity_expiry_and_match_metadata(mode, capacity):
    now = [0.0]
    directory = Directory(clock=lambda: now[0], public_address='192.0.2.1')
    body = dict(compatibility=VERSION, name='Match', port=41831, public=True, mode=mode)
    _, lobby = directory.request('POST', '/v2/lobbies', {}, body, '127.0.0.1', '')
    path = '/v2/lobbies/'+lobby['code']
    info = dict(map_scene=2, game_type_id=2, in_progress=False,
                map_name='Time to Split', game_type='Story')
    with pytest.raises(LobbyError) as error:
        directory.request('DELETE', path, {}, {}, '127.0.0.1', 'wrong')
    assert error.value.status == 403
    directory.request('PUT', path, {}, {**body, 'match_info': info}, '127.0.0.1', lobby['owner_token'])
    _, listed = directory.request('GET', '/v2/lobbies',
        dict(compatibility=[VERSION], mode=[mode]), {}, '127.0.0.1', '')
    assert listed['lobbies'][0]['map_name'] == 'Time to Split'
    assert 'owner_token' not in listed['lobbies'][0] and 'peers' not in listed['lobbies'][0]
    for _ in range(capacity-1):
        directory.request('POST', path+'/join', {}, body, '127.0.0.1', '')
    with pytest.raises(LobbyError) as error:
        directory.request('POST', path+'/join', {}, body, '127.0.0.1', '')
    assert error.value.status == 409
    now[0] = 91
    _, listed = directory.request('GET', '/v2/lobbies',
        dict(compatibility=[VERSION], mode=[mode]), {}, '127.0.0.1', '')
    assert listed['lobbies'] == []


def test_tls_pin_and_failed_private_start_do_not_leave_a_listener(tmp_path):
    directory = LocalDirectory(tmp_path, '192.0.2.1', 0)
    try:
        with pytest.raises(ValueError, match='fingerprint'):
            Client(directory.address, '0'*64).request('GET', '/v2/lobbies?compatibility='+VERSION)
        with pytest.raises(DirectoryError) as error:
            Client(directory.address, directory.fingerprint).request('GET', '/v3/lobbies?compatibility='+VERSION)
        assert error.value.status == 404
    finally:
        directory.close()
    directory.close()
    assert directory.poll() == 0


@pytest.mark.parametrize('mode,capacity', [('arcade', 16), ('story', 4)])
def test_invite_only_is_listed_but_public_id_cannot_admit_a_player(mode, capacity):
    directory = Directory()
    body = dict(compatibility=VERSION, name='Locked match', port=41831, public=False, mode=mode)
    _, hosted = directory.request('POST', '/v2/lobbies', {}, body, '127.0.0.1', '')
    _, other = directory.request('POST', '/v2/lobbies', {}, body, '127.0.0.1', '')
    path = '/v2/lobbies/'+hosted['code']+'/join'
    _, reply = directory.request('GET', '/v2/lobbies',
        dict(compatibility=[VERSION], mode=[mode]), {}, '127.0.0.1', '')
    listing = reply['lobbies'][0]
    assert len(reply['lobbies']) == 2 and listing['invite_only'] and listing['capacity'] == capacity
    assert hosted['code'] != hosted['lobby_code']
    assert hosted['lobby_code'] not in str(reply) and 'owner_token' not in str(reply)
    for invalid in ({}, {'lobby_code': hosted['code']}, {'lobby_code': other['lobby_code']}, {'lobby_code': 123}, {'lobby_code': chr(233)*16}):
        with pytest.raises(LobbyError) as error:
            directory.request('POST', path, {}, {**body, **invalid}, '127.0.0.1', '')
        assert error.value.status == 403
    assert directory.online[hosted['code']]['peers'] == {}
    _, joined = directory.request('POST', path, {}, {**body, 'lobby_code': hosted['lobby_code'].lower()}, '127.0.0.1', '')
    assert joined['players'] == 2 and 'lobby_code' not in joined
    # Legacy code-only invitations resolve inside the same directory, while
    # subsequent heartbeat/leave uses the canonical public ID from admission.
    _, invited = directory.request('POST', '/v2/lobbies/'+hosted['lobby_code']+'/join', {}, body, '127.0.0.1', '')
    assert invited['code'] == hosted['code'] and invited['players'] == 3


def test_native_locked_listing_and_code_admission_over_real_tls_and_udp(tmp_path):
    import struct
    directory = LocalDirectory(tmp_path, '192.0.2.1', 0)
    owner, guest = menu.Controller(VERSION), menu.Controller(VERSION)
    try:
        owner.private = guest.private = (directory.address, directory.fingerprint)
        owner.private_direct = owner.private_relay = True
        owner.begin(request(menu.CREATE, port=udp_port(), public=False)); wait(owner)
        hosted = owner.session.lobby
        guest.begin(request(menu.BROWSE)); wait(guest)
        assert len(guest.lobbies) == 1 and guest.lobbies[0]['invite_only']
        assert menu.LOBBY.unpack_from(guest.snapshot(), menu.STATE.size)[6] == 1
        assert hosted['lobby_code'].encode() not in guest.snapshot()
        assert menu.STATE.unpack_from(owner.snapshot())[9].split(b'\0')[0].decode() == hosted['lobby_code']
        guest.begin(request(menu.JOIN, 2, code=hosted['code'], address=hosted['code']))
        import time
        deadline = time.monotonic()+5
        while guest.status == menu.BUSY and time.monotonic() < deadline:time.sleep(.01)
        assert guest.status == menu.ERROR and guest.session is None
        guest.begin(request(menu.JOIN, 3, code=hosted['lobby_code'], address=hosted['code'])); wait(guest)
        assert guest.session.bridge.ready.is_set() and guest.session.lobby['code'] == hosted['code']
        assert menu.STATE.unpack_from(guest.snapshot())[9].split(b'\0')[0].decode() == hosted['lobby_code']
        guest.begin(request(menu.LEAVE, 4)); wait(guest)
        assert directory.poll() is None
    finally:
        guest.close(); owner.close(); directory.close()


@pytest.mark.parametrize('origin', ['127.0.0.1','192.168.1.7'])
@pytest.mark.parametrize('remote,local_address,expected', [
    ('192.168.1.9','192.168.1.8','lan'),
    ('127.0.0.1','127.0.0.1','origin'),
    ('8.8.8.8','192.168.1.8','public'),
    ('192.168.1.9','8.8.4.4','public')])
def test_private_match_address_follows_actual_connection_route(origin,remote,local_address,expected):
    directory=Directory(public_address='8.8.4.4')
    body=dict(compatibility=VERSION,name='Match',port=41831,public=True,mode='arcade')
    _,hosted=directory.request('POST','/v2/lobbies',{},body,origin,'')
    _,listing=directory.request('GET','/v2/lobbies',{'compatibility':[VERSION]}, {},remote,'',local_address)
    address=(origin if origin!='127.0.0.1' else local_address) if expected=='lan' else origin if expected=='origin' else '8.8.4.4'
    assert listing['lobbies'][0]['address']==address
    _,joined=directory.request('POST','/v2/lobbies/'+hosted['code']+'/join',{},body,remote,'',local_address)
    assert joined['address']==address


@pytest.mark.parametrize('transport', ['direct', 'relay'])
def test_remote_private_invite_avoids_public_hairpin_for_tls_and_match_udp(tmp_path,monkeypatch,transport):
    from online_host import local_ipv4_addresses
    addresses=local_ipv4_addresses('192.0.2.1')
    if not addresses:pytest.skip('No RFC1918 interface available for real LAN route test')
    directory=LocalDirectory(tmp_path,'192.0.2.1',0,state_folder=tmp_path/'certificate')
    host=guest=None
    controller=menu.Controller(VERSION)
    class RemoteClient(Client):
        def request(self,*args,**kwargs):
            if self.url.hostname in ('127.0.0.1','192.0.2.1'):
                raise ConnectionRefusedError('same-machine and router-hairpin routes unavailable')
            return super().request(*args,**kwargs)
    try:
        host=Session.create(Client(directory.address,directory.fingerprint),VERSION,'Windows host',udp_port(),True,transport=transport)
        monkeypatch.setattr(menu,'Client',RemoteClient)
        code=menu.server_invitation(directory.public_url,directory.fingerprint,local_addresses=addresses)
        controller.begin(request(menu.PRIVATE_CONNECT,code=code));wait(controller)
        endpoint=controller.directory(1)
        assert endpoint.url.hostname in addresses
        listing=endpoint.request('GET','/v2/lobbies?compatibility='+VERSION)
        assert listing['lobbies'][0]['address']==endpoint.url.hostname
        guest=Session.join(endpoint,VERSION,host.lobby['code'])
        assert guest.bridge.ready.is_set() and guest.lobby['address']==endpoint.url.hostname
    finally:
        if guest:guest.close()
        if host:host.close()
        controller.close();directory.close()


def test_relay_credentials_isolation_replay_rebinding_and_expiry():
    import hmac
    from online_transport import RELAY, REGISTER, REGISTERED, FORWARD, relay_packet
    now = [0.0]
    directory = Directory(clock=lambda: now[0], public_address='8.8.4.4', relay_port=30821)
    body = dict(compatibility=VERSION, name='Relay', port=30821, public=True, transport='relay')
    def rpc(method, path, data=body, token=''):
        return directory.request(method, path, {'compatibility':[VERSION]}, data, '8.8.8.8', token)[1]
    first = rpc('POST', '/v2/lobbies')
    second = rpc('POST', '/v2/lobbies')
    path = '/v2/lobbies/'+first['code']
    guest = rpc('POST', path+'/join')
    other = rpc('POST', path+'/join')
    listing = rpc('GET', '/v2/lobbies', {})
    assert listing['private_relay'] and first['port'] == 30821
    assert first['relay_key'] not in str(listing) and guest['relay_key'] not in str(listing)
    assert first['relay_key'] != guest['relay_key'] != guest['peer_key']
    room = bytes.fromhex(first['room'])
    host_key, guest_key = bytes.fromhex(first['relay_key']), bytes.fromhex(guest['relay_key'])
    host_ep, guest_ep = ('8.8.8.8', 40001), ('1.1.1.1', 40002)
    host_reg = relay_packet(room, 1, 1, 0, REGISTER, host_key)
    assert directory.relay_datagram(b'bad', host_ep) is None
    assert directory.relay_datagram(host_reg[:-1]+bytes([host_reg[-1]^1]), host_ep) is None
    response, endpoint = directory.relay_datagram(host_reg, host_ep)
    assert endpoint == host_ep and RELAY.unpack_from(response)[-1] == REGISTERED
    assert hmac.compare_digest(hmac.digest(host_key, response[:-32], 'sha256'), response[-32:])
    assert directory.relay_datagram(host_reg, ('9.9.9.9', 1234)) is None
    directory.relay_datagram(relay_packet(room, 1, guest['peer_id'], 0, REGISTER, guest_key), guest_ep)
    packet = relay_packet(room, 2, guest['peer_id'], 1, FORWARD, guest_key, b'opaque game packet')
    assert directory.relay_datagram(packet, ('9.9.9.9', 1234)) is None
    packet = relay_packet(room, 3, guest['peer_id'], 1, FORWARD, guest_key, b'opaque game packet')
    response, endpoint = directory.relay_datagram(packet, guest_ep)
    assert endpoint == host_ep and response[RELAY.size:-32] == b'opaque game packet'
    assert hmac.compare_digest(hmac.digest(host_key, response[:-32], 'sha256'), response[-32:])
    assert directory.relay_datagram(packet, guest_ep) is None
    # A credential from another room cannot register/forward in this one.
    assert directory.relay_datagram(relay_packet(bytes.fromhex(second['room']), 4,
        guest['peer_id'], 1, FORWARD, guest_key, b'cross room'), guest_ep) is None
    assert directory.relay_datagram(relay_packet(room, 4, guest['peer_id'],
        other['peer_id'], FORWARD, guest_key, b'bypass host'), guest_ep) is None
    # Authenticated registration can rebind after the client's NAT changes.
    new_ep = ('1.1.1.1', 40003)
    assert directory.relay_datagram(relay_packet(room, 5, guest['peer_id'], 0, REGISTER, guest_key), new_ep)[1] == new_ep
    member = directory.online[first['code']]['relay_members'][guest['peer_id']]
    member['budget'] = 0
    assert directory.relay_datagram(relay_packet(room, 6, guest['peer_id'], 0, REGISTER, guest_key), new_ep) is None
    member['budget'] = 16384
    # Stale registration is not a forwarding target, even while TLS lease lives.
    now[0] = 6
    assert directory.relay_datagram(relay_packet(room, 7, guest['peer_id'], 1,
        FORWARD, guest_key, b'stale'), new_ep) is None
    directory.relay_datagram(relay_packet(room, 2, 1, 0, REGISTER, host_key), host_ep)
    directory.relay_datagram(relay_packet(room, 8, guest['peer_id'], 0, REGISTER, guest_key), new_ep)
    rpc('DELETE', path+'/peers/'+str(guest['peer_id']), {}, guest['peer_key'])
    assert directory.relay_datagram(relay_packet(room, 9, guest['peer_id'], 0, REGISTER, guest_key), new_ep) is None
    rpc('DELETE', path, {}, first['owner_token'])
    assert directory.relay_datagram(relay_packet(room, 3, 1, 0, REGISTER, host_key), host_ep) is None
    now[0] = 91
    assert directory.relay_datagram(relay_packet(bytes.fromhex(second['room']), 1, 1, 0,
        REGISTER, bytes.fromhex(second['relay_key'])), host_ep) is None


def test_private_relay_delivers_game_packets_between_guests_and_releases_listeners(tmp_path, monkeypatch):
    import time
    from online_transport import LOCAL, LOCAL_MAGIC, virtual_ip
    import online_client
    monkeypatch.setattr(online_client, 'map_port', lambda *args: pytest.fail('Lobby host must not map a port'))
    directory = LocalDirectory(tmp_path, '192.0.2.1', 0)
    sessions, games = [], []
    tcp_port, relay_port = directory.server.server_port, directory.relay.port
    try:
        client = Client(directory.address, directory.fingerprint)
        host = Session.create(client, VERSION, 'Relay room', relay_port, True, transport='relay')
        sessions.append(host)
        sessions.extend(Session.join(client, VERSION, host.lobby['code']) for _ in range(2))
        for active in sessions:
            bridge = active.bridge
            assert bridge.relay == ('127.0.0.1', relay_port)
            game = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            games.append(game); game.bind(('127.0.0.1', 0)); game.settimeout(3)
            game.sendto(LOCAL.pack(LOCAL_MAGIC, bytes.fromhex(VERSION), bridge.local_token,
                virtual_ip(bridge.peer_id), bytes(4), 1000, 0), bridge.local.getsockname())
            deadline = time.monotonic()+3
            while 1000 not in bridge.ports and time.monotonic() < deadline: time.sleep(.01)
            assert 1000 in bridge.ports
        first, second = sessions[1].bridge, sessions[2].bridge
        games[1].sendto(LOCAL.pack(LOCAL_MAGIC, bytes.fromhex(VERSION), first.local_token,
            virtual_ip(first.peer_id), virtual_ip(second.peer_id), 1000, 1000)+b'private relay gameplay', first.local.getsockname())
        deadline = time.monotonic()+3
        while time.monotonic() < deadline:
            packet = games[2].recv(8192)
            if LOCAL.unpack_from(packet)[-1] != 0: break
        assert packet[LOCAL.size:] == b'private relay gameplay'
    finally:
        for game in games: game.close()
        for active in reversed(sessions): active.close()
        directory.close()
    assert directory.poll() == 0
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('0.0.0.0', relay_port))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        assert probe.connect_ex(('127.0.0.1', tcp_port)) != 0


def test_busy_relay_port_rolls_back_directory_listener(tmp_path):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as busy:
        busy.bind(('0.0.0.0', 0))
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as available:
            available.bind(('127.0.0.1', 0)); tcp = available.getsockname()[1]
        with pytest.raises(OSError):
            LocalDirectory(tmp_path, '192.0.2.1', tcp, relay_port=busy.getsockname()[1])
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            assert probe.connect_ex(('127.0.0.1', tcp)) != 0
