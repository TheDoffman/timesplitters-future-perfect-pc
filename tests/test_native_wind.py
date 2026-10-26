"""Shared procedural solver, exact clock admission and ownership behavior."""
import os
from pathlib import Path
import subprocess

from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


def test_wind_wrappers_are_generated_and_called_through_manual_entry():
    import importlib.util
    runtime = ROOT / 'external/ts3FPrecomp'
    spec = importlib.util.spec_from_file_location('wind_manual_scan', runtime / 'tools/recomp/manual_scan.py')
    scanner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(scanner)
    skip, wrap, referenced = scanner.scan(str(runtime / 'titles/futureperfect/src/recomp_manual.c'))
    assert {0xa5ce0, 0x1ddf10} <= wrap
    assert {0xa5ce0, 0x1ddf10} <= skip
    assert 0xab050 in referenced


def test_native_wind_solver_clock_and_ownership(tmp_path):
    compiler, environment = native_compiler(tmp_path)
    executable = tmp_path / ('wind.exe' if os.name == 'nt' else 'wind')
    source = ROOT / 'tests/native_wind.c'
    if Path(compiler).stem.lower() == 'cl':
        command = [compiler, '/nologo', '/O2', '/I' + str(ROOT / 'src'), str(source),
                   '/Fe:' + str(executable), '/Fo:' + str(tmp_path / 'wind.obj')]
    else:
        command = [compiler, '-std=c11', '-O2', '-I', str(ROOT / 'src'), str(source),
                   '-lm', '-o', str(executable)]
    built = subprocess.run(command, cwd=tmp_path, env=environment,
                           capture_output=True, text=True, timeout=60)
    assert built.returncode == 0, built.stdout + built.stderr
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=15)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'PASS owner isolation/reuse/removal' in result.stdout


def test_original_wind_registration_clock_and_two_owners(tmp_path):
    """Original routines are injected locally; no guest code/assets are tracked."""
    import hashlib
    import struct
    import re
    import pytest
    from test_recomp_float_joins import config, FunctionTranslator, compile_and_run
    image_path = ROOT / "private/game/default.xbe"
    if not image_path.is_file():
        pytest.skip("Supported US XBE required for original wind graph")
    image = image_path.read_bytes()
    assert hashlib.sha256(image).hexdigest() == "3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc"
    ranges = [
        (0x1ddf10, 0x1de015, "sub_001DDF10_gen"),
        (0xab050, 0xab373, "sub_000AB050"),
        (0x1de250, 0x1de4a8, "sub_001DE250"),
        (0xa5ce0, 0xa5e34, "sub_000A5CE0_actual"),
        (0x116510, 0x116673, "sub_00116510"),
        (0x116680, 0x1166ce, "sub_00116680"),
        (0x151650, 0x15178f, "sub_00151650_actual"),
        (0x151e80, 0x151f1c, "sub_00151E80"),
        (0xa7670, 0xa786a, "sub_000A7670"),
        (0xa0c50, 0xa0d24, "sub_000A0C50"),
        (0x36e430, 0x36e43b, "sub_0036E430"),
        (0x153240, 0x15326d, "sub_00153240"),
        (0xa0130, 0xa05bf, "sub_000A0130"),
        (0x9fba0, 0x9fbdf, "sub_0009FBA0"),
        (0xa00b0, 0xa012e, "sub_000A00B0"),
        (0x1ce2e0, 0x1ce404, "sub_001CE2E0"),
        (0x68830, 0x69587, "sub_00068830"),
    ]
    constants = (
        0x69588, 0x6958c, 0x69590, 0x69594, 0x69598, 0x6959c,
        0x695a0, 0x695a4, 0x695a8, 0x695ac, 0x695b0, 0x695b4,
        0x1de4a8, 0x1de4ac, 0x1de4b0, 0x1de4b4, 0x1de4b8, 0x475c78,
        0x475c84, 0x475c88, 0x475cc8, 0x475cd4, 0x475ce0, 0x475ce4,
        0x475d0c, 0x475d10, 0x475d18, 0x475d4c, 0x476040, 0x4760cc,
        0x4760d0, 0x478628, 0x47862c, 0x478634, 0x47890c, 0x47d1f4,
        0x47d324, 0x47d57c, 0x47d580, 0x47d754, 0x47d800, 0x47d838,
        0x47d83c,
    )
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(image_path))
        functions = {a: dict(name=name, call_name=f'sub_{a:08X}', end=end) for a, end, name in ranges}
        translator = FunctionTranslator(image, functions)
        code = '\n'.join('void ' + f['name'] + '(void);' for f in functions.values())
        code += '\n' + '\n'.join(translator.translate_function(a, f) for a, f in functions.items())
        values = '\n'.join(f'MEM32(0x{a:x})=0x{struct.unpack_from("<I", image, config.va_to_file_offset(a))[0]:08x};'
                           for a in constants)
        source = (ROOT / 'tests/native_wind_original.c').read_text(encoding='utf-8')
        source = source.replace('/* POLICY */', '#define RECOMP_NUMERIC_POLICY_HEADER "' + (ROOT / 'src/pc_native_numeric.h').as_posix() + '"')
        source = source.replace('/* TIMING_HEADER */', '#include "' + (ROOT / 'src/pc_native_timing.h').as_posix() + '"')
        source = source.replace('/* WIND_GUEST */', '#include "' + (ROOT / 'src/pc_native_wind_guest.inc').as_posix() + '"')
        declarations = '\n'.join('void ' + name + '(void);' for name in sorted(set(re.findall(r'void (sub_[0-9A-F]{8}(?:_actual|_gen)?)\(void\)', source + code))))
        source = source.replace('/* ORIGINAL_ROUTINES */', declarations + '\n' + code).replace('/* ORIGINAL_CONSTANTS */', values)
        output = compile_and_run(tmp_path, source)
        assert 'multiowner comparisons=9864 original_registration_reuse_removal=PASS' in output
        assert 'pause_frames=371 divergent_raw_game_phases=361' in output
        executable = tmp_path / ('float_join.exe' if os.name == 'nt' else 'float_join')
        for mode, expected in [('scopes', 'original_byte_parity=47'), ('whole', 'whole fallback360 owner frames byte equal')]:
            checked = subprocess.run([str(executable), mode], capture_output=True, text=True, timeout=15)
            assert checked.returncode == 0, checked.stdout + checked.stderr
            assert expected in checked.stdout
        frozen = subprocess.run([str(executable), 'freeze-matrix'], capture_output=True, text=True, timeout=15)
        assert frozen.returncode != 0 and 'expected[i]' in frozen.stderr
        control = tmp_path / 'missing-registration'
        control.mkdir()
        broken = source.replace('pc_native_wind_register_guest(sub_000A5CE0_actual);', 'sub_000A5CE0_actual();')
        assert broken != source
        with pytest.raises(AssertionError, match='wind_owners.count==2'):
            compile_and_run(control, broken)
    finally:
        vars(config).update(previous)
