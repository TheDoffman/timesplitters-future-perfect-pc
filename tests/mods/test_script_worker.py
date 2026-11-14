"""Actual native Lua worker adversarial tests; no game data or network required."""
import importlib.util
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

import pytest

from mod_package import ModError
import mod_script_protocol as protocol

ROOT = Path(__file__).resolve().parents[2]
PLAYERS = [(1, 0, 0, 100, 0., 1., 2.), (2, 1, 0, 100, 2., 3., 4.)]


@pytest.fixture(scope='session')
def worker(tmp_path_factory):
    spec = importlib.util.spec_from_file_location('build_mod_worker', ROOT / 'tools/build/build-mod-worker.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    local = Path(sys.executable).with_name('cmake.exe' if os.name == 'nt' else 'cmake')
    cmake = str(local) if local.is_file() else shutil.which('cmake')
    assert cmake, 'Install the repository-pinned CMake to validate the native mod worker'
    return builder.build(tmp_path_factory.mktemp('mod-worker'), cmake,
                         sanitize=os.environ.get('TSFP_MOD_SANITIZE') == '1')


def run(worker, source, events=(), timeout=3):
    data = protocol.load(source.encode() if isinstance(source, str) else source)
    data += b''.join(events) + protocol.quit_worker()
    result = subprocess.run([str(worker)], input=data, capture_output=True, timeout=timeout)
    assert result.stderr == b'', result.stderr[:4096]
    return result, protocol.replies(result.stdout)


def test_real_custom_mode_updates_score_and_finishes_match(worker):
    source = (ROOT / 'examples/mods/first-to-three/scripts/main.lua').read_text()
    events = [protocol.event(1, 1, players=PLAYERS)]
    events += [protocol.event(i + 2, 3, tick=i + 1, players=PLAYERS, actor=1, target=2) for i in range(3)]
    events += [protocol.event(5, 4, tick=4, players=PLAYERS)]
    result, replies = run(worker, source, events)
    assert result.returncode == 0 and len(replies) == 6
    for i, reply in enumerate(replies[1:], 1):
        protocol.validate_commands(reply, i, [1, 2], stopping=i == 5)
    assert [reply['commands'][0]['value'] for reply in replies[2:5]] == [1, 2, 3]
    assert replies[4]['commands'][-1] == dict(kind=2, player=1, value=0, text='')


@pytest.mark.parametrize('name', ['io','os','package','debug','coroutine','require','dofile','loadfile','load',
                                 'collectgarbage','pcall','xpcall','print','warn','setmetatable','getmetatable','rawset'])
def test_forbidden_libraries_and_escape_helpers_absent(worker, name):
    result, replies = run(worker, f'assert({name} == nil); return {{api=1}}')
    assert result.returncode == 0 and replies[0]['status'] == 0


@pytest.mark.parametrize('source,error', [
    ('return {api=2}', 'unsupported script API'),
    ('return {api=1,on_tik=function() end}', 'unknown callback'),
    ('return {api=1,on_tick=42}', 'callback must be'),
    ('return true', 'callback table'),
    ('while true do end', 'instruction budget'),
    ('local a={} while true do a[#a+1]=string.rep("x",1000000) end', 'memory budget'),
    ('return {api=1,on_start=function() game.message("queued"); error("boom") end}', 'boom'),
    ('return {api=1,on_start=function() while true do end end}', 'instruction budget'),
    ('return {api=1,on_start=function() for i=1,65 do game.message("too many") end end}', 'host-call budget'),
    ('return {api=1,on_start=function() game.set_score(32,1) end}', 'unknown player'),
    ('return {api=1,on_start=function() game.set_score(1,1000001) end}', 'out of range'),
    ('return {api=1,on_start=function() game.set_score("1",1) end}', 'requires an integer'),
    ('return {api=1,on_start=function() game.message(string.rep("x",513)) end}', 'invalid message'),
    ('return {api=1,on_start=function() game.message("bad\\0text") end}', 'invalid message'),
    ('return {api=1,on_start=function() game.message("\\255") end}', 'invalid message'),
    ('return {api=1,on_start=function() game.end_match(1);game.set_score(1,4) end}', 'after match end'),
], ids=['api','typo','callback','module','load-loop','memory','transaction','callback-loop','calls',
        'unknown-player','score-range','score-type','message-size','message-control','message-utf8','end-order'])
def test_faults_are_terminal_and_never_commit_partial_commands(worker, source, error):
    result, replies = run(worker, source, [protocol.event(1, 1, players=PLAYERS)])
    assert result.returncode == 2 and replies[-1]['status'] != 0
    assert error in replies[-1]['error'] and replies[-1]['commands'] == []


@pytest.mark.parametrize('events', [
    [protocol.event(1, 2)], [protocol.event(2, 1)],
    [protocol.event(1, 1), protocol.event(2, 1)],
    [protocol.event(1, 1), protocol.event(2, 2, tick=2), protocol.event(3, 2, tick=2)],
    [protocol.event(1, 1, players=[PLAYERS[0], PLAYERS[0]])],
    [protocol.event(1, 1, players=[(1, 0, 0, 100, float('nan'), 0., 0.)])],
    [protocol.event(1, 1), protocol.event(2, 3, target=0)],
], ids=['tick-before-start','sequence','double-start','double-tick','duplicate-id','nan-position','death-without-target'])
def test_game_event_validation(worker, events):
    result, replies = run(worker, 'return {api=1}', events)
    assert result.returncode == 2 and 'event' in replies[-1]['error']


def test_snapshot_copies_rng_and_upstream_numeric_fixes(worker):
    source = '''assert(string.dump == nil and math.random == nil and math.randomseed == nil)
for i=1,0/0,-1 do error("NaN loop ran") end
assert(1/(-0.0 - 0) < 0)
return {api=1,on_start=function()
 local p=game.players(); assert(p[1].health==100); p[1].health=0
 assert(game.players()[1].health==100)
 game.set_score(1,game.random(1,1000));game.message("Hello 世界")
end}'''
    first, output = run(worker, source, [protocol.event(1, 1, players=PLAYERS)])
    second, again = run(worker, source, [protocol.event(1, 1, players=PLAYERS)])
    assert first.returncode == second.returncode == 0
    assert output[-1]['commands'] == again[-1]['commands']
    assert output[-1]['commands'][-1]['text'] == 'Hello 世界'
    assert max(reply['memory'] for reply in output) <= 16 * 1024 * 1024


def test_host_tick_values_remain_exact_64_bit_integers(worker):
    source = 'return {api=1,on_start=function(e) assert(e.tick == 9007199254740990 and math.type(e.tick) == "integer") end}'
    result, replies = run(worker, source, [protocol.event(1, 1, tick=9007199254740990)])
    assert result.returncode == 0 and replies[-1]['status'] == 0


@pytest.mark.parametrize('source', [b'\x1bLua\x55', b'return {api=1}\0trailing'])
def test_binary_and_hidden_source_rejected(worker, source):
    result, replies = run(worker, source)
    assert result.returncode == 2 and 'source-only' in replies[-1]['error']


def test_oversized_and_truncated_protocol_frames_rejected(worker):
    for data in [struct.pack('<I', 2**32 - 1), b'\x01', struct.pack('<I', 400) + b'short']:
        result = subprocess.run([str(worker)], input=data, capture_output=True, timeout=2)
        assert result.returncode == 2 and len(result.stdout) < 1024


def test_wall_watchdog_bounds_expensive_c_pattern_functions(worker):
    source = 'return {api=1,on_start=function() local s=string.rep("a",100000); s:match("a*a*a*a*a*b") end}'
    start = time.monotonic()
    result, replies = run(worker, source, [protocol.event(1, 1)])
    assert result.returncode == 124 and time.monotonic() - start < 2
    assert len(replies) == 1  # Only load acknowledged; no partial event committed.


def test_parent_rejects_untrusted_reply_sequence_and_ids(worker):
    _, replies = run(worker, 'return {api=1,on_start=function() game.set_score(1,5) end}', [protocol.event(1, 1, players=PLAYERS)])
    with pytest.raises(ModError, match='another event'):
        protocol.validate_commands(replies[-1], 2, [1, 2])
    with pytest.raises(ModError, match='unknown player'):
        protocol.validate_commands(replies[-1], 1, [2])


def test_inside_checkout_build_applies_the_identical_pinned_fixes(tmp_path):
    # Git previously returned success but skipped patches when stage lived in an
    # ignored build path. Compare both locations to the pinned post-fix hashes.
    spec = importlib.util.spec_from_file_location('mod_worker_prepare_test', ROOT / 'tools/build/build-mod-worker.py')
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    import tempfile
    (ROOT / 'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='mod-worker-prepare-', dir=ROOT / 'build') as local:
        outside = builder.prepare_lua(tmp_path)
        inside = builder.prepare_lua(Path(local))
        assert builder.sha(outside / 'lvm.c') == builder.sha(inside / 'lvm.c')
        assert 'else if (luai_numgt(0, flim))' in (inside / 'lvm.c').read_text()
        (inside / 'lvm.c').write_text('changed')
        with pytest.raises(ValueError, match='changed'):
            builder.prepare_lua(Path(local))


@pytest.mark.parametrize('kind', ['version','sequence','count','memory','opcode','diagnostic','length'])
def test_parent_rejects_hostile_worker_replies(kind):
    data = bytearray(struct.pack('<IIQII', 1, 0, 1, 64, 0) + struct.pack('<I', 0))
    if kind == 'version': struct.pack_into('<I', data, 0, 2)
    elif kind == 'sequence': struct.pack_into('<Q', data, 8, 2**53)
    elif kind == 'count': struct.pack_into('<I', data, 20, 65)
    elif kind == 'memory': struct.pack_into('<I', data, 16, 16 * 1024 * 1024 + 1)
    elif kind == 'opcode':
        struct.pack_into('<I', data, 20, 1)
        data = data[:24] + struct.pack('<IIiI', 99, 0, 0, 0) + struct.pack('<I', 0)
    elif kind == 'diagnostic':
        struct.pack_into('<I', data, 4, 1)
        struct.pack_into('<I', data, 24, 1)
        data += b'\xff'
    else:
        data += b'extra bytes'
    with pytest.raises(ModError):
        protocol.replies(struct.pack('<I', len(data)) + data)
