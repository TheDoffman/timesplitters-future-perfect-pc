"""Execute the production matte filter; optionally lift the original US scenes."""
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'external/ts3FPrecomp'


def compile_and_run(tmp_path, source, fails=False):
    compiler, environment = native_compiler(tmp_path)
    fixture = tmp_path / 'letterbox.c'
    fixture.write_text(source, encoding='utf-8')
    exe = tmp_path / ('letterbox.exe' if os.name == 'nt' else 'letterbox')
    includes = RUNTIME / 'templates/runtime'
    if os.name == 'nt':
        command = [compiler, '/nologo', '/std:c11', '/O2', '/we4028', '/we4029', '/we4142',
                   '/I' + str(includes), '/I' + str(ROOT / 'src'), '/I' + str(ROOT / 'tests'),
                   '/I' + str(RUNTIME / 'src/d3d'), str(fixture), '/Fe:' + str(exe),
                   '/Fo:' + str(tmp_path / 'test.obj')]
    else:
        command = [compiler, '-std=gnu11', '-O2', '-I' + str(includes), '-I' + str(ROOT / 'src'), '-I' + str(ROOT / 'tests'),
                   '-I' + str(RUNTIME / 'src/d3d'), str(fixture), '-lm', '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=environment,
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    assert (result.returncode != 0) == fails, result.stdout + result.stderr
    assert ('FAIL:' in result.stderr) if fails else ('PASS:' in result.stdout)


def fixture_source():
    manual = (RUNTIME / 'titles/futureperfect/src/recomp_manual.c').read_text()
    wrapper = re.search(r'void sub_0001EFB0\(void\)\n\{.*?\n\}', manual, re.S)
    assert wrapper, 'Apply the complete runtime patch stack before testing'
    fixture = (ROOT / 'tests/cutscene_letterbox.c').read_text()
    camera = (ROOT / 'src/pc_display_menu.c').read_text()
    projection = re.search(r'void pc_display_perspective_projection\(.*?\n\}', camera, re.S)
    assert projection, 'Production perspective projection function missing'
    camera = camera[camera.index('static float cutscene_camera_zoom'):projection.end()]
    return fixture.replace('/* PRODUCTION_WRAPPER */', wrapper.group()).replace('/* PRODUCTION_CAMERA */', camera)


@pytest.mark.parametrize('mutation', ['none', 'bars', 'camera', 'menu-fov', 'air-lift-fov', 'movie-fit', 'movie-inset'])
def test_cutscene_rectangle_filter(tmp_path, mutation):
    source = fixture_source()
    if mutation == 'bars':
        source = source.replace('uint32_t frame = g_esp, caller = FP_MEM32(frame);',
                                'uint32_t frame = g_esp, caller = 0;')
    elif mutation == 'camera':
        source = source.replace('zoom=cutscene_camera_zoom();', 'zoom=1.f;')
    elif mutation == 'menu-fov':
        guard = '(int32_t)WORD_AT(0x0079094Cu)>100 ||'
        assert guard in source
        source = source.replace(guard, '')
    elif mutation == 'air-lift-fov':
        guard = ' &&\n        WORD_AT(actor+0x114)!=0x800'
        assert guard in source
        source = source.replace(guard, '')
    elif mutation == 'movie-fit':
        header = (RUNTIME / 'src/d3d/d3d8_movie.h').read_text(encoding='utf-8')
        signature = 'int fill_width, float *w, float *h)\n{'
        assert signature in header
        (tmp_path / 'd3d8_movie.h').write_text(
            header.replace(signature, signature + '\n    fill_width = 0;'), encoding='utf-8')
    elif mutation == 'movie-inset':
        header = (RUNTIME / 'src/d3d/d3d8_movie.h').read_text(encoding='utf-8')
        signature = 'float *x, float *y, float *w, float *h)\n{'
        assert signature in header
        (tmp_path / 'd3d8_movie.h').write_text(
            header.replace(signature, signature + '\n    left = right = 0;'), encoding='utf-8')
    compile_and_run(tmp_path, source, fails=mutation != 'none')


def test_original_us_cutscene_routines(tmp_path):
    xbe = ROOT / 'private/game/default.xbe'
    if not xbe.is_file():
        pytest.skip('Local supported US XBE required; never included in CI')
    image = xbe.read_bytes()
    assert hashlib.sha256(image).hexdigest() == '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc'
    sys.path.insert(0, str(RUNTIME))
    from tools.recomp import config
    from tools.recomp.translator import FunctionTranslator
    previous = dict(vars(config))
    try:
        config.configure_from_xbe(str(xbe))
        functions = {start: {'end': end, 'name': f'sub_{start:08X}'}
                     for start, end in ((0x6F150, 0x6F3B3), (0x1396B0, 0x139733),
                                       (0xD2CE0, 0xD2D16), (0x225C70, 0x225DEE))}
        functions[0x1EFB0] = {'name': 'sub_0001EFB0_gen', 'call_name': 'sub_0001EFB0'}
        routines = '\n'.join(FunctionTranslator(image, functions).translate_function(start, info)
                             for start, info in functions.items() if 'end' in info)
        assert 'sub_0001EFB0_gen' not in routines, 'Lifted callers must use the wrapper'
        addresses = sorted({int(a, 16) for a in re.findall(r'MEMF\(0x([0-9A-Fa-f]+)\)', routines)
                            if 0x470000 <= int(a, 16) < 0x490000})
        addresses = sorted(set(addresses) | set(range(0x50D408, 0x50D458, 4)))
        constants = '\n'.join(f'MEM32(0x{a:X}) = 0x{struct.unpack_from("<I", image, config.va_to_file_offset(a))[0]:08X};'
                              for a in addresses)
        source = '#define ORIGINAL_CUTSCENES\n' + fixture_source()
        source = source.replace('/* ORIGINAL_ROUTINES */', routines)
        source = source.replace('/* ORIGINAL_CONSTANTS */', constants)
        compile_and_run(tmp_path, source)
    finally:
        vars(config).update(previous)
