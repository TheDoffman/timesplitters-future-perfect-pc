"""Authenticated Arcade migration rooms, separate from legacy v2/Story rooms."""
import base64
import hmac
import re
import secrets
import struct

from online_migration import ArcadeMigration, MigrationError, SCHEMA

CHECKPOINT_HEADER = struct.Struct('<4I16s4I')
MAX_NATIVE_CHECKPOINT = 65536


class ArcadeServiceError(ValueError):
    def __init__(self, status, message):
        super().__init__(message)
        self.status, self.message = status, message


class ArcadeService:
    """Caller authenticates TLS and holds the parent directory lock throughout."""
    def __init__(self, directory):
        self.directory = directory
        self.rooms = {}
        self.by_id = {}

    def member(self):
        now = self.directory.clock()
        return dict(token=secrets.token_urlsafe(32), wire_key=secrets.token_hex(32),
                    relay_key=secrets.token_hex(32), endpoint=None, sequence=-1,
                    seen=0, budget=None, budget_at=0, expires=now+90,
                    native_counter=-1, native_at=now, active=False)

    def poll(self, room):
        migration = room['migration']
        now = self.directory.clock()
        if migration:
            migration.poll()
            self.sync(room)
        else:
            expired = [peer for peer, member in room['members'].items() if member['expires'] <= now]
            for peer in expired:
                room['members'].pop(peer)
            if room['host_peer'] not in room['members']:
                room['closed'] = True
        if room['closed'] or not room['members']:
            self.rooms.pop(room['code'], None)
            self.by_id.pop(room['room'], None)

    def sync(self, room):
        migration = room['migration']
        if not migration:
            return
        if migration.epoch != room['epoch']:
            room['epoch'] = migration.epoch
            # Retired owners know old wire keys. New epochs replace both game
            # and outer credentials before any surviving native client resumes.
            for member in room['members'].values():
                member.update(wire_key=secrets.token_hex(32), relay_key=secrets.token_hex(32),
                              sequence=-1, endpoint=None, seen=0)
        if migration.phase == 'preparing' and room['host_peer'] is not None:
            room['previous_host'] = room['host_peer']
        room['members'] = {peer: member for peer, member in room['members'].items()
                           if peer in migration.members}
        room['host_peer'] = migration.host_peer
        room['closed'] = migration.phase == 'closed'

    def public(self, room, remote=''):
        address, port = self.directory.relay_address
        if self.directory.relay_lan_address and remote:
            import ipaddress
            if ipaddress.ip_address(remote).is_private:
                address = self.directory.relay_lan_address
        migration = room['migration']
        return dict(code=room['code'], room=room['room'], compatibility=room['compatibility'],
                    name=room['name'], mode='arcade', address=address, port=port,
                    transport='relay', migration_schema=SCHEMA, players=len(room['members']), capacity=16,
                    epoch=room['epoch'], host_peer=room['host_peer'],
                    phase=migration.phase if migration else 'lobby')

    def private(self, room, peer, remote=''):
        member = room['members'][peer]
        result = dict(self.public(room, remote), peer_id=peer, peer_token=member['token'],
                      peer_key=member['wire_key'], relay_key=member['relay_key'])
        migration = room['migration']
        result['match_id'] = migration.match_id if migration else room['room']
        result['candidate'] = migration.candidate if migration else None
        result['previous_host'] = room['previous_host']
        result['backup_ready'] = False
        if peer == room['host_peer'] or migration and peer == migration.candidate:
            result['peers'] = {str(other): value['wire_key'] for other, value in room['members'].items()
                               if other != peer}
        if migration:
            snapshot = migration.selected if migration.phase == 'preparing' else (
                migration.checkpoints[-1] if migration.checkpoints else None)
            if snapshot:
                result['checkpoint'] = dict(sequence=snapshot.sequence, tick=snapshot.tick,
                    digest=snapshot.digest, epoch=snapshot.epoch,
                    payload=base64.b64encode(snapshot.payload).decode('ascii'))
                result['backup_ready'] = any(other != room['host_peer'] and member.can_host and
                    snapshot.digest in member.acknowledged and
                    self.directory.clock()-member.seen <= migration.lease_seconds
                    for other, member in migration.members.items())
        return result

    def start_match(self, room):
        # A new engine round gets a fresh identity. Existing participant IDs and
        # the last elected host remain stable between rounds.
        host = room['host_peer']
        room['previous_host'] = None
        migration = ArcadeMigration(host_peer=host, clock=self.directory.clock)
        migration.epoch = room['epoch']+1
        for peer in room['members']:
            if peer != host:
                migration.join(peer, schema=SCHEMA)
        for peer, member in room['members'].items():
            migration.members[peer].seen = member['native_at']
        room['migration'] = migration
        self.sync(room)

    def update_native(self, room, peer, body):
        member = room['members'][peer]
        counter, active = body.get('native_counter', -1), body.get('active', False)
        if type(counter) is not int or counter < -1 or type(active) is not bool:
            raise ArcadeServiceError(400, 'Invalid native progress')
        fresh = counter > member['native_counter']
        if fresh:
            member.update(native_counter=counter, native_at=self.directory.clock(), active=active)
        if not room['migration'] and peer == room['host_peer'] and fresh and active:
            self.start_match(room)
        migration = room['migration']
        if not migration:
            return
        if fresh:
            migration.heartbeat(peer, migration.epoch)
        if peer == room['host_peer'] and fresh and not active and migration.phase == 'active':
            room['migration'] = None
            return
        # Status requests from the previous epoch can recover current keys but
        # cannot acknowledge, publish or commit state under that old authority.
        if body.get('epoch') != migration.epoch:
            return
        offer = body.get('checkpoint')
        if offer is not None:
            if peer != room['host_peer'] or migration.phase != 'active':
                raise ArcadeServiceError(403, 'Only the active host publishes checkpoints')
            if not isinstance(offer, str) or len(offer) > 90000:
                raise ArcadeServiceError(413, 'Checkpoint exceeds native limit')
            payload = base64.b64decode(offer, validate=True)
            if not CHECKPOINT_HEADER.size <= len(payload) <= MAX_NATIVE_CHECKPOINT:
                raise ArcadeServiceError(400, 'Invalid native checkpoint length')
            magic, schema, size, seq, match_id, epoch, level, tick, phase = CHECKPOINT_HEADER.unpack_from(payload)
            if (magic != 0x314d4341 or schema != 1 or size != len(payload) or
                match_id.hex() != migration.match_id or epoch != migration.epoch or level > 100 or phase != 7):
                raise ArcadeServiceError(409, 'Native checkpoint does not match the active round')
            # Retries of an already committed native offer are idempotent.
            if not migration.checkpoints or seq > migration.checkpoints[-1].sequence:
                migration.publish(peer, epoch, seq, tick, payload)
            elif seq == migration.checkpoints[-1].sequence and payload != migration.checkpoints[-1].payload:
                raise ArcadeServiceError(409, 'Checkpoint sequence cannot identify different state')
        ack = body.get('checkpoint_ack')
        if ack is not None:
            if not isinstance(ack, dict):
                raise ArcadeServiceError(400, 'Invalid native acknowledgement')
            migration.acknowledge(peer, migration.epoch, ack.get('sequence'), ack.get('digest'))
        ready = body.get('ready')
        if ready is not None:
            migration.ready(peer, migration.epoch, ready)
        self.sync(room)

    def request(self, method, path, query, body, remote, token):
        try:
            return self._request(method, path, query, body, remote, token)
        except MigrationError as error:
            raise ArcadeServiceError(409, str(error)) from error

    def _request(self, method, path, query, body, remote, token):
        for room in list(self.rooms.values()):
            self.poll(room)
        if not self.directory.relay_address:
            raise ArcadeServiceError(503, 'Arcade migration requires a shared relay')
        if path == '/v3/lobbies' and method == 'GET':
            version = self.directory.version(query.get('compatibility', [''])[0])
            return 200, dict(lobbies=[self.public(room, remote) for room in self.rooms.values()
                if room['public'] and room['compatibility'] == version and not room['closed']])
        if path == '/v3/lobbies' and method == 'POST':
            version = self.directory.version(body.get('compatibility'))
            name = body.get('name')
            if (body.get('migration_schema') != SCHEMA or body.get('mode', 'arcade') != 'arcade' or
                not isinstance(name, str) or not 1 <= len(name) <= 48 or
                any(not 32 <= ord(c) <= 126 for c in name) or type(body.get('public', False)) is not bool):
                raise ArcadeServiceError(400, 'Invalid migrating Arcade lobby')
            if len(self.rooms)+len(self.directory.online) >= 256 or sum(
                room['origin'] == remote for room in self.rooms.values()) >= 8:
                raise ArcadeServiceError(429, 'Lobby limit reached')
            code = secrets.token_hex(8).upper()
            while code in self.rooms:
                code = secrets.token_hex(8).upper()
            room = dict(code=code, room=secrets.token_hex(16), name=name, compatibility=version,
                public=body.get('public', False), origin=remote, members={1:self.member()}, next_peer=2,
                host_peer=1, previous_host=None, epoch=1, migration=None, closed=False)
            self.rooms[code] = room
            self.by_id[room['room']] = room
            return 201, self.private(room, 1, remote)
        match = re.fullmatch(r'/v3/lobbies/([a-fA-F0-9]{16})(/join|/members/(\d+))', path)
        if not match:
            raise ArcadeServiceError(404, 'Unknown Arcade endpoint')
        room = self.rooms.get(match[1].upper())
        if not room:
            raise ArcadeServiceError(410, 'Arcade lobby closed or expired')
        if match[2] == '/join' and method == 'POST':
            if self.directory.version(body.get('compatibility')) != room['compatibility']:
                raise ArcadeServiceError(409, 'Incompatible game version or content')
            if body.get('migration_schema') != SCHEMA:
                raise ArcadeServiceError(409, 'Native Arcade migration support is required')
            # The stock title does not support joining an active Arcade match.
            if room['migration']:
                raise ArcadeServiceError(409, 'Match in progress; join from its lobby')
            if len(room['members']) >= 16 or room['next_peer'] > 254:
                raise ArcadeServiceError(409, 'Lobby full or peer IDs exhausted')
            peer = room['next_peer'];room['next_peer'] += 1
            room['members'][peer] = self.member()
            return 201, self.private(room, peer, remote)
        peer = int(match[3]) if match[3] else 0
        member = room['members'].get(peer)
        if not member or not token or not hmac.compare_digest(member['token'].encode(), token.encode()):
            raise ArcadeServiceError(403, 'Invalid Arcade member credential')
        if method == 'DELETE':
            if room['migration']:
                room['migration'].leave(peer, room['epoch'])
                self.sync(room)
            else:
                del room['members'][peer]
                if peer == room['host_peer']:
                    room['closed'] = True
            if room['closed'] or not room['members']:
                self.rooms.pop(room['code'], None)
                self.by_id.pop(room['room'], None)
            return 200, dict(closed=True)
        if method != 'PUT':
            raise ArcadeServiceError(405, 'Method not allowed')
        if self.directory.version(body.get('compatibility')) != room['compatibility']:
            raise ArcadeServiceError(409, 'Incompatible game version or content')
        self.update_native(room, peer, body)
        if room['closed'] or peer not in room['members']:
            raise ArcadeServiceError(410, 'Arcade lobby closed or member retired')
        member['expires'] = self.directory.clock()+90
        return 200, self.private(room, peer, remote)

    def relay_room(self, room_id):
        room = self.by_id.get(room_id.hex())
        if room:
            self.poll(room)
            if room['closed'] or room['code'] not in self.rooms:
                return None
        return room
