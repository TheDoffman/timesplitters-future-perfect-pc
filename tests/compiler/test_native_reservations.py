from pathlib import Path
import os,subprocess,sys
import pytest
HERE=Path(__file__).resolve().parent
ROOT=next(p for p in HERE.parents if (p/'tools/project_paths.py').exists())
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile

def build(tmp_path,mutation=None):
    for name in ('native_reservations.c','native_reservations.h','native_reservations_contracts.c','host_pages.h'):
        p=HERE/name
        if not p.exists():p=ROOT/'src/runtime'/name
        text=p.read_text(encoding='utf-8')
        if mutation and name=='native_reservations.c':
            old,new=mutation;assert text.count(old)==1;text=text.replace(old,new)
        (tmp_path/name).write_text(text,encoding='utf-8')
    return _compile(tmp_path,tmp_path/'native_reservations_contracts.c',extra=['/W4'] if os.name=='nt' else ['-Wall','-Wextra'])

def test_native_reservation_contracts(tmp_path):
    exe=build(tmp_path)
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert p.returncode==0,p.stdout+p.stderr
    assert 'page-state contracts passed' in p.stdout

@pytest.mark.parametrize('mutation',[
    ('protection,state==DISCARDED','protection,0'),
    ('protection,state==DISCARDED','protection,state==COMMITTED'),
    ('base==(uintptr_t)r->base && !bytes','base>=(uintptr_t)r->base && !bytes'),
    ('if(changed<0)r->quarantined=1;','if(changed<-1)r->quarantined=1;'),
    ('if(!s->backend.release(s->backend.context,r->base,r->bytes))return 0;',
     '(void)s->backend.release(s->backend.context,r->base,r->bytes);'),
    ('bytes=r->bytes;','bytes=s->page_size;'),
    ('if(r->pages[i]!=COMMITTED)goto done;','if(r->pages[i]>DISCARDED)goto done;'),
])
def test_reservation_negative_controls(tmp_path,mutation):
    exe=build(tmp_path,mutation)
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    assert p.returncode!=0,'mutated reservation owner passed'
