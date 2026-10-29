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

@pytest.mark.parametrize('mutation',[None,
    ('f->closed || f->live>=f->limit','f->closed'),
    ('TSFP_NAME_IGNORE_ASCII_CASE,key);','TSFP_NAME_IGNORE_ASCII_CASE,0);'),
    ('put(target,body);','put(target,driver);'),
    ('(void)tsfp_guest_objects_dereference(f->objects,key);return 1;', '(void)key;return 1;'),
    ('put(target,body);(void)tsfp_guest_objects_release(&lease);', 'put(target,body);(void)lease;'),
    ('if(result==1) {drop_metadata(f,n);return 1;}', 'if(result>=0) {drop_metadata(f,n);return 1;}'),
])
def test_original_device_factory_lifetime_transactions(tmp_path,mutation,real=False,bridge=None,finalization=None,callback=None,dispatch=None,event=None,final_event=None):
    for name in ('guest_device_factory.c','guest_device_factory.h','guest_objects.c','guest_objects.h',
                 'guest_object_services.h','guest_device_contract.c','guest_device_body.c','guest_device_body.h','guest_kernel_devices.inc',
                 'guest_device_callback.c','guest_device_callback.h','guest_object_scope.c','guest_object_scope.h',
                 'guest_device_dispatch.c','guest_device_dispatch.h','guest_callback_call.c','guest_callback_call.h',
                 'guest_device_event.c','guest_device_event.h','guest_event.c','guest_event.h','guest_device_event_scope.c','guest_device_event_scope.h'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_object_names.c','guest_object_names.h'):
        (tmp_path/name).write_bytes(source_file(name).read_bytes())
    for name in ('guest_allocation.c','guest_allocation.h'):
        (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
    if mutation:
        old,new=mutation;p=tmp_path/'guest_device_factory.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    if final_event:
        name,old,new=final_event;p=tmp_path/name;s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    if event:
        name,old,new=event;p=tmp_path/name;s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    if dispatch and isinstance(dispatch,tuple):
        name,old,new=dispatch[:3];p=tmp_path/name;s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    if callback:
        name,old,new=callback;p=tmp_path/name;s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    if finalization:
        old,new=finalization;p=tmp_path/'guest_device_factory.c';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    if bridge:
        old,new=bridge;p=tmp_path/'guest_kernel_devices.inc';s=p.read_text()
        assert s.count(old)==1;p.write_text(s.replace(old,new))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    if callback is not None or dispatch is not None:
        for name in ('guest_lifetime.h','guest_nonlocal_jump.c','recomp_types.h'):
            (tmp_path/name).write_bytes((ROOT/'src/runtime'/name).read_bytes())
        flags+=[tmp_path/'guest_nonlocal_jump.c']
        if dispatch is None:flags+=['/DTSFP_DEVICE_CALLBACK_SCOPE'] if os.name=='nt' else ['-DTSFP_DEVICE_CALLBACK_SCOPE']
        else:
            mode=dispatch if isinstance(dispatch,int) else dispatch[3] if len(dispatch)==4 else 1
            flags+=[('/D' if os.name=='nt' else '-D')+f'TSFP_DEVICE_DISPATCH={mode}']
            (tmp_path/'guest_state.c').write_bytes((ROOT/'src/runtime/guest_state.c').read_bytes())
            flags+=[tmp_path/'guest_state.c',tmp_path/'guest_device_dispatch.c',tmp_path/'guest_callback_call.c',tmp_path/'guest_device_event.c',tmp_path/'guest_event.c',tmp_path/'guest_device_event_scope.c']
    if event is not None or final_event is not None:
        (tmp_path/'recomp_types.h').write_bytes((ROOT/'src/runtime/recomp_types.h').read_bytes())
        flags+=[tmp_path/'guest_device_event.c',tmp_path/'guest_event.c']
        define='TSFP_DEVICE_EVENT' if final_event is None else 'TSFP_DEVICE_FINAL_EVENT'
        flags+=[('/D' if os.name=='nt' else '-D')+define]
    if finalization is not None:flags+=['/DTSFP_DEVICE_FINALIZATION'] if os.name=='nt' else ['-DTSFP_DEVICE_FINALIZATION']
    if bridge is not None:flags+=['/DTSFP_DEVICE_BRIDGE'] if os.name=='nt' else ['-DTSFP_DEVICE_BRIDGE']
    if real:flags+=['/DTSFP_REAL_DEVICE_BODY'] if os.name=='nt' else ['-DTSFP_REAL_DEVICE_BODY']
    exe=_compile(tmp_path,tmp_path/'guest_device_contract.c',extra=[tmp_path/name for name in (
        'guest_device_factory.c','guest_device_body.c','guest_device_callback.c','guest_object_scope.c','guest_objects.c','guest_object_names.c','guest_allocation.c')]+flags)
    result=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation or bridge or finalization or callback or isinstance(dispatch,tuple) or event or final_event:assert result.returncode!=0,'Broken device factory passed'
    else:
        assert result.returncode==0,result.stdout+result.stderr
        assert 'PASS:' in result.stdout

def test_factory_with_explicit_device_layout(tmp_path):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True)

@pytest.mark.parametrize('bridge',[(),
    ('output=STACK_ARG(5)','output=STACK_ARG(4)'),
    ('exclusive=(uint8_t)STACK_ARG(4)','exclusive=STACK_ARG(4)'),
    ('case TSFP_DEVICE_COLLISION:return UINT32_C(0xc0000035);','case TSFP_DEVICE_COLLISION:return UINT32_C(0);'),
    ('(void)tsfp_device_factory_delete(&bridge_devices,body);','(void)body;'),
])
def test_device_bridge_with_real_factory(tmp_path,bridge):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True,bridge=bridge)

