"""Original recorded-input lifecycle and timing; local supported XBE required."""
import hashlib
from pathlib import Path
import re
import struct

import pytest

ROOT = Path(__file__).resolve().parents[1]


class DemoTimingMismatch(AssertionError):
    pass


def test_shared_demo_interval_scope(tmp_path):
    from test_recomp_float_joins import compile_and_run
    source = (ROOT / "tests/native_demo_scope.c").read_text()
    source = source.replace("../src/pc_native_demo_guest.inc",
                            (ROOT / "src/pc_native_demo_guest.inc").as_posix())
    compile_and_run(tmp_path, source)


def test_shared_demo_playback_schedule(tmp_path):
    from test_recomp_float_joins import compile_and_run
    source = (ROOT / "tests/native_demo_schedule.c").read_text().replace(
        "../src/pc_native_demo_guest.inc", (ROOT / "src/pc_native_demo_guest.inc").as_posix())
    compile_and_run(tmp_path, source)


@pytest.mark.parametrize("mutation", ["phase", "raw-end", "zero-time", "exit-metadata"])
def test_shared_demo_playback_regressions(tmp_path, mutation):
    from test_recomp_float_joins import compile_and_run
    parent = (ROOT / "src/pc_native_demo_guest.inc").read_text()
    playback = (ROOT / "src/pc_native_demo_playback_guest.inc").read_text()
    if mutation == "phase":
        before, after = "plan.phase=pc_native_demo_playback.phase;", "plan.phase=0;"
        assert playback.count(before) == 1
        playback = playback.replace(before, after)
        message = "fabs(visual-expected(n,rate))<0.00001"
    elif mutation == "raw-end":
        before = ("ND_WORD(0x7497B0u)=(plan->packet.raw_time-plan->packet.raw+\n"
                  "        (uint32_t)(phase*plan->packet.raw/f)+ND_WORD(0x4E7940u))&0xffffu;")
        assert playback.count(before) == 1
        playback = playback.replace(before, "ND_WORD(0x7497B0u)=plan->packet.raw_time;")
        message = "WORD(0x7497b0)==(n+1)*60/rate"
    elif mutation == "zero-time":
        before = "if(value==1u && pc_native_demo_skip_playback)return 0u;"
        assert parent.count(before) == 1
        parent = parent.replace(before, "/* zero-time gate removed */")
        message = "WORD(0x6b94c0)==0x10000 && !FLOAT(0x4e794c)"
    else:
        before = "g_tsfp_native_step.game_units=0;"
        assert parent.count(before) == 1
        parent = parent.replace(before, "/* stale advancing game metadata */")
        message = "g_tsfp_native_step.game_delta==0"
    playback_path = tmp_path / "playback_mutation.inc"
    playback_path.write_text(playback.replace('"pc_native_timing.h"',
        '"'+(ROOT / "src/pc_native_timing.h").as_posix()+'"'))
    parent_path = tmp_path / "demo_mutation.inc"
    parent_path.write_text(parent.replace('"pc_native_demo_playback_guest.inc"',
        '"'+playback_path.as_posix()+'"'))
    source = (ROOT / "tests/native_demo_schedule.c").read_text().replace(
        "../src/pc_native_demo_guest.inc", parent_path.as_posix())
    with pytest.raises(AssertionError, match=r"Assertion failed:.*"+re.escape(message)):
        compile_and_run(tmp_path, source)


