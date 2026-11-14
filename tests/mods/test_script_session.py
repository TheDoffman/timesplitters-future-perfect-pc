"""Persistent native session, hostile RPC and nonblocking host API checks."""
import json
from pathlib import Path
import struct
import subprocess
import time

import pytest

from test_script_process import process_tools  # Shared session-scoped native build.

ROOT = Path(__file__).resolve().parents[2]


def run(tools, tmp_path, source, scenario='fault', hostile=False):
    suffix = tools['worker'].suffix
    driver = tools['worker'].with_name('tsfp-mod-session-driver' + suffix)
    worker = tools['worker'].with_name('tsfp-mod-session-worker' + suffix) if hostile else tools['worker']
    result = subprocess.run([str(driver), str(worker), str(tmp_path), scenario],
                            input=source.encode() if isinstance(source, str) else source,
                            capture_output=True, timeout=40)
    assert result.returncode == 0, (result.returncode, result.stdout[:2048], result.stderr[:2048])
    assert result.stderr == b''
    records = [json.loads(line) for line in result.stdout.splitlines()]
    if records[0].get('refused'):
        return records
    assert records[-1]['stopped'] is True
    # This rejects doing the independent 500ms callback wait on the host API.
    # OS scheduling can preempt a tiny call, so this is not a frame-time promise.
    assert records[-1]['max_api_us'] < 100000, records[-1]
    assert not list(tmp_path.iterdir()), 'Owned worker cache must be cleaned after reap'
    return records


def fault(records):
    faults = [item for item in records if item.get('fault')]
    assert len(faults) == 1 and faults[0]['count'] == 0, records
    assert not any('commands' in item for item in records), records
    return bytes.fromhex(faults[0]['error_hex']).decode()


def packet(*, version=1, status=0, sequence=1, memory=0, commands=(), error=b'', trailing=b''):
    body = struct.pack('<IIQII', version, status, sequence, memory, len(commands))
    for kind, player, value, text in commands:
        body += struct.pack('<IIiI', kind, player, value, len(text)) + text
    body += struct.pack('<I', len(error)) + error + trailing
    return struct.pack('<I', len(body)) + body


def test_persistent_original_custom_mode_survives_batch_time_limit(process_tools, tmp_path):
    started = time.monotonic()
    records = run(process_tools, tmp_path, (ROOT / 'examples/mods/first-to-three/scripts/main.lua').read_bytes(), 'match')
    replies = records[:-1]
    assert time.monotonic() - started > 2
    assert [r['sequence'] for r in replies] == [1, 2, 3, 4, 5]
    assert [r['commands'][0]['value'] for r in replies[1:4]] == [1, 2, 3]
    assert replies[3]['commands'][-1]['kind'] == 2
    assert records[-1]['busy'] is True


def test_many_async_ticks_keep_lua_state_and_deliver_stop_transaction(process_tools, tmp_path):
    records = run(process_tools, tmp_path, '''local n=0
return {api=1,on_tick=function() n=n+1 end,on_stop=function() game.message(tostring(n)) end}''', 'persistent')
    assert len(records) == 103
    assert bytes.fromhex(records[-2]['commands'][0]['text_hex']) == b'100'
    assert records[-2]['sequence'] == 102


def test_unconsumed_reply_applies_backpressure(process_tools, tmp_path):
    records = run(process_tools, tmp_path, 'return {api=1,on_start=function() game.message("one") end}', 'backpressure')
    assert len(records) == 2 and records[-1]['busy'] is True
    assert bytes.fromhex(records[0]['commands'][0]['text_hex']) == b'one'


def test_maximum_source_is_written_in_bounded_pipe_chunks(process_tools, tmp_path):
    tail = b']]return {api=1}'
    source = b'--[[' + b'x' * (1024 * 1024 - 4 - len(tail)) + tail
    records = run(process_tools, tmp_path, source, 'backpressure')
    assert records[0] == dict(sequence=1, commands=[])


@pytest.mark.parametrize('scenario', ['invalid-sequence', 'invalid-roster', 'invalid-position', 'invalid-phase'])
def test_host_lifecycle_roster_and_numeric_errors_fault_closed(process_tools, tmp_path, scenario):
    assert 'authoritative' in fault(run(process_tools, tmp_path, 'return {api=1}', scenario))


