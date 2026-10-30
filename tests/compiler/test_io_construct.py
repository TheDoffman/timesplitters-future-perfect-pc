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

PREP='    if(prepare && !prepare(context,&n->owner)) {\n        tsfp_io_owner_release(&n->owner);return 0;\n    }\n'
@pytest.mark.parametrize('mutation',[None,'early_publish',
    (PREP,'    if(prepare && !prepare(context,&n->owner)) {\n        return 0;\n    }\n'),
    ('if(prepare && !prepare(context,&n->owner))','if(prepare && (!prepare(context,&n->owner) && 0))'),
])
def test_private_request_construction_before_registry_publication(tmp_path,mutation):
    providers=('guest_io_pool','guest_io_registry','guest_io_owner','guest_io_request','guest_io_build','guest_io_transfer','guest_allocation')
    for name in [f'{p}.{ext}' for p in providers for ext in ('c','h')]+['guest_io_stack.h','io_pool_contract.c','io_construct_contract.c']:
        candidate=HERE/name;source=candidate if candidate.is_file() else (ROOT/'tests/compiler'/name if name=='io_pool_contract.c' else ROOT/'src/runtime'/name)
        (tmp_path/name).write_bytes(source.read_bytes())
    if mutation:
        p=tmp_path/'guest_io_pool.c';s=p.read_text(encoding='utf-8')
        if mutation=='early_publish':
            assert s.count(PREP)==1;s=s.replace(PREP,'');anchor='    /* Retain the creator until registration completes.';assert s.count(anchor)==1;s=s.replace(anchor,PREP+anchor)
        else:
            old,new=mutation;assert s.count(old)==1;s=s.replace(old,new)
        p.write_bytes(s.encode())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'io_construct_contract.c',extra=[*[tmp_path/f'{p}.c' for p in providers],*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken request construction passed'
    else:assert run.returncode==0,run.stdout+run.stderr
