"""Original critical ownership, real OS concurrency, RTL ABI and status diagnostics."""
from pathlib import Path
import os,re,subprocess,sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile

def run_fixture(tmp_path,fixture,sources,mutation=None,arguments=(),env=None,expected=0):
    names=('guest_critical.h','host_critical.h','guest_virtual_memory.h','guest_allocation.h','guest_rtl_services.h','guest_kernel_rtl.h','guest_kernel_rtl.inc',fixture,*sources)
    for name in names:
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        name,a,b=mutation;p=tmp_path/name;s=p.read_text(encoding='utf-8');assert s.count(a)==1
        p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/fixture,extra=[*(tmp_path/n for n in sources),*flags])
    run=subprocess.run([str(exe),*arguments],capture_output=True,text=True,env=env,timeout=40)
    if mutation or expected:assert run.returncode!=0,'Broken or fatal-path critical service returned success'
    else:assert run.returncode==0,run.stdout+run.stderr
    return run

@pytest.mark.parametrize('mutation',[None,
    ('guest_critical.c','r->single && key?1:key','r->single && key?2+key:key'),
    ('guest_critical.c','p->key!=key','p->key==key'),
    ('guest_critical.c','p->owner!=thread || !p->depth','p->owner==thread || !p->depth'),
    ('guest_critical.c','p->owner=thread;++p->depth','p->owner=thread;p->depth+=2'),
    ('guest_critical.c','if(r->active || r->held)','if(r->held)'),
    ('guest_critical.c','r->ops.unlock(r->ops.context);acquired=','r->ops.lock(r->ops.context);acquired='),
    ('guest_critical.c','if(!r->ops.dispose(r->ops.context,p))','if(r->ops.dispose(r->ops.context,p))'),
])
def test_shared_critical_ownership_and_lifetime(tmp_path,mutation):
    run_fixture(tmp_path,'critical_contract.c',['guest_critical.c'],mutation)

@pytest.mark.parametrize('mode',['normal','single'])
def test_native_recursive_critical_concurrency(tmp_path,mode):
    run_fixture(tmp_path,'critical_host_contract.c',['guest_critical.c','host_critical.c'],arguments=[mode])

@pytest.mark.parametrize('mutation',[None,
    ('guest_kernel_rtl.inc','bridge_rtl_critical(TSFP_KERNEL_CRITICAL_TRY)','bridge_rtl_critical(TSFP_KERNEL_CRITICAL_ENTER)'),
    ('guest_kernel_rtl.inc','bridge_RtlEnterCriticalSectionAndRegion(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_ENTER)','bridge_RtlEnterCriticalSectionAndRegion(void){bridge_rtl_critical(TSFP_KERNEL_CRITICAL_LEAVE)'),
    ('guest_kernel_rtl.inc','result!=TSFP_CRITICAL_NOT_OWNER)','result!=TSFP_CRITICAL_NOT_OWNER && result!=TSFP_CRITICAL_NO_MEMORY)'),
    ('guest_kernel_rtl.inc','g_eax=action==TSFP_KERNEL_CRITICAL_TRY && result==TSFP_CRITICAL_OK?1:0','g_eax=action==TSFP_KERNEL_CRITICAL_TRY && result!=TSFP_CRITICAL_OK?1:0'),
    ('guest_kernel_rtl.inc','message=STACK_ARG(2)','message=STACK_ARG(1)'),
])
def test_original_rtl_imports(tmp_path,mutation):
    run_fixture(tmp_path,'kernel_rtl_contract.c',[],mutation)

@pytest.mark.parametrize('mutation',[None,
    ('guest_rtl_services.c','{0x103,997}','{0x103,317}'),
    ('guest_rtl_services.c','fallback(context,status)','fallback(context,(uint16_t)status)'),
    ('guest_rtl_services.c','address>UINT32_MAX-i','address>UINT32_MAX'),
    ('guest_rtl_services.c',"output[capacity-2]='~'","output[capacity-2]='x'"),
])
def test_original_status_and_diagnostic_text(tmp_path,mutation):
    run_fixture(tmp_path,'rtl_services_contract.c',['guest_rtl_services.c'],mutation)

