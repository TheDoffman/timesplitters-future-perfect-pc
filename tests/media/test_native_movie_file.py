"""Local, windowless real-file XMV checks; no personal saves or audio devices."""


import hashlib
import os
from pathlib import Path
import re
import subprocess

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]
RUNTIME = ROOT / 'external/ts3FPrecomp'
MOVIES = ROOT / 'private/game/xmv'


@pytest.mark.skipif(os.name != 'nt' or not MOVIES.exists(),
                    reason='Requires Windows host movie backend and local XMV data')
@pytest.mark.parametrize('live_clock', [False, True], ids=['controlled-clock', 'windows-timers'])
@pytest.mark.parametrize('movie_name', ['eag_e.xmv', 'frd.xmv'])
def test_real_movie_native_cadence(tmp_path, live_clock, movie_name):
    movie = MOVIES / movie_name
    if not movie.is_file():
        pytest.skip('Local XMV data required; never included in CI')
    xbe = ROOT / 'private/game/default.xbe'
    assert hashlib.sha256(xbe.read_bytes()).hexdigest() == (
        '3cfd001a84fc3e08175d6c4b2e42ebf49577a089b5d0bd87ac724f61a41816bc')
    ffmpeg = RUNTIME / 'third_party/ffmpeg'
    if not (ffmpeg / 'include/libavcodec/avcodec.h').exists():
        pytest.skip('Verified local FFmpeg dependency is unavailable')
    compiler, env = native_compiler(tmp_path)
    src = RUNTIME / 'src'
    includes = [src, src/'hle', src/'d3d', src/'video', src/'config',
                ROOT/'src', ffmpeg/'include']
    executable = tmp_path/'movie.exe'
    command = [compiler, '/nologo', '/O2', '/std:c11', '/DXMV_HAVE_FFMPEG']
    if live_clock:
        command.append('/DLIVE_HOST_CLOCK')
    command += ['/I'+str(p) for p in includes]
    command += [str(ROOT/'tests/media/native_movie_file.c'), str(src/'video/xmv_demux.c'),
                str(src/'video/xmv_decode.c'), str(src/'hle/xbox_adpcm.c'),
                '/Fe:'+str(executable), '/Fo:'+str(tmp_path)+'\\']
    compiled = subprocess.run(command, cwd=tmp_path, env=env, capture_output=True,
                              text=True, timeout=60)
    assert compiled.returncode == 0, compiled.stdout+compiled.stderr
    environment = {k:v for k,v in env.items() if not k.startswith(('TSFP_', 'RECOMP_'))}
    environment['RECOMP_FFMPEG_DIR'] = str(ffmpeg/'bin')
    results = []
    native_rates = ('60', '120', '144', '180', '240')
    for rate in (native_rates if live_clock else ('original', *native_rates)):
        result = subprocess.run([str(executable), str(movie), rate], cwd=tmp_path,
                                env=environment, capture_output=True, text=True, timeout=60)
        (tmp_path/f'movie-{rate}.log').write_text(result.stdout+result.stderr, encoding='utf-8')
        assert result.returncode == 0, result.stdout+result.stderr
        line = next(line for line in result.stdout.splitlines() if line.startswith('RESULT '))
        print(line)
        if live_clock:
            timing = next(line for line in result.stdout.splitlines() if line.startswith('TIMING '))
            print(timing)
        values = dict(re.findall(r'(\w+)=(\S+)', line))
        assert 'PASS:' in result.stdout
        results.append(values)
    reference = results[0]
    for result in results[1:]:
        for field in ('pictures', 'image_hash', 'timestamp_hash', 'audio_bytes', 'audio_hash'):
            assert result[field] == reference[field], (field, results)
        # Last-picture and subsequent EOF observation each fall on poll
        # boundaries (up to two original 60-Hz intervals). Decoded timestamps
        # and image/PCM bytes above must stay exact.
        assert abs(int(result['duration_ms'])-int(reference['duration_ms'])) <= 34
        assert int(result['polls']) <= int(result['rate'])*int(result['duration_ms'])/1000+3