@pytest.fixture(scope="module")
def demo_source():
    from test_recomp_float_joins import config, FunctionTranslator

    xbe = ROOT / "private/game/default.xbe"
    if not xbe.is_file():
        pytest.skip("Supported US XBE required for original demo playback")
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == (
        "3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc")
    ranges = [(0x42510, 0x42587), (0x425a0, 0x425da), (0x431f0, 0x4321c),
              (0x43220, 0x4382d), (0x438b0, 0x43a83),
              (0x69e70, 0x69e7b), (0x69e80, 0x69e99), (0x69ea0, 0x69eae)]
    services = [0x389b0, 0x42640, 0x69620, 0x69e60, 0x69ed0,
                0x150230, 0x153b80, 0x18ff70, 0x190340, 0x190520,
                0x1cce70, 0x1ccf80, 0x1cdae0, 0x356800, 0x35e960]
    # A new wrapper must be wired into this fixture, not silently bypassed.
    manual = (ROOT / "external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c").read_text()
    assert "void sub_00043220(void) { pc_native_demo_guest(sub_00043220_gen); }" in manual
    for start, _ in ranges:
        if start == 0x43220:
            continue  # Production wrapper is compiled in the C harness below.
        assert not re.search(r"\bvoid\s+sub_%08X\s*\(" % start, manual), (
            "Integrate the new demo wrapper into this regression fixture")
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions = {start: dict(end=end, name=f"sub_{start:08X}")
                     for start, end in ranges}
        functions.update({address: dict(end=address+1, name=f"sub_{address:08X}")
                          for address in services})
        translator = FunctionTranslator(image, functions)
        generated = "\n".join(translator.translate_function(start, functions[start])
                              for start, _ in ranges)
        assert generated.count("void sub_00043220(void)") == 1
        generated = generated.replace("void sub_00043220(void)", "void sub_00043220_gen(void)")
        constants = "\n".join(
            f"MEM32(0x{address:x})=0x{struct.unpack_from('<I', image, config.va_to_file_offset(address))[0]:08x};"
            for address in [0x475c78, 0x475d18,
                            *range(0x42588, 0x4259c, 4), *range(0x43830, 0x43844, 4)])
    finally:
        vars(config).update(previous)
    harness = (ROOT / "tests/native_demo_playback.c").read_text()
    for marker, value in {
        "@NUMERIC@": (ROOT / "src/pc_native_numeric.h").as_posix(),
        "@VISUAL@": (ROOT / "src/pc_native_visual_time_guest.inc").as_posix(),
        "@DEMO@": (ROOT / "src/pc_native_demo_guest.inc").as_posix(),
        "@TIMING@": (ROOT / "src/pc_native_timing.h").as_posix(),
        "/* ORIGINAL_CODE */": generated,
        "/* ORIGINAL_CONSTANTS */": constants,
    }.items():
        assert harness.count(marker) == 1
        harness = harness.replace(marker, value)
    return harness


@pytest.fixture(scope="module")
def playback_output(demo_source, tmp_path_factory):
    from test_recomp_float_joins import compile_and_run
    return compile_and_run(tmp_path_factory.mktemp("demo-playback"), demo_source)


@pytest.fixture(scope="module")
def playback_results(playback_output):
    rows = {}
    for line in playback_output.splitlines():
        if line.startswith("RESULT "):
            rate, channels, raw, game, raw_time, game_time = map(float, line.split()[1:])
            rows[tuple(map(int, (rate, channels, raw, game)))] = (raw_time, game_time)
    assert len(rows) == 40, playback_output
    return rows


@pytest.fixture(scope="module")
def roundtrip_results(playback_output):
    rows = {}
    for line in playback_output.splitlines():
        if line.startswith("ROUNDTRIP "):
            rate, playback, channels, raw_time, game_time, finish = map(float, line.split()[1:])
            rows[int(rate), int(playback), int(channels)] = raw_time, game_time, finish
    assert len(rows) == 50, playback_output
    return rows


@pytest.mark.parametrize("recording,playback", [
    (recording,playback) for recording in (60,120,144,180,240) for playback in (60,120,144,180,240)])
@pytest.mark.parametrize("channels", [1,4])
def test_recorded_pause_roundtrip(roundtrip_results, recording, playback, channels):
    raw, game, finish = roundtrip_results[recording, playback, channels]
    if abs(raw-1.) > 1e-5 or abs(game-.5) > 1e-5 or abs(finish-1.) > 1.01/playback:
        raise DemoTimingMismatch(
            f"record {recording}, playback {playback}, controllers {channels}: "
            f"after 1 wall second stream raw/game={raw}/{game}s, last packet at {finish}s; "
            "expected 1.0/0.5s and completion near 1s")


