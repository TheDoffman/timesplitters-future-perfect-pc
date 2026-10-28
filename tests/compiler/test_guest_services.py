"""Project architectural storage and ABI diagnostic service contracts."""
import os
from pathlib import Path
import re
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

SOURCE=Path(__file__).parent

def provider_source(name):
    return ROOT/'src/runtime'/name

def compile_contract(folder,kind,*,enabled=True,mutation=None,no_tls=False):
    prefix=('#define RECOMP_ABI_CHECK 1\n' if enabled else '')
    if no_tls:prefix+='#define RECOMP_TLS\n'
    paths=[]
    names=['guest_state.c',f'guest_{kind}_runtime.c']
    if kind=='abi':names.append('guest_abi_diagnostics.c')
    for name in names:
        source=((ROOT/'src/runtime' if name in ('guest_state.c','guest_abi_diagnostics.c') else SOURCE)/name).read_text()
        if mutation and mutation[0]==name:
            _,before,after=mutation
            assert source.count(before)==1
            source=source.replace(before,after)
        path=folder/name;path.write_text(prefix+source);paths.append(path)
    if kind=='state':
        peer=folder/'peer.c'
        peer.write_text(prefix+'#define STATE_PEER\n'+(SOURCE/'guest_state_runtime.c').read_text())
        paths.append(peer)
    flags=[] if os.name=='nt' else ['-pthread']
    return _compile(folder,paths[0],extra=[*paths[1:],*flags])

def run(executable,*args):
    return subprocess.run([str(executable),*args],capture_output=True,text=True,
                          cwd=executable.parent,timeout=30)

def test_state_defaults_cross_unit_and_thread_isolation(tmp_path):
    result=run(compile_contract(tmp_path,'state'))
    assert result.returncode==0,result.stdout+result.stderr+str(result.returncode)

@pytest.mark.parametrize('before,after',[
    ('UINT32_C(0x1000)','UINT32_C(0x2000)'),
    ('UINT16_C(0x037f)','UINT16_C(0x077f)'),
    ('UINT16_C(0x4000)','UINT16_C(0)'),
])
def test_wrong_initial_state_is_rejected(tmp_path,before,after):
    exe=compile_contract(tmp_path,'state',mutation=('guest_state.c',before,after))
    assert run(exe).returncode==1

def test_shared_register_storage_is_rejected_after_compilation(tmp_path):
    exe=compile_contract(tmp_path,'state',no_tls=True)
    assert run(exe).returncode==7

def test_continuations_use_real_project_register_storage(tmp_path):
    source=(ROOT/'tests/compiler/nonlocal_jump_runtime.c').read_text()
    stub='RECOMP_TLS uint32_t g_ebx,g_edi,g_esi,g_esp,g_ebp,g_seh_ebp;'
    assert source.count(stub)==1
    harness=tmp_path/'continuations.c';harness.write_text(source.replace(stub,''))
    exe=_compile(tmp_path,harness,extra=[provider_source('guest_state.c'),
        ROOT/'src/runtime/guest_nonlocal_jump.c',*([] if os.name=='nt' else ['-pthread'])])
    modes=['basic','zero','high-bit','all-bits','rearm','nested','capacity','threads','missing']
    if os.name=='nt':modes.append('fibers')
    for mode in modes:
        result=run(exe,mode)
        assert result.returncode==0,result.stdout+result.stderr+f' {mode}={result.returncode}'

@pytest.fixture(scope='module')
def diagnostics(tmp_path_factory):
    return compile_contract(tmp_path_factory.mktemp('abi-service'),'abi')

def records(result):
    assert result.returncode==0,result.stdout+result.stderr+str(result.returncode)
    return [part for part in result.stderr.split('[ABI] ') if part]

def test_report_values_wrapping_and_no_guest_mutation(diagnostics):
    rows=records(run(diagnostics,'values'))
    assert len(rows)==5
    assert 'sub_00000050: ebx esi edi esp(epilogue never ran)' in rows[0]
    assert 'ebx 00000001->00000005 esi 00000002->00000006 edi 00000003->00000007' in rows[0]
    assert 'esp delta +0' in rows[0]
    assert 'sub_00000051: esp-too-high' in rows[1]
    assert 'esp +12, expected +8 (ret 4)' in rows[1]
    assert 'sub_00000052: esp-too-low' in rows[2]
    assert 'esp +0, expected +0 (ret 4294967292)' in rows[3]
    assert 'esp -2147483648, expected -2147483648 (ret 2147483644)' in rows[4]
    for row in rows:
        assert 'recent icall targets: 0000100D 0000100E 0000100F 00001000' in row

@pytest.mark.parametrize('mode',['limits','threads'])
def test_process_wide_deduplication_and_separate_limits(diagnostics,mode):
    rows=records(run(diagnostics,mode))
    assert len(rows)==96
    ordinary=[r for r in rows if 'expected' not in r]
    exact=[r for r in rows if 'expected' in r]
    assert len(ordinary)==32 and len(exact)==64
    for group,count in ((ordinary,32),(exact,64)):
        assert sorted(int(re.match(r'sub_([0-9A-F]+)',r)[1],16) for r in group)==list(range(count))

@pytest.mark.parametrize('enabled',[False,True])
def test_production_call_macros_reach_service_only_when_enabled(tmp_path,enabled):
    result=run(compile_contract(tmp_path,'abi',enabled=enabled),'macros')
    rows=records(result)
    assert len(rows)==(2 if enabled else 0)
    if enabled:
        assert rows[0].startswith('sub_00000030: ebx\n')
        assert rows[1].startswith('sub_00000040: ebx esp-too-high\n')

@pytest.mark.parametrize('before,after,mode',[
    ('exact ? 64u : 32u','exact ? 64u : 31u','limits'),
    ('if (set->addresses[i]==address) return 0;', 'if (set->addresses[i]==address) break;', 'limits'),
    ('cursor-age','cursor-age+1u','values'),
])
def test_compiled_diagnostic_mutations_fail_the_oracle(tmp_path,before,after,mode):
    exe=compile_contract(tmp_path,'abi',mutation=('guest_abi_diagnostics.c',before,after))
    with pytest.raises(AssertionError):
        if mode=='limits':test_process_wide_deduplication_and_separate_limits(exe,mode)
        else:test_report_values_wrapping_and_no_guest_mutation(exe)


def test_installed_kernel_uses_project_state_and_diagnostics():
    kernel=ROOT/'external/ts3FPrecomp/src/kernel'
    memory=(kernel/'xbox_memory_layout.c').read_text()
    assert 'void recomp_abi_violation_log(' not in memory
    assert not re.search(r'^RECOMP_TLS .*g_(?:eax|ebx|fs_base|xmm0|mm0|fp_stack|df)\b',memory,re.M)
    assert not re.search(r'^volatile uint(?:32|64)_t g_icall',memory,re.M)
    cmake=(kernel/'CMakeLists.txt').read_text()
    for name in ('guest_state.c','guest_abi_diagnostics.c'):
        assert '"${TSFP_SHARED_SOURCE}/runtime/'+name+'"' in cmake
    assert re.search(r'#define XBOX_TIB_MAIN\s+0x00001000',
                     (kernel/'xbox_memory_layout.h').read_text())
