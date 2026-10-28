from pathlib import Path
import os
import subprocess
import sys
import pytest
import re

HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
sys.path.insert(0,str(ROOT/'tests/support'))
from test_translation_product import _compile

def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/'src/runtime'/name


def contract(tmp_path, mutation=None):
    for name in ('guest_objects.h','guest_objects.c','guest_objects_contracts.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        path=tmp_path/'guest_objects.c'
        text=path.read_text(encoding='utf-8')
        old,new=mutation
        assert text.count(old)==1
        path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'guest_objects_contracts.c',extra=[tmp_path/'guest_objects.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)

def test_ownership_and_actual_concurrent_close(tmp_path):
    result=contract(tmp_path)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS:' in result.stdout

@pytest.mark.parametrize('mutation',[
    ('h->used==1 && h->generation==((token>>14)&LAST_GENERATION)', 'h->used==1'),
    ('o->handles || o->refs || o->leases) { leave(s);return; }','o->handles || o->refs) { leave(s);return; }'),
    ('\n    ++o->refs;*key=object_key(s,o);','\n    *key=object_key(s,o);'),
    ('if(close_source) consume(s,h);','if(close_source) { (void)h; }'),
    ('if(h->generation==LAST_GENERATION) h->exhausted=1;','if(h->generation==LAST_GENERATION) h->generation=0;'),
    ('if(!o->dispatcher) o->dispatcher=candidate;','o->dispatcher=candidate;'),
    ('retry && o->state==TSFP_OBJECT_RETRY','retry && o->state!=TSFP_OBJECT_CLOSING'),
    ('enter(s);o->host=0;dispatcher=o->dispatcher;leave(s);',
     'enter(s);dispatcher=o->dispatcher;leave(s);'),
    ('*ticket=install(s,h,o);h->used=2;', '*ticket=install(s,h,o);h->used=1;'),
    ('other!=o && other->state && other->dispatcher==candidate',
     'other==o && other->state && other->dispatcher==candidate'),
    ('s->free_dispatcher(s->context,dispatcher,o->kind,o->flags)',
     's->free_dispatcher(s->context,dispatcher,0,0)'),
])
def test_broken_ownership_is_rejected(tmp_path,mutation):
    result=contract(tmp_path,mutation)
    assert result.returncode!=0,'Broken provider passed its lifetime contract'

@pytest.mark.skipif(os.name!='nt',reason='Real Windows event lifetime adapter')
def test_real_windows_wait_survives_guest_handle_close(tmp_path):
    for name in ('guest_objects.h','guest_objects.c','guest_objects_windows.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    exe=_compile(tmp_path,tmp_path/'guest_objects_windows.c',extra=[tmp_path/'guest_objects.c','/W4'])
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS:' in result.stdout

def run_scope(tmp_path,mutation=None):
    for name in ('guest_objects.h','guest_objects.c','guest_object_scope.h',
                 'guest_object_scope.c','guest_object_scope_contracts.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        path=tmp_path/'guest_object_scope.c';text=path.read_text(encoding='utf-8')
        old,new=mutation;assert text.count(old)==1
        path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_object_scope_contracts.c',
                 extra=[tmp_path/'guest_objects.c',tmp_path/'guest_object_scope.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)

def test_nested_operation_scopes_and_explicit_thread_exit(tmp_path):
    result=run_scope(tmp_path)
    assert result.returncode==0,result.stdout+result.stderr

@pytest.mark.parametrize('mutation',[
    ('stack->current=frame->previous;frame->previous=NULL;',
     'stack->current=NULL;frame->previous=NULL;'),
    ('while(frame->count) {','while(frame->count>1) {'),
    ('while(stack->current) if(!tsfp_guest_scope_leave(stack,stack->current)) ok=0;',
     'if(stack->current) if(!tsfp_guest_scope_leave(stack,stack->current)) ok=0;'),
    ('frame->count==TSFP_GUEST_SCOPE_HANDLES','frame->count==TSFP_GUEST_SCOPE_HANDLES-1'),
    ('frame->tokens[i]==alias && frame->leases[i].key==key','frame->tokens[i]==alias'),
])
def test_broken_scope_lifetimes_are_rejected(tmp_path,mutation):
    result=run_scope(tmp_path,mutation)
    assert result.returncode!=0,'Broken scope ownership passed'

def run_jump(tmp_path,mutation=None):
    for name in ('guest_objects.h','guest_objects.c','guest_object_scope.h','guest_object_scope.c',
                 'guest_lifetime.h','guest_nonlocal_jump.c','guest_object_jump_contracts.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    (tmp_path/'recomp_types.h').write_bytes((ROOT/'src/runtime/recomp_types.h').read_bytes())
    if mutation:
        name,old,new=mutation;path=tmp_path/name;text=path.read_text(encoding='utf-8')
        assert text.count(old)==1;path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_object_jump_contracts.c',extra=[
        tmp_path/'guest_objects.c',tmp_path/'guest_object_scope.c',tmp_path/'guest_nonlocal_jump.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)

def test_real_guest_longjmp_unwinds_only_abandoned_scopes(tmp_path):
    result=run_jump(tmp_path)
    assert result.returncode==0,result.stdout+result.stderr

@pytest.mark.parametrize('mutation',[
    ('guest_nonlocal_jump.c','if (restore_lifetime && !restore_lifetime(lifetime_marks[index]))',
     'if (restore_lifetime && lifetime_marks[index]==UINT64_MAX && !restore_lifetime(lifetime_marks[index]))'),
    ('guest_nonlocal_jump.c','lifetime_marks[index]=capture_lifetime?capture_lifetime():0;',
     'lifetime_marks[index]=capture_lifetime?0:0;'),
    ('guest_object_scope.c','if(!p) return 0;','if(!p) return 1;'),
])
def test_broken_continuation_lifetimes_are_rejected(tmp_path,mutation):
    result=run_jump(tmp_path,mutation)
    assert result.returncode!=0,'Broken continuation lifecycle passed'

@pytest.fixture(scope='module')
def existing_continuation_fixture(tmp_path_factory):
    folder=tmp_path_factory.mktemp('candidate-continuations')
    for name in ('guest_nonlocal_jump.c','guest_lifetime.h'):
        (folder/name).write_bytes(source_file(name).read_bytes())
    harness=folder/'contracts.c'
    harness.write_bytes((ROOT/'tests/compiler/nonlocal_jump_runtime.c').read_bytes())
    return _compile(folder,harness,extra=[folder/'guest_nonlocal_jump.c',*([] if os.name=='nt' else ['-pthread'])])

def run_services(tmp_path,mutation=None):
    for name in ('guest_objects.h','guest_objects.c','guest_object_services.h',
                 'guest_object_services.c','guest_object_services_contracts.c'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    if mutation:
        name,old,new=mutation;path=tmp_path/name;text=path.read_text(encoding='utf-8')
        assert text.count(old)==1;path.write_text(text.replace(old,new),encoding='utf-8')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'guest_object_services_contracts.c',extra=[
        tmp_path/'guest_objects.c',tmp_path/'guest_object_services.c',*flags])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)

def test_guest_object_abi_and_reference_publication(tmp_path):
    result=run_services(tmp_path)
    assert result.returncode==0,result.stdout+result.stderr

@pytest.mark.parametrize('mutation',[
    ('guest_object_services.c','s->resolve_output(s->context,address,4)',
     's->resolve_output(s->context,address,1)'),
    ('guest_object_services.c','if(expected_type && (lease.kind>=TSFP_KIND_COUNT ||',
     'if(expected_type==UINT32_MAX && (lease.kind>=TSFP_KIND_COUNT ||'),
    ('guest_object_services.c','word(target,body);status=TSFP_OBJECT_SUCCESS;',
     'word(target,body);tsfp_guest_objects_dereference(s->objects,lease.key);status=TSFP_OBJECT_SUCCESS;'),
    ('guest_object_services.c','if(candidate) s->discard_body(s->context,candidate);',
     'if(candidate) { (void)candidate; }'),
    ('guest_object_services.c','if(retained && status!=TSFP_OBJECT_SUCCESS)',
     'if(retained && status==TSFP_OBJECT_BAD_HANDLE)'),
    ('guest_object_services.c','lease.kind==TSFP_KIND_THREAD || lease.kind==TSFP_KIND_EVENT_MANUAL',
     'lease.kind==TSFP_KIND_THREAD || lease.kind==TSFP_KIND_EVENT_MANUAL || lease.kind==TSFP_KIND_EVENT_AUTO'),
    ('guest_object_services.c','uint64_t key=s->current_key?s->current_key(s->context,value):0;',
     'static uint64_t key; if(!key) key=s->current_key?s->current_key(s->context,value):0;'),
    ('guest_objects.c','*token=install(s,h,o);leave(s);return 1;\n}\nint tsfp_guest_objects_close',
     '*token=TOKEN_TAG;leave(s);return 1;\n}\nint tsfp_guest_objects_close'),
])
def test_broken_guest_object_service_is_rejected(tmp_path,mutation):
    result=run_services(tmp_path,mutation)
    assert result.returncode!=0,'Broken guest object ABI passed'

def run_binding(tmp_path,mutation=None,real_memory=False):
    names=('guest_objects.h','guest_objects.c','guest_object_scope.h','guest_object_scope.c',
           'guest_object_services.h','guest_object_services.c','guest_lifetime.h',
           'guest_kernel_objects.inc','guest_kernel_threads.inc')
    for name in names:
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    bridge=(ROOT/'external/ts3FPrecomp/src/kernel/kernel_bridge.c').read_text(encoding='utf-8')
    start=bridge.index('static void bridge_PsTerminateSystemThread(void)\n{')
    opening=bridge.index('{',start);depth=1;end=None
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]',bridge[opening+1:]):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:end=opening+1+token.end();break
    assert end
    source=(HERE/'guest_kernel_binding_contracts.c').read_text(encoding='utf-8')
    source=source.replace('/*PS_TERMINATE_BINDING*/',bridge[start:end])
    (tmp_path/'binding.c').write_text(source,encoding='utf-8')
    if mutation:
        name,old,new=mutation;path=tmp_path/name;text=path.read_text(encoding='utf-8')
        assert text.count(old)==1;path.write_text(text.replace(old,new),encoding='utf-8')
    memory_sources=([ROOT/'src/runtime'/name for name in
        ('guest_thread_memory.c','guest_allocation.c','guest_image.c')]+['/DTSFP_REAL_THREAD_MEMORY']) if real_memory else []
    exe=_compile(tmp_path,tmp_path/'binding.c',extra=[tmp_path/'guest_objects.c',
        tmp_path/'guest_object_scope.c',tmp_path/'guest_object_services.c',
        '/I'+str(ROOT/'src/runtime'),'/W4','/D_CRT_SECURE_NO_WARNINGS',*memory_sources])
    return subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)

@pytest.mark.skipif(os.name!='nt',reason='Real Windows thread publication adapter')
@pytest.mark.parametrize('real_memory',[False,True],ids=['bounded-memory','project-memory'])
def test_actual_kernel_thread_and_object_adapters(tmp_path,real_memory):
    result=run_binding(tmp_path,real_memory=real_memory)
    assert result.returncode==0,result.stdout+result.stderr
    assert 'PASS: real thread publication' in result.stdout

@pytest.mark.skipif(os.name!='nt',reason='Real Windows thread publication adapter')
@pytest.mark.parametrize('mutation',[
    ('guest_kernel_threads.inc','bridge_thread_key=launch->object;g_fs_base=launch->tib;',
     'bridge_thread_key=0;g_fs_base=launch->tib;'),
    ('guest_kernel_threads.inc','suspended?CREATE_SUSPENDED:0','suspended==2?CREATE_SUSPENDED:0'),
    ('guest_kernel_threads.inc','!bridge_object_publish_thread(launch->object,launch->tib)',
     '!launch->tib'),
    ('guest_kernel_threads.inc','xbox_HeapFree(launch->tib);xbox_FreeThreadStack(launch->stack);free(launch);',
     'xbox_FreeThreadStack(launch->stack);free(launch);'),
    ('binding.c','        bridge_object_thread_exit();\n        ExitThread(exit_status);',
     '        ExitThread(exit_status);'),
])
def test_broken_native_kernel_bindings_are_rejected(tmp_path,mutation):
    result=run_binding(tmp_path,mutation)
    assert result.returncode!=0,'Broken native runtime binding passed'

@pytest.mark.parametrize('mode',['basic','zero','high-bit','all-bits','rearm','nested','capacity','threads','missing',
                                'fibers','returning-adapter','missing-adapter'])
def test_existing_continuation_contract_is_preserved(existing_continuation_fixture,mode):
    if os.name!='nt' and mode in ('fibers','returning-adapter','missing-adapter'):
        pytest.skip('Real Windows fiber stack adapter')
    result=subprocess.run([str(existing_continuation_fixture),mode],capture_output=True,text=True,timeout=30)
    if mode in ('returning-adapter','missing-adapter'):
        assert result.returncode!=0 and 'adapter did not transfer control' in result.stderr
    else:
        assert result.returncode==0,result.stdout+result.stderr