@pytest.mark.parametrize("mutation", ["timer", "raw-clock"])
def test_demo_transition_regressions_are_detected(demo_source, tmp_path, mutation):
    from test_recomp_float_joins import compile_and_run
    path = ROOT / "src/pc_native_demo_guest.inc"
    source = path.read_text().replace('"pc_native_demo_playback_guest.inc"',
        '"'+(ROOT / "src/pc_native_demo_playback_guest.inc").as_posix()+'"')
    if mutation == "timer":
        before = "pc_native_demo_interval_active ? pc_native_demo_raw_interval : right"
        after = "right"
        message = r"Assertion failed:.*0x6b98b0"
    else:
        before, after = "if(active &&", "if(0 && active &&"
        message = r"Assertion failed:.*0x4e7940"
    assert source.count(before) == 1
    mutated = tmp_path / "demo_mutation.inc"
    mutated.write_text(source.replace(before, after))
    with pytest.raises(AssertionError, match=message):
        compile_and_run(tmp_path, demo_source.replace(path.as_posix(), mutated.as_posix()))


def test_demo_wait_and_exit_timing(playback_results):
    # The shared original-code executable asserts native wait/exit timer and
    # published raw-clock parity, pause, EOF and restoration outside the scope.
    assert len(playback_results) == 40


@pytest.mark.parametrize("mutation", ["record-gate", "record-carry", "record-reset"])
def test_demo_recording_regressions_are_detected(demo_source, tmp_path, mutation):
    from test_recomp_float_joins import compile_and_run
    path = ROOT / "src/pc_native_demo_guest.inc"
    source = path.read_text().replace('"pc_native_demo_playback_guest.inc"',
        '"'+(ROOT / "src/pc_native_demo_playback_guest.inc").as_posix()+'"')
    if mutation == "record-gate":
        before = "return value==3u && pc_native_demo_skip_record ? 0u : value;"
        after = "return value;"
        message = "groups==60"
    elif mutation == "record-carry":
        before, after = "pc_native_demo_record.game+=game;", "pc_native_demo_record.game=game;"
        message = "MEM8(packet+1)==game_total-recorded_game"
    else:
        before = "pc_native_demo_cursor_reset(uint32_t value)\n{\n    pc_native_demo_record.valid=0;"
        after = "pc_native_demo_cursor_reset(uint32_t value)\n{"
        message = "MEM8(0x10140+c*14+1)==0"
    assert source.count(before) == 1
    mutated = tmp_path / "demo_record_mutation.inc"
    mutated.write_text(source.replace(before, after))
    with pytest.raises(AssertionError, match=r"Assertion failed:.*"+re.escape(message)):
        compile_and_run(tmp_path, demo_source.replace(path.as_posix(), mutated.as_posix()))


@pytest.mark.parametrize("rate", [60,120,144,180,240])
@pytest.mark.parametrize("channels", [1, 4])
@pytest.mark.parametrize("raw,game", [(1, 1), (1, 0), (2, 1), (2, 0)])
def test_recorded_time_matches_original60(playback_results, rate, channels, raw, game):
    measured_raw, measured_game = playback_results[rate, channels, raw, game]
    # Compare identical records against the original 60-update path. With unit
    # records this also means one second of game time per second of wall time.
    # Multi-tick records test rate-independent parity with that original path;
    # they do not establish wall-time fidelity of variable-duration recordings.
    expected_raw = float(raw)
    expected_game = float(game)
    if abs(measured_raw-expected_raw) > 1e-5 or abs(measured_game-expected_game) > 1e-5:
        raise DemoTimingMismatch(
            f"{rate} updates/s, {channels} controllers, record {raw}/{game} ticks: "
            f"1 wall second consumed raw/game {measured_raw}/{measured_game}s; "
            f"expected {expected_raw}/{expected_game}s")


def test_recorded_time_under_variable_cadence(playback_output):
    rows = [line.split()[1:] for line in playback_output.splitlines()
            if line.startswith("VARIABLE_CADENCE ")]
    assert {(int(c), int(p)) for c, p, _, _ in rows} == {(1, 0), (1, 1), (4, 0), (4, 1)}
    for _, _, wall, game in rows:
        assert abs(float(wall)-float(game)) < 1e-5
