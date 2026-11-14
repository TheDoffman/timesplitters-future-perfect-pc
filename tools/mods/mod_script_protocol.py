"""Reference worker wire codec for developer tools/tests; never applies game commands.

The native game supervisor must enforce independent process confinement, read/
write deadlines and byte limits before passing a complete reply to its validator.
"""
import struct
import unicodedata

from mod_package import ModError, require

SOURCE_MAX = 1024 * 1024
REPLY_MAX = 32 + 64 * (20 + 512)
INTEGER_MAX = 2**53 - 1


def frame(data):
    require(len(data) <= SOURCE_MAX + 8, 'Worker input exceeds its frame budget')
    return struct.pack('<I', len(data)) + data


def load(source, seed=1):
    require(isinstance(source, bytes) and 0 < len(source) <= SOURCE_MAX, 'Invalid Lua source length')
    require(type(seed) is int and 0 <= seed < 2**32, 'Invalid script seed')
    return frame(struct.pack('<II', 1, seed) + source)


def event(sequence, kind, tick=0, players=(), actor=0, target=0, tick_rate=60):
    body = struct.pack('<IQIQIIII', 2, sequence, kind, tick, tick_rate, actor, target, len(players))
    return frame(body + b''.join(struct.pack('<IIiifff', *p) for p in players))


def quit_worker():
    return frame(struct.pack('<I', 3))


def replies(data):
    require(isinstance(data, bytes), 'Worker reply must be bytes')
    at = 0
    result = []
    while at < len(data):
        require(len(data) - at >= 4, 'Truncated worker frame')
        size, = struct.unpack_from('<I', data, at)
        at += 4
        require(28 <= size <= REPLY_MAX and len(data) - at >= size, 'Invalid worker reply length')
        wire = data[at:at + size]
        at += size
        version, status, sequence, memory, count = struct.unpack_from('<IIQII', wire)
        require(version == 1 and status in (0, 1, 2) and sequence <= INTEGER_MAX
                and memory <= 16 * 1024 * 1024 and count <= 64, 'Invalid worker reply header')
        offset = 24
        commands = []
        for _ in range(count):
            require(len(wire) - offset >= 16, 'Truncated worker command')
            kind, player, value, length = struct.unpack_from('<IIiI', wire, offset)
            offset += 16
            require(kind in (1, 2, 3) and length <= 512 and len(wire) - offset >= length,
                    'Invalid worker command')
            try:
                text = wire[offset:offset + length].decode('utf-8')
            except UnicodeError as exc:
                raise ModError('Invalid worker text encoding') from exc
            offset += length
            require(not any(unicodedata.category(c) == 'Cc' for c in text), 'Invalid worker message controls')
            if kind == 1:
                require(1 <= player <= 32 and -1000000 <= value <= 1000000 and not text, 'Invalid score command')
            elif kind == 2:
                require(player <= 32 and value == 0 and not text, 'Invalid match command')
            else:
                require(player == value == 0 and text, 'Invalid message command')
            commands.append({'kind': kind, 'player': player, 'value': value, 'text': text})
        require(len(wire) - offset >= 4, 'Missing worker diagnostic length')
        length, = struct.unpack_from('<I', wire, offset)
        offset += 4
        require(length <= 255 and len(wire) - offset == length, 'Invalid worker diagnostic')
        try:
            error = wire[offset:].decode('ascii')
        except UnicodeError as exc:
            raise ModError('Invalid worker diagnostic encoding') from exc
        require(all(32 <= ord(c) <= 126 for c in error) and (status == 0) == (not error)
                and (status == 0 or count == 0), 'Invalid worker status or partial transaction')
        result.append({'sequence': sequence, 'status': status, 'memory': memory, 'commands': commands, 'error': error})
    return result


def validate_commands(reply, sequence, player_ids, stopping=False):
    """Validate the entire transaction before the host changes any gameplay state."""
    require(reply['status'] == 0 and reply['sequence'] == sequence, 'Worker failed or replied to another event')
    ids = set(player_ids)
    ended = False
    for command in reply['commands']:
        if command['kind'] in (1, 2):
            require(not stopping and not ended, 'Gameplay command after match end')
            require(command['player'] in ids or (command['kind'] == 2 and command['player'] == 0),
                    'Worker targeted an unknown player')
            if command['kind'] == 2:
                ended = True
    return reply['commands']
