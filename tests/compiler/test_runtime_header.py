"""Own header/service contracts and generation without a toolkit header."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import pytest
from test_translation_product import _compile
from test_translation_cli import fixture
from native_compiler import native_compiler
ROOT=Path(__file__).resolve().parents[2]
HEADER=ROOT/'src/runtime/recomp_types.h'

def test_transitional_template_matches_canonical_header():
    assert HEADER.read_text() == (ROOT/'external/ts3FPrecomp/templates/runtime/recomp_types.h').read_text()


@pytest.mark.parametrize('admission', ['complete', 'macro-only'])
def test_inherited_guarded_call_fixture_with_native_compiler(tmp_path, admission):
    # Exercise the inherited oracle even when only MSVC is installed. Its own
    # compiler discovery otherwise skips this case on many Windows developers.
    compiler, environment = native_compiler(tmp_path)
    environment.update(TSFP_PROJECT_ROOT=str(ROOT), TSFP_TEST_CC=compiler)
    code = '''import os, subprocess, sys
from pathlib import Path
from tools.recomp import test_icall_guarded_runtime as test
test._cc = lambda: os.environ['TSFP_TEST_CC']
if sys.argv[1] == 'macro-only':
    test._admission = lambda: test._macro('RECOMP_ICALL_IS_CODE')
def run(directory, sources, extra=()):
    assert not extra
    exe = Path(directory) / 'guarded.exe'
    flags = (['/nologo', '/std:c11', '/O2', '/WX', *sources, '/Fe:' + str(exe)]
             if os.name == 'nt' else ['-std=c11', '-Werror', *sources, '-o', str(exe)])
    built = subprocess.run([test._cc(), *flags], cwd=directory, capture_output=True, text=True)
    assert built.returncode == 0, built.stdout + built.stderr
    return subprocess.run([str(exe)], cwd=directory, capture_output=True, text=True)
test._run = run
test.test_guarded_site_matches_generic_dispatch_for_every_target()
'''
    result = subprocess.run([sys.executable, '-c', code, admission],
                            cwd=ROOT/'external/ts3FPrecomp', env=environment,
                            capture_output=True, text=True, timeout=60)
    if admission == 'complete':
        assert result.returncode == 0, result.stdout + result.stderr
    else:
        assert result.returncode != 0
        assert 'tsfp_guest_target_is_code' in result.stdout + result.stderr

@pytest.mark.parametrize('variant',['production','feedback','inclusive_high','lost_kernel','duplicate_address','lost_counter','lost_trace'])
def test_services(tmp_path,variant):
    text=(HEADER).read_text()
    changes={
        'inclusive_high':('return address < g_xbox_code_hi;', 'return address <= g_xbox_code_hi;'),
        'lost_kernel':('if (address >= UINT32_C(0xfe000000)) return 1;', 'if (address >= UINT32_C(0xfe000000)) return 0;'),
        'duplicate_address':('tsfp_guest_target_is_code((uint32_t)(address))', '((void)(address), tsfp_guest_target_is_code((uint32_t)(address)))'),
        'lost_counter':('++g_icall_guard_hits;', '(void)g_icall_guard_hits;'),
        'lost_trace':('#define RECOMP_TRACE_EXIT recomp_trace_exit', '#define RECOMP_TRACE_EXIT(name, address) ((void)0)')}
    if variant in changes:
        old,new=changes[variant];assert old in text;text=text.replace(old,new)
    (tmp_path/'recomp_types.h').write_text(text)
    (tmp_path/'recomp_icall_feedback.h').write_text('#define RECOMP_ICALL_SEEN_UNRESOLVED 1\n#define RECOMP_ICALL_SEEN_RESOLVED 2\n#define RECOMP_ICALL_OBSERVE(a,b) ((void)0)\n#define RECOMP_ICALL_OBSERVE_SITE(a,b) ((void)0)\n')
    source=tmp_path/'services.c';source.write_bytes((ROOT/'tests/compiler/runtime_header.c').read_bytes())
    flags=[]
    if variant in ('feedback','lost_counter'):flags=['/DRECOMP_ICALL_FEEDBACK'] if os.name=='nt' else ['-DRECOMP_ICALL_FEEDBACK']
    exe=_compile(tmp_path,source,extra=flags)
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    expected={'inclusive_high':1,'lost_kernel':1,'duplicate_address':1,'lost_counter':3,'lost_trace':2}.get(variant,0)
    assert run.returncode==expected,run.stdout+run.stderr
    if not expected:assert '201733 address cases and service contracts passed' in run.stdout


def test_default_header_and_compiled_guest_without_toolkit(tmp_path):
    args,records=fixture(tmp_path)
    i=args.index('--types-header');del args[i:i+2]
    project=tmp_path/'own-project'
    compiler=project/'tools/compiler';compiler.mkdir(parents=True)
    for source in (ROOT/'tools/compiler').iterdir():
        if source.suffix in ('.py','.json','.txt','.def'):shutil.copy2(source,compiler/source.name)
    cli=compiler/'translation_cli.py'
    header=project/'src/runtime/recomp_types.h';header.parent.mkdir(parents=True)
    header.write_bytes((HEADER).read_bytes())
    run=subprocess.run([sys.executable,str(cli),*args,'--all','--split','2'],cwd=project,capture_output=True,text=True,timeout=30)
    assert run.returncode==0,run.stdout+run.stderr
    assert not (tmp_path/'no-toolkit').exists()
    gen=tmp_path/'gen'
    assert (gen/'recomp_types.h').read_text()==header.read_text()
    source=gen/'execute.c'
    paths=[*sorted(gen.glob('recomp_0*.c')),gen/'recomp_dispatch.c',gen/'recomp_hle.c']
    source.write_text('\n'.join('#include "'+p.name+'"' for p in paths)+'''
RECOMP_TLS uint32_t g_eax,g_esp;
void recomp_stub_missing(uint32_t address){}
int main(void){g_esp=100;if(recomp_get_count()!=6)return 1;
recomp_lookup(0x11000)();if(g_eax!=1||g_esp!=104)return 2;
recomp_lookup(0x11040)();if(g_eax!=5||g_esp!=108)return 3;
if(!recomp_dispatch_init())return 4;recomp_lookup(0x11050)();return g_eax!=6||g_esp!=112;}
''')
    executable=_compile(gen,source)
    assert subprocess.run([str(executable)],timeout=30).returncode==0
    before={p.name:p.read_bytes() for p in gen.iterdir() if p.name.startswith('recomp_')}
    header.unlink()
    failed=subprocess.run([sys.executable,str(cli),*args,'--all','--split','2'],cwd=project,capture_output=True,text=True,timeout=30)
    assert failed.returncode!=0
    assert before=={p.name:p.read_bytes() for p in gen.iterdir() if p.name.startswith('recomp_')}
