from pathlib import Path
import os
import sys
import subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
RUNTIME=ROOT/'external/ts3FPrecomp'

def build(tmp_path,mutation=None):
    sources={'kernel.h':HERE/'guest_kernel_boot_contract.h',
             'xbox_memory_layout.h':HERE/'guest_kernel_boot_memory.h',
             'guest_kernel_boot.c':ROOT/'src/runtime/guest_kernel_boot.c',
             'boot_contract.c':HERE/'guest_kernel_boot_contracts.c'}
    for name,source in sources.items():
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_kernel_boot.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    extra=[tmp_path/'guest_kernel_boot.c']
    if os.name=='nt':
        extra+=['/W4','/D_CRT_SECURE_NO_WARNINGS','/I'+str(RUNTIME/'src')]
    else:
        extra+=['-Wall','-Wextra','-I',str(RUNTIME/'src'),'-I',str(ROOT/'src/runtime'),
                RUNTIME/'src/platform/win32_compat.c',
                *[ROOT/'src/runtime'/name for name in ('shared_views.c','owned_arena.c','mmio_views.c','guest_arena_service.c')],
                ROOT/'src/native/shared_host.c','-lm','-lpthread']
    return _compile(tmp_path,tmp_path/'boot_contract.c',extra=extra)

def execute(tmp_path,exe,scenario,**settings):
    env=os.environ.copy();env.pop('XBOX_LOG_LEVEL',None);env.pop('TSFP_DEBUG_QUIET',None);env.update(settings)
    return subprocess.run([str(exe),scenario],cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)

@pytest.mark.parametrize('scenario',['threads','quiet','low','high','fallback'])
def test_original_boot_and_diagnostics(tmp_path,scenario):
    exe=build(tmp_path)
    settings={'quiet':{'TSFP_DEBUG_QUIET':'1'},'low':{'XBOX_LOG_LEVEL':'-50'},'high':{'XBOX_LOG_LEVEL':'999999999999999999999'}}.get(scenario,{})
    if scenario=='fallback':(tmp_path/'xbox_kernel.log').mkdir()
    r=execute(tmp_path,exe,scenario,**settings);assert r.returncode==0,r.stdout+r.stderr
    if scenario=='quiet':
        assert not (tmp_path/'xbox_kernel.log').exists() and not r.stderr
        return
    text=r.stderr if scenario=='fallback' else (tmp_path/'xbox_kernel.log').read_text()
    assert 'ERROR_MARK' in text
    assert ('DEBUG_MARK' in text)==(scenario=='high')
    assert ('TRACE_MARK' in text)==(scenario=='high')
    if scenario=='low':assert 'services ready' not in text
    if scenario=='threads':assert 'PASS:' in r.stdout

@pytest.mark.parametrize('mutation',[
    ('xbox_GuestLockInit();xbox_GuestLockEnter();','xbox_GuestLockEnter();xbox_GuestLockInit();'),
    ('xbox_hal_init_timers();','/* missing timer startup */'),
    ('if(diagnostic_ready) return;','if(diagnostic_ready==2) return;'),
    ('diagnostic_quiet || level>diagnostic_level','diagnostic_quiet || level<diagnostic_level'),
    ('if(diagnostic_ready) EnterCriticalSection(&diagnostic_lock);',
     'if(diagnostic_ready) (void)&diagnostic_lock;'),
    ('if(diagnostic_ready) LeaveCriticalSection(&diagnostic_lock);',
     'if(diagnostic_ready) (void)&diagnostic_lock;'),
])
def test_broken_boot_is_rejected(tmp_path,mutation):
    exe=build(tmp_path,mutation);r=execute(tmp_path,exe,'threads')
    if r.returncode==0:
        text=(tmp_path/'xbox_kernel.log').read_text()
        assert 'ERROR_MARK' not in text or 'DEBUG_MARK' in text
    else:assert r.returncode!=0


def test_candidate_compiles_with_real_runtime_headers(tmp_path):
    from native_compiler import native_compiler
    compiler,env=native_compiler(tmp_path)
    source=tmp_path/'real_boot.c';source.write_bytes((ROOT/'src/runtime/guest_kernel_boot.c').read_bytes())
    obj=tmp_path/('boot.obj' if os.name=='nt' else 'boot.o')
    if os.name=='nt':
        args=[compiler,'/nologo','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS','/c',str(source),
              '/I'+str(RUNTIME/'src'),'/I'+str(RUNTIME/'src/kernel'),'/Fo:'+str(obj)]
    else:
        args=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-c',str(source),
              '-I',str(RUNTIME/'src'),'-I',str(RUNTIME/'src/kernel'),'-o',str(obj)]
    run=subprocess.run(args,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=60)
    assert run.returncode==0,run.stdout+run.stderr
    assert obj.is_file()