@pytest.mark.parametrize('mode',['normal','single','trace','fault'])
def test_actual_kernel_rtl_host(tmp_path,mode):
    # Only logging/legacy header declarations are substituted. The original
    # providers, actual native recursive locks and kernel integration are linked.
    (tmp_path/'kernel.h').write_bytes(b'''#ifndef RTL_FIXTURE_KERNEL_H
#define RTL_FIXTURE_KERNEL_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#define XBOX_LOG_WARN 1
#define XBOX_LOG_ERROR 2
#define XBOX_LOG_RTL 3
void xbox_log(int,int,const char *,...);
#endif
''')
    env=os.environ.copy()
    for name in ('RECOMP_CS_MODE','RECOMP_CS_TRACE_CRT','RECOMP_CS_WATCH'):env.pop(name,None)
    if mode=='single':env['RECOMP_CS_MODE']='single'
    if mode=='trace':env.update(RECOMP_CS_TRACE_CRT='all',RECOMP_CS_WATCH='17')
    result=run_fixture(tmp_path,'kernel_rtl_host_contract.c',
        ['guest_critical.c','host_critical.c','guest_rtl_services.c','guest_kernel_rtl.c'],arguments=[mode],env=env,expected=mode=='fault')
    if mode=='trace':assert 'stack candidate 00008040' in result.stderr
    if mode=='fault':assert 'fatal host synchronization failure' in result.stderr

def test_whole_native_rtl_module_is_retired():
    rt=ROOT/'external/ts3FPrecomp';kernel=rt/'src/kernel'
    sources=re.search(r'set\(KERNEL_SOURCES(.*?)\)',(kernel/'CMakeLists.txt').read_text(encoding='utf-8'),re.S).group(1).split()
    assert 'kernel_rtl.c' not in sources and 'kernel_thunks.c' not in sources
    cmake=(kernel/'CMakeLists.txt').read_text(encoding='utf-8')
    for name in ('guest_critical','host_critical','guest_rtl_services','guest_kernel_rtl'):
        assert cmake.count('${TSFP_SHARED_SOURCE}/runtime/'+name+'.c')==1
    bridge=(kernel/'kernel_bridge.c').read_text(encoding='utf-8')
    assert bridge.count('#include "guest_kernel_rtl.inc"')==1
    assert 'bridge_try_cs_shadow' not in bridge and 'BRIDGE_TRY_CS_SLOTS' not in bridge
    imports=((277,4,'RtlEnterCriticalSection'),(278,4,'RtlEnterCriticalSectionAndRegion'),(291,4,'RtlInitializeCriticalSection'),
             (294,4,'RtlLeaveCriticalSection'),(295,4,'RtlLeaveCriticalSectionAndRegion'),(306,4,'RtlTryEnterCriticalSection'),
             (301,4,'RtlNtStatusToDosError'),(352,12,'RtlRip'))
    for ordinal,bytes_,name in imports:
        assert 'static void bridge_'+name+'(void)' not in bridge
        assert re.search(r'case\s+'+str(ordinal)+r':\s+return\s+'+str(bytes_)+r';\s*/\*\s*'+name+r'\b',bridge)
    retired=re.compile(r'\bxbox_(?:SetCrtLockTable|Rtl(?:EnterCriticalSection|LeaveCriticalSection|InitializeCriticalSection|NtStatusToDosError|Unwind|RaiseException|Rip|Snprintf|Sprintf|Vsnprintf|Vsprintf))\b')
    for base in (rt/'src',rt/'titles/futureperfect/src',ROOT/'src'):
        for p in base.rglob('*'):
            if p.suffix not in ('.c','.h','.cpp','.inc') or p.name in ('kernel_rtl.c','kernel_thunks.c'):continue
            assert not retired.search(p.read_text(encoding='utf-8',errors='replace')),p