@pytest.mark.parametrize('source', [b'', b'\x1bLuaU', b'return {api=1}\0trailing', b'x' * (1024 * 1024 + 1)],
                         ids=['empty', 'binary', 'nul', 'oversized'])
def test_invalid_sources_never_start_a_worker(process_tools, tmp_path, source):
    assert run(process_tools, tmp_path, source) == [{'refused': True}]
    assert not list(tmp_path.iterdir())


@pytest.mark.parametrize('source', ['hang', 'load-hang', 'partial', 'diagnostic', 'crash'])
def test_native_faults_are_independently_stopped_without_blocking_host(process_tools, tmp_path, source):
    records = run(process_tools, tmp_path, source, hostile=True)
    fault(records)
    assert records[-1]['polls'] > 1


@pytest.mark.parametrize('source', ['idle-spin', 'idle-output'])
def test_unsolicited_idle_activity_faults_without_dispatch(process_tools, tmp_path, source):
    records = run(process_tools, tmp_path, source, 'idle', hostile=True)
    fault(records)


def test_close_returns_immediately_while_native_callback_is_hung(process_tools, tmp_path):
    records = run(process_tools, tmp_path, 'hang', 'close', hostile=True)
    assert records == [records[-1]] and records[-1]['stopped'] is True


def test_allowed_idle_cpu_does_not_accumulate_across_idle_windows(process_tools, tmp_path):
    records = run(process_tools, tmp_path, 'paced-idle', 'paced-idle', hostile=True)
    assert records[0] == dict(sequence=1, commands=[])


@pytest.mark.parametrize('wire', [
    packet(version=2), packet(status=3), packet(sequence=0), packet(sequence=2),
    packet(memory=16 * 1024 * 1024 + 1), packet(error=b'bad success'),
    packet(status=1), packet(status=1, error=b'\xff'), packet(status=1, error=b'bad\ntext'),
    packet(commands=[(1, 1, 1, b''), (99, 0, 0, b'')]),
    packet(commands=[(1, 3, 1, b'')]), packet(commands=[(1, 0, 1, b'')]),
    packet(commands=[(1, 1, 1000001, b'')]), packet(commands=[(1, 1, 1, b'x')]),
    packet(commands=[(2, 3, 0, b'')]), packet(commands=[(2, 0, 1, b'')]),
    packet(commands=[(2, 1, 0, b''), (1, 1, 3, b'')]),
    packet(commands=[(3, 1, 0, b'x')]), packet(commands=[(3, 0, 1, b'x')]),
    packet(commands=[(3, 0, 0, b'')]), packet(commands=[(3, 0, 0, b'\xc0\x80')]),
    packet(commands=[(3, 0, 0, b'\xed\xa0\x80')]), packet(commands=[(3, 0, 0, b'\xc2\x80')]),
    packet(commands=[(3, 0, 0, b'\xf4\x90\x80\x80')]),
    packet(commands=[(3, 0, 0, b'\x00')]), packet(commands=[(3, 0, 0, b'x' * 513)]),
    packet(commands=[(3, 0, 0, b'x')] * 65), packet(trailing=b'extra'),
    struct.pack('<I', 0xffffffff), struct.pack('<I', 27) + b'x' * 27,
], ids=lambda wire: wire.hex()[:32])
def test_hostile_worker_transactions_never_return_partial_commands(process_tools, tmp_path, wire):
    fault(run(process_tools, tmp_path, 'wire:' + wire.hex(), hostile=True))


@pytest.mark.parametrize('commands', [[(1, 1, -1000000, b'')], [(2, 0, 0, b'')],
                                     [(3, 0, 0, 'valid \U0001f680'.encode())],
                                     [(1, 2, 1000000, b''), (2, 2, 0, b''), (3, 0, 0, b'after end')]])
def test_valid_native_transactions_are_returned_intact(process_tools, tmp_path, commands):
    records = run(process_tools, tmp_path, 'wire:' + packet(commands=commands).hex(), 'backpressure', hostile=True)
    # Backpressure deliberately waits60ms; native fixture must remain idle.
    assert records[0]['commands'] == [dict(kind=k, player=p, value=v, text_hex=t.hex()) for k, p, v, t in commands]


def test_load_ack_cannot_contain_gameplay_commands(process_tools, tmp_path):
    wire = packet(sequence=0, commands=[(1, 1, 7, b'')])
    fault(run(process_tools, tmp_path, 'load-wire:' + wire.hex(), hostile=True))