@pytest.mark.parametrize('finalization',[(),
    ('n->phase=FINAL_WAITING;unlock(f);return 0;','n->phase=FINAL_READY;unlock(f);return 0;'),
    ('n->phase==DELETING && n->exposed && f->ops.defer_finalization','n->phase==DELETING && f->ops.defer_finalization'),
    ('n->key!=ticket->key || n->driver!=ticket->driver || n->phase!=FINAL_ACTIVE)','n->key!=ticket->key || n->phase!=FINAL_ACTIVE)'),
    ('n->key!=ticket->key || n->driver!=ticket->driver || n->phase!=FINAL_ACTIVE)','n->driver!=ticket->driver || n->phase!=FINAL_ACTIVE)'),
    ('n->phase=completed?FINAL_READY:FINAL_UNCERTAIN;','n->phase=FINAL_READY;(void)completed;'),
])
def test_device_deferred_finalization(tmp_path,finalization):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True,finalization=finalization)

@pytest.mark.parametrize('callback',[(),
    ('guest_device_callback.c','&frame->ticket,!abandoned','&frame->ticket,abandoned>=0'),
    ('guest_object_scope.c','if(cleanup && !cleanup(context,abandoned))ok=0;','if(cleanup && abandoned<0 && !cleanup(context,abandoned))ok=0;'),
    ('guest_device_callback.c','frame->active=0;\n    return','frame->active=1;\n    return'),
    ('guest_object_scope.c','while(stack->current) {\n        stack->current->abandoned=1;','while(stack->current) {\n        stack->current->abandoned=0;'),
])
def test_device_callback_real_guest_longjmp(tmp_path,callback):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True,callback=callback)

@pytest.mark.parametrize('dispatch',[1,2,3,4,5,6,
    ('guest_device_dispatch.c','routine=word(driver+4);','routine=word(driver);'),
    ('guest_device_dispatch.c','ops,routine,frame.ticket.body','ops,routine,frame.ticket.driver'),
    ('guest_device_dispatch.c','tsfp_device_callback_abandon(scopes,&frame)','tsfp_device_callback_end(scopes,&frame)'),
    ('guest_device_event_scope.c','frame->active=0;return tsfp_device_event_release(&frame->event);','frame->active=0;return abandoned?1:tsfp_device_event_release(&frame->event);',5),
    ('guest_device_callback.c','frame->ticket.body==body','frame->ticket.body!=body',6),
])
def test_device_driver_dispatch(tmp_path,dispatch):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True,dispatch=dispatch)

@pytest.mark.parametrize('event',[(),
    ('guest_device_event.c','address-0x34','address-0x30'),
    ('guest_device_factory.c','if(lease.kind==TSFP_KIND_DEVICE)','if(lease.kind!=0)'),
    ('guest_event.c','tsfp_atomic_cas32(view->signal,1,0)','tsfp_atomic_cas32(view->signal,1,1)'),
    ('guest_device_event.c','if(event[0]!=1 || event[1] || event[2]!=4 || ((uintptr_t)(event+4)&3)) {\n        (void)tsfp_guest_objects_release(&lease);','if(event[0]!=1 || event[1] || event[2]!=16 || ((uintptr_t)(event+4)&3)) {\n        (void)tsfp_guest_objects_release(&lease);'),
    ('guest_device_event.c','out->lease=lease;','out->lease=lease;(void)tsfp_guest_objects_release(&out->lease);'),
])
def test_embedded_device_event_ownership(tmp_path,event):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True,event=event)

@pytest.mark.parametrize('final_event',[(),
    ('guest_device_factory.c','n->phase==FINAL_READY && n->final_pins','n->phase==FINAL_READY && n->final_pins==SIZE_MAX'),
    ('guest_device_factory.c','n->phase!=FINAL_ACTIVE || n->final_pins==SIZE_MAX','n->phase==DISPOSING || n->final_pins==SIZE_MAX'),
    ('guest_device_factory.c','++n->final_pins;value=n->mapping;','(void)n->final_pins;value=n->mapping;'),
])
def test_final_callback_event_pins(tmp_path,final_event):
    test_original_device_factory_lifetime_transactions(tmp_path,None,real=True,final_event=final_event)


def source_file(name):
    fixture=HERE/name
    return fixture if fixture.is_file() else ROOT/"src/runtime"/name
