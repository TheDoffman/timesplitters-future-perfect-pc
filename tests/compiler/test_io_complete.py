from pathlib import Path
import os,sys,subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(ROOT/'tests/compiler'))
from test_translation_product import _compile
WRITE='    if(value.user_iosb) {put((unsigned char *)guard.delivery.iosb,value.status);put((unsigned char *)guard.delivery.iosb+4,value.information);}\n'
@pytest.mark.parametrize('mutation',[None,'late_iosb',
    ('value.thread=word(p+0x50);','value.thread=word(p+0x4c);'),
    ('guard.delivery.iosb+4,value.information','guard.delivery.iosb+4,value.status'),
    ('if(!ok)frame.scope.abandoned=1;','if(!ok)frame.scope.abandoned=0;'),
    ('released=owner?tsfp_io_owner_release(owner):1;','released=owner?1:1;'),
    ('if(prepared)clean=guard->ops.release','if(prepared && !guard->delivery.cookie)clean=guard->ops.release'),
    ('guard.delivery.iosb_bytes<8','guard.delivery.iosb_bytes<7'),
    ('owner->phase==TSFP_IO_ACTIVE || owner->phase==TSFP_IO_PENDING','owner->phase!=TSFP_IO_DEAD || owner->phase==TSFP_IO_PENDING'),
    ('if(result==TSFP_IO_COMPLETE_DONE)guard.disposition=TSFP_IO_DELIVERY_DONE;','if(result==TSFP_IO_COMPLETE_DONE)guard.disposition=TSFP_IO_DELIVERY_PRIVATE;'),
])
def test_original_completion_publication_and_native_unwind(tmp_path,mutation):
    providers=('guest_io_complete','guest_io_scope','guest_io_owner','guest_io_request','guest_objects','guest_object_scope','guest_nonlocal_jump')
    names=[f'{p}.c' for p in providers]+[f'{p}.h' for p in providers if p!='guest_nonlocal_jump']+['guest_io_stack.h','recomp_types.h','guest_lifetime.h','io_completion_contract.c']
    for name in names:
        candidate=HERE/name;source=candidate if candidate.is_file() else ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        p=tmp_path/'guest_io_complete.c';s=p.read_text(encoding='utf-8')
        if mutation=='late_iosb':
            assert s.count(WRITE)==1;s=s.replace(WRITE,'');needle='    ok=ops->notify(ops->context,guard.delivery.cookie,&value);\n';assert s.count(needle)==1;s=s.replace(needle,needle+WRITE)
        else:
            old,new=mutation;assert s.count(old)==1;s=s.replace(old,new)
        p.write_bytes(s.encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_completion_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken completion protocol passed'
    else:assert run.returncode==0,run.stdout+run.stderr
