"""Run the actual shared framebuffer cache against a fake screen-copy device."""


import os
from pathlib import Path
import subprocess
import sys

import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[2]


@pytest.mark.parametrize('mutation',['fixed','consumed-arguments'])
def test_offscreen_copy_rectangles(tmp_path,mutation):
    """The production HLE route keeps native pause pixels and copy offsets."""
    source=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8.c').read_text()
    start=source.index('static int shadow_copy_texture_rects(')
    implementation=source[start:source.index('\n#endif',start)]
    record=(ROOT/'external/ts3FPrecomp/src/hle/hle_d3d8_record.c').read_text()
    start=record.index('typedef struct { IDirect3DTexture8 *dst, *source;')
    replay=record[start:record.index('HRESULT host_CopyBackBufferToTexture(',start)]
    start=record.index('HRESULT host_CopyTextureRectToTexture(')
    replay+=record[start:record.index('HRESULT host_CopyTextureRegionScaled(',start)]
    fixture=(ROOT/'tests/graphics/offscreen_copy.c').read_text()
    fixture=fixture.replace('/* RECORDING */',replay)
    start=source.index('HLE_EXPORT(D3DDevice_CopyRects)')
    wrapper=source[start:source.index('\n}\n',start)+3]
    if mutation=='consumed-arguments':
        assert 'rects_va,count,points_va))' in wrapper
        wrapper=wrapper.replace('rects_va,count,points_va))','HLE_ARG(1),HLE_ARG(2),HLE_ARG(4)))')
    fixture=fixture.replace('/* WRAPPER */',wrapper)
    path=tmp_path/'offscreen.c';path.write_text(fixture.replace('/* PRODUCTION */',implementation))
    compiler,environment=native_compiler(tmp_path)
    exe=tmp_path/('offscreen.exe' if os.name=='nt' else 'offscreen')
    if os.name=='nt':
        command=[compiler,'/nologo','/std:c11','/O2',str(path),'/Fe:'+str(exe)]
    else:
        command=[compiler,'-std=c11','-O2',str(path),'-o',str(exe)]
    result=subprocess.run(command,cwd=tmp_path,env=environment,capture_output=True,text=True,timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    if mutation=='consumed-arguments':
        assert result.returncode!=0,'Fixture did not detect native argument consumption'
        assert 'last_rect.left==320' in result.stderr
    else:
        assert result.returncode==0,result.stdout+result.stderr


@pytest.mark.skipif(sys.platform not in ('win32', 'darwin'),
                    reason='HLE renderer is compiled on Windows and macOS')
@pytest.mark.parametrize('mutation', ['none', 'stale-copy', 'first-hit', 'last-hit'])
@pytest.mark.parametrize('common_renderer', [False, True] if sys.platform == 'darwin' else [True],
                         ids=lambda common: 'rhi' if common else 'legacy-gl')
def test_framebuffer_sampling_passes(tmp_path, mutation, common_renderer):
    compiler, environment = native_compiler(tmp_path)
    source = ROOT / 'external/ts3FPrecomp/src'
    includes = [source, ROOT / 'src', *(source / name for name in ('d3d', 'hle', 'kernel', 'config', 'platform'))]
    if mutation != 'none':
        # Prove the fixture detects stale copies and incorrect alias selection.
        original = (source / 'hle/hle_d3d8_texture.c').read_text()
        replacements = {
            'stale-copy': ('        framebuffer_binding_changed(stage, texture);',
                           '        /* mutation: stale copy */'),
            'first-hit': ('        if (!framebuffer) break;',
                          '        if (!framebuffer) break; *framebuffer = entry->framebuffer; break;'),
            'last-hit': ('        if (!first) first = entry;', '        first = entry;'),
        }
        old, new = replacements[mutation]
        assert original.count(old) == 1
        mutant = tmp_path / 'mutant/hle/hle_d3d8_texture.c'
        mutant.parent.mkdir(parents=True)
        mutant.write_text(original.replace(old, new))
        includes.insert(0, mutant.parent.parent)
    exe = tmp_path / ('framebuffer.exe' if os.name == 'nt' else 'framebuffer')
    fixture = ROOT / 'tests/graphics/framebuffer_sampling.c'
    if os.name == 'nt':
        # MSVC otherwise accepts mock/declaration mismatches as warnings,
        # while Clang rejects them. Enforce the same contract on Windows.
        command = [compiler, '/nologo', '/std:c11', '/O2', '/Gy', '/Gw',
                   '/we4028', '/we4029']
        command += ['/I' + str(path) for path in includes]
        command += [str(fixture), '/Fe:' + str(exe), '/Fo:' + str(tmp_path / 'test.obj'),
                    '/link', '/OPT:REF']
    else:
        command = [compiler, '-std=gnu11', '-O2', '-Wl,-dead_strip']
        if common_renderer:
            command += ['-DTSFP_RHI_RENDERER=1']
        command += ['-I' + str(path) for path in includes]
        command += [str(fixture), '-o', str(exe)]
    result = subprocess.run(command, cwd=tmp_path, env=environment,
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if mutation != 'none':
        assert result.returncode != 0, f'Regression did not detect {mutation}'
        expected = {'stale-copy': 'copies == 2 && sampled == 20',
                    'first-hit': 'actual_fb == expected_fb',
                    'last-hit': 'bound_host_entry(host, &actual_fb) == expected'}
        assert expected[mutation] in result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS: framebuffer sampling passes' in result.stdout
