import os
from pathlib import Path
import subprocess
import re
import pytest
from native_compiler import native_compiler

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize('fixture', ['text_read_scope', 'prompt_guest_scope'])
def test_text_read_scope(tmp_path, fixture):
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path / ('text-read-scope.exe' if os.name == 'nt' else 'text-read-scope')
    source = str(ROOT / 'tests' / (fixture+'.c'))
    args = ([compiler, '/nologo', '/std:c11', '/W4', '/I'+str(ROOT/'src'), source, '/Fe:'+str(exe)]
            if os.name == 'nt' else
            [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'src'), source, '-o', str(exe)])
    subprocess.run(args, cwd=tmp_path, env=env, check=True, timeout=60)
    subprocess.run([str(exe)], check=True, timeout=10)


@pytest.mark.parametrize('platform', ['TSFP_PC_CONTROLS', 'TSFP_NATIVE_PROMPTS'])
@pytest.mark.parametrize('break_scope', [False, True], ids=['fixed', 'detect-repeated-queries'])
@pytest.mark.parametrize('panel', [False, True], ids=['outline', 'panel'])
def test_outlined_label_scope(tmp_path, platform, break_scope, panel):
    manual = (ROOT/'external/ts3FPrecomp/titles/futureperfect/src/recomp_manual.c').read_text(encoding='utf-8')
    wrapper = re.search(r'void sub_00041E30\(void\)\n\{.*?\n\}', manual, re.S)
    assert wrapper, 'Apply the complete patch stack before testing'
    body = wrapper.group()
    if break_scope and not panel:
        body = body.replace('pc_prompts_layout_begin();', '').replace('pc_prompts_layout_end();', '')
    panel_body = ''
    if panel:
        for name in ('static void pc_panel_text_original', 'void sub_0012E3F0'):
            function = re.search(re.escape(name)+r'\(void\)\n\{.*?\n\}', manual, re.S)
            assert function, 'Apply the complete patch stack before testing'
            panel_body += function.group()+'\n'
        if break_scope:
            panel_body = panel_body.replace('pc_prompts_layout_begin();', '').replace('pc_prompts_layout_end();', '')
    source = tmp_path/'outline.c'
    defines = '#define '+platform+'\n'
    if panel:
        defines += '#define TEST_PANEL_SCOPE\n'
        if platform == 'TSFP_NATIVE_PROMPTS': defines += '#define TSFP_NATIVE_DISPLAY\n'
    source.write_text(defines+(ROOT/'tests/text_outline_scope.c').read_text().replace(
        '/* PRODUCTION_OUTLINE_WRAPPER */', body).replace('/* PRODUCTION_PANEL_WRAPPER */', panel_body), encoding='utf-8')
    compiler, env = native_compiler(tmp_path)
    exe = tmp_path/('outline.exe' if os.name=='nt' else 'outline')
    args = ([compiler, '/nologo', '/std:c11', '/W4', '/I'+str(ROOT/'src'), str(source), '/Fe:'+str(exe)]
            if os.name=='nt' else [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                                  '-I'+str(ROOT/'src'), str(source), '-o', str(exe)])
    subprocess.run(args, cwd=tmp_path, env=env, check=True, timeout=60)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
    if break_scope:
        assert result.returncode != 0 and 'queries==1' in result.stderr
        if panel: assert 'queries=5 measurements=3 draws=10' in result.stdout
    else:
        assert result.returncode == 0, result.stdout+result.stderr
