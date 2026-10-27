"""Execute float-compare joins and the US pickup and sensitivity routines.

The XBE integration test requires a local supported dump; generated game code
stays in pytest's temporary directory and is never checked into the repository.
"""
import hashlib
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'external/ts3FPrecomp'
sys.path.insert(0, str(RUNTIME))
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator, _merge_flag_states
from tools.recomp.disasm import Operand


@pytest.fixture(autouse=True)
def restore_section_mapping():
    previous = dict(vars(config))
    yield
    vars(config).update(previous)


def translate(image):
    base = 0x10000
    config._install([config.Section('.text', base, len(image), 0, len(image), True)],
                    base, base, 'float-join-test')
    info = {'end': base + len(image), 'name': 'float_join'}
    return FunctionTranslator(image, {base: info}).translate_function(base, info)


@pytest.mark.parametrize('opcode', ['0f2f', '0f2e', '660f2f', '660f2e'])
@pytest.mark.parametrize('consumer', ['7605', '0f96c0', '0f46c7'])
def test_sse_comparison_snapshots_survive_different_operands(opcode, consumer):
    # test ecx,ecx; jz alternate; comi* xmm3,xmm0; jmp join;
    # alternate: comi* xmm0,xmm1; join: jbe / setbe / cmovbe.
    compare_a = bytes.fromhex(opcode + 'd8')
    compare_b = bytes.fromhex(opcode + 'c1')
    take = bytes.fromhex(consumer)
    image = (bytes.fromhex('85c974') + bytes([len(compare_a) + 2]) + compare_a +
             b'\xeb' + bytes([len(compare_b)]) + compare_b + take +
             bytes.fromhex('b801000000c3'))
    code = translate(image)
    assert '_flags /*' not in code
    assert '!(_fca > _fcb)' in code


def test_integer_and_float_states_do_not_merge():
    a = Operand(type='reg', reg='xmm0')
    b = Operand(type='reg', reg='xmm1')
    assert _merge_flag_states([('comiss', [a, b]), ('cmp', [a, b])]) is None
    assert _merge_flag_states([('comiss', [a, b]), None]) is None


def test_generator_change_invalidates_existing_game_c(tmp_path):
    spec = importlib.util.spec_from_file_location('generation', ROOT / 'tools/recomp-generation.py')
    generation = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generation)
    paths = ['scripts/recompile.py', 'templates/runtime/recomp_types.h',
             'titles/futureperfect/src/recomp_manual.c', 'tools/recomp/translator.py',
             'titles/futureperfect/src/recomp/gen/recomp_funcs.h']
    for name in paths:
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('original', encoding='utf-8')
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    assert generation.current(tmp_path)
    (tmp_path / 'tools/recomp/translator.py').write_text('fixed', encoding='utf-8')
    assert not generation.current(tmp_path)
    generation.record(tmp_path)
    assert generation.current(tmp_path)


def compile_and_run(tmp_path, source, expect_success=True):
    compiler, environment = native_compiler(tmp_path)
    path = tmp_path / 'float_join.c'
    path.write_text(source, encoding='utf-8')
    executable = tmp_path / ('float_join.exe' if os.name == 'nt' else 'float_join')
    includes = RUNTIME / 'templates/runtime'
    fault_adapter = ROOT / 'src/guest_integer_fault.c'
    if Path(compiler).stem.lower() == 'cl':
        command = [compiler, '/nologo', '/O2', '/I' + str(includes), str(path), str(fault_adapter),
                   '/Fe:' + str(executable), '/Fo:' + str(tmp_path) + os.sep]
    else:
        command = [compiler, '-std=c11', '-O2', '-I', str(includes), str(path), str(fault_adapter), '-lm', '-o', str(executable)]
    built = subprocess.run(command, cwd=tmp_path, env=environment, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
    assert (result.returncode == 0) == expect_success, result.stdout + result.stderr
    if expect_success:
        assert 'PASS:' in result.stdout
    else:
        assert 'FAIL: sensitivity' in result.stderr
    return result.stdout


def test_original_pickup_range_function(tmp_path):
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required for the pickup integration test')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    config.configure_from_xbe(str(xbe))
    info = {'end': 0xC184D, 'name': 'sub_000C15B0'}
    code = FunctionTranslator(image, {0xC15B0: info}).translate_function(0xC15B0, info)
    assert '_flags /* jbe:' not in code, 'Pickup radius branch became a constant'
    harness = (ROOT / 'tests/pickup_collision.c').read_text(encoding='utf-8')
    compile_and_run(tmp_path, harness.replace('/* GENERATED_PICKUP_ROUTINE */', code))


@pytest.mark.parametrize('legacy_join', [False, True], ids=['fixed', 'pre-fix-reproduction'])
def test_original_controller_sensitivity(tmp_path, monkeypatch, legacy_join):
    from tools.recomp import translator

    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Supported US XBE required for the sensitivity integration test')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    config.configure_from_xbe(str(xbe))
    if legacy_join:
        merge = translator._merge_flag_states

        def without_sse_join(states):
            # Reproduce the generator before recomp-float-joins.patch: distinct
            # float comparisons at the shared clamp lost their flag snapshot.
            if (states and all(s and s[0] in {'comiss', 'ucomiss', 'comisd', 'ucomisd'} for s in states)
                    and any(s != states[0] for s in states[1:])):
                return None
            return merge(states)

        monkeypatch.setattr(translator, '_merge_flag_states', without_sse_join)
    info = {'end': 0x2EACA7, 'name': 'sub_002EAB30'}
    code = FunctionTranslator(image, {0x2EAB30: info}).translate_function(0x2EAB30, info)
    constants = '\n'.join(
        f'MEM32(0x{address:X}) = 0x{struct.unpack_from("<I", image, config.va_to_file_offset(address))[0]:08X};'
        for address in (0x475C78, 0x475CE8, 0x47D674))
    harness = (ROOT / 'tests/controller_sensitivity.c').read_text(encoding='utf-8')
    source = harness.replace('/* GENERATED_SENSITIVITY_ROUTINE */', code)
    source = source.replace('/* ORIGINAL_SLIDER_CONSTANTS */', constants)
    compile_and_run(tmp_path, source, expect_success=not legacy_join)
