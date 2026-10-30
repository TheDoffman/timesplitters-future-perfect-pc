"""Completion target ownership, real disposal and compiled fault controls."""
from pathlib import Path
import os, sys, subprocess
import pytest
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0, str(HERE))
from test_translation_product import _compile

@pytest.mark.parametrize('mutation', [None,
    ('pin->dispatcher!=r->base || pin->kind!=r->kind', 'pin->dispatcher!=r->base'),
    ('result->user_iosb!=t->region[0].address', 'result->user_iosb==0'),
    ('if(host_overlap(iosb,event))goto failed;', 'if(host_overlap(iosb,event) && !iosb)goto failed;'),
    ('pin_span(t,0,8)', 'pin_span(t,0,7)'),
    ('t->lease[1].host:0', 't->lease[1].host:123'),
    ('tsfp_event_set(&t->event)>=0', 'tsfp_event_state(&t->event)>=0'),
    ('t->state=TSFP_IO_TARGET_QUARANTINED;leave(t);return 1;',
     't->state=TSFP_IO_TARGET_BOUND;leave(t);return 1;'),
    ('tsfp_guest_objects_retain(objects,t->region[i].key)', '(t->region[i].key!=0)'),
])
def test_owned_completion_targets(tmp_path, mutation):
    providers = ('guest_io_targets', 'guest_event', 'guest_objects', 'guest_io_complete',
                 'guest_io_owner', 'guest_io_request', 'guest_io_scope', 'guest_object_scope')
    for p in (ROOT/'src/runtime').glob('*.h'):
        (tmp_path/p.name).write_bytes(p.read_bytes())
    for name in [*(p+'.c' for p in providers), 'io_targets_contract.c']:
        p = HERE/name
        if not p.is_file():
            p = ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    p = tmp_path/'guest_objects.c'
    source = p.read_text(encoding='utf-8')
    assert source.count('int tsfp_guest_objects_release(') == 1
    p.write_bytes(source.replace('int tsfp_guest_objects_release(',
                                'int tsfp_guest_objects_release_actual(').encode('utf-8'))
    if mutation:
        old, new = mutation
        p = tmp_path/'guest_io_targets.c'
        source = p.read_text(encoding='utf-8')
        assert source.count(old) == 1
        p.write_bytes(source.replace(old, new).encode('utf-8'))
    flags = ['/W4'] if os.name == 'nt' else ['-Wall', '-Wextra']
    exe = _compile(tmp_path, tmp_path/'io_targets_contract.c',
                   extra=[*[tmp_path/(p+'.c') for p in providers], *flags])
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    if mutation:
        assert run.returncode != 0, 'Broken target ownership passed'
    else:
        assert run.returncode == 0, run.stdout+run.stderr
