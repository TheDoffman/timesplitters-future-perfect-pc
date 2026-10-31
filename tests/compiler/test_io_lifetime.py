"""Execute project request ownership and callback contracts with fault controls."""
from pathlib import Path
import os
import subprocess
import sys
import pytest

HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p / 'tools/project_paths.py').is_file())
sys.path.insert(0, str(ROOT / 'tools'))
from project_paths import configure
configure()
from test_translation_product import _compile

CORE = ('guest_io_owner', 'guest_io_request')
OBJECTS = ('guest_objects', 'guest_object_scope')
DEVICES = CORE + OBJECTS + ('guest_object_names', 'guest_device_factory',
                          'guest_device_body', 'guest_device_callback', 'guest_allocation')
CASES = {
    'scope': (CORE + OBJECTS + ('guest_io_scope', 'guest_nonlocal_jump'),
              'io_scope_contract.c', [
        ('guest_io_owner.c', 'o->internal+=2;', 'o->internal+=1;'),
        ('guest_io_owner.c', 'o->phase=TSFP_IO_PENDING;o->dispatch_abandoned=1;',
         'o->phase=TSFP_IO_ACTIVE;o->dispatch_abandoned=1;'),
        ('guest_io_owner.c', 'o->phase=abandoned?TSFP_IO_UNCERTAIN:TSFP_IO_COMPLETE;',
         'o->phase=TSFP_IO_COMPLETE;(void)abandoned;'),
        ('guest_io_scope.c', '&frame->ticket,abandoned', '&frame->ticket,abandoned<0')]),
    'callback': (('guest_callback_call', 'guest_state', 'guest_nonlocal_jump'),
                 'stdcall_contract.c', [
        ('guest_callback_call.c', old, new) for old, new in [
            ('put(frame+4+4*i,words[i]);', 'put(frame+4+4*i,words[i]+1);'),
            ('value=g_eax;balanced=g_esp==saved.esp;', 'value=saved.eax;balanced=g_esp==saved.esp;'),
            ('if(balanced && result)*result=value;', 'if(result)*result=value;'),
            ('count>4', 'count>1'), ('bytes=4+4*count;', 'bytes=4+4*count+4;'),
            ('if(admit && !admit(context))return 0;', 'if(admit)(void)admit(context);'),
            ('prepared->entry=NULL;', '(void)entry;'), ('g_esp!=prepared->esp', 'g_esp==0'),
            ('candidate.frame=frame;', 'candidate.frame=frame;put(frame,0);')]]),
    'attachment': (DEVICES, 'io_device_attachment_contract.c', [
        ('guest_io_owner.c', 'if(release)release(attachment);', '(void)release;(void)attachment;'),
        ('guest_io_owner.c', 'o->phase==TSFP_IO_READY && !o->release_attachment', 'o->phase!=TSFP_IO_DEAD'),
        ('guest_io_owner.c', 'if(release)release(attachment);', 'if(release){release(attachment);release(attachment);}')]),
    'devices': (DEVICES + ('guest_io_devices',), 'io_devices_contract.c', [
        ('guest_io_devices.c', old, new) for old, new in [
            ('if(e->body==body)', 'if(e->body==0)'), ('b->count>=b->capacity', 'b->count>b->capacity'),
            ('i>0;--i', 'i>1;--i'),
            ('if(!tsfp_guest_objects_release(&e->lease))++b->uncertain;', '(void)tsfp_guest_objects_release(&e->lease);')]]),
    'rollback': (CORE + OBJECTS + ('guest_io_scope',), 'io_rollback_contract.c', [
        ('guest_io_owner.c', 'o->phase=TSFP_IO_READY;o->internal-=2;', 'o->phase=TSFP_IO_PENDING;o->internal-=2;'),
        ('guest_io_owner.c', 'o->phase=TSFP_IO_READY;o->internal-=2;', 'o->phase=TSFP_IO_READY;o->internal-=1;'),
        ('guest_io_owner.c', 'ticket->kind=forward?5:3;ok=1;', 'ticket->kind=forward?4:1;ok=1;'),
        ('guest_io_scope.c', 'if(ok)f->active=0;else f->scope.abandoned=1;', 'if(ok)f->active=0;'),
        ('guest_io_owner.c', '++o->internal;ticket->owner=o;ticket->kind=4;', 'o->internal+=2;ticket->owner=o;ticket->kind=4;'),
        ('guest_io_owner.c', '        --o->internal;\n    } else {', '        o->phase=TSFP_IO_READY;--o->internal;\n    } else {'),
        ('guest_io_owner.c', 'forward && o->phase==TSFP_IO_PENDING', 'forward && o->phase==TSFP_IO_READY'),
        ('guest_io_scope.c', 'kind==5', 'kind==6')]),
}
PARAMS = [(name, mutation) for name, (_, _, faults) in CASES.items() for mutation in [None, *faults]]

@pytest.mark.parametrize('name,mutation', PARAMS,
                         ids=[f'{name}-{i}' for i, (name, _) in enumerate(PARAMS)])
def test_request_callback_lifetime(tmp_path, name, mutation):
    providers, fixture, _ = CASES[name]
    headers = {f'{base}.h' for base in providers if base not in ('guest_state', 'guest_nonlocal_jump')}
    headers.update(('guest_io_stack.h', 'guest_object_services.h', 'recomp_types.h', 'guest_lifetime.h'))
    for filename in [*[f'{base}.c' for base in providers], *sorted(headers)]:
        (tmp_path / filename).write_bytes((ROOT / 'src/runtime' / filename).read_bytes())
    for filename in (fixture, 'guest_device_contract.c', 'guest_callback_call_contract.c'):
        (tmp_path / filename).write_bytes((HERE / filename).read_bytes())
    if mutation:
        filename, old, new = mutation
        path = tmp_path / filename
        source = path.read_text(encoding='utf-8')
        assert source.count(old) == (2 if old == '&frame->ticket,abandoned' else 1)
        path.write_bytes(source.replace(old, new).encode('utf-8'))
    flags = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra']
    executable = _compile(tmp_path, tmp_path / fixture,
                          extra=[*[tmp_path / f'{base}.c' for base in providers
                                   if base != 'guest_io_devices'], *flags])
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=45)
    if mutation:
        assert result.returncode != 0, f'Broken {name} passed'
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'PASS:' in result.stdout
