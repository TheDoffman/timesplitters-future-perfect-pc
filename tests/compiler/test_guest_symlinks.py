"""Original symbolic-link storage and selected guest adapter contracts."""
from pathlib import Path
import os, sys, subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile

@pytest.mark.parametrize('mutation',[None,
 ('p->name_bytes==n','p->name_bytes>=n'),
 ('memcmp(p+1,name,n)','memcmp(p+1,name,n-1)'),
 ('s->count>=s->limit','s->count>s->limit'),
 ('capacity<p->target_bytes','capacity+1<p->target_bytes'),
 ('memcpy((unsigned char *)(node+1)+n,target,bytes);','memcpy((unsigned char *)(node+1)+n,target,bytes-1);'),
 ('s->closed=1;p=s->head;','s->closed=0;p=s->head;'),
])
def test_original_copied_namespace(tmp_path,mutation):
    for name in ('guest_symlinks.c','guest_symlinks.h','symlink_contract.c'):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        p=tmp_path/'guest_symlinks.c';s=p.read_text(encoding='utf-8');a,b=mutation
        assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'symlink_contract.c',extra=[tmp_path/'guest_symlinks.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken namespace passed'
    else:assert run.returncode==0,run.stdout+run.stderr

@pytest.mark.parametrize('mutation',[None,
 ('n>max','(n>max && n>UINT16_MAX)'),
 ('address,8,0','address,7,0'),
 ('bridge_symlink_string(target,&b,&m)','bridge_symlink_string(target?name:0,&b,&m)'),
])
def test_original_guest_symbolic_link_adapter(tmp_path,mutation):
    for name in ('guest_symlinks.c','guest_symlinks.h','guest_kernel_symlinks.c','guest_kernel_symlinks.h','guest_kernel_symlinks.inc','kernel_symlink_contract.c'):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        p=tmp_path/'guest_kernel_symlinks.inc';s=p.read_text(encoding='utf-8');a,b=mutation
        assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'kernel_symlink_contract.c',extra=[tmp_path/'guest_symlinks.c',tmp_path/'guest_kernel_symlinks.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken guest link adapter passed'
    else:assert run.returncode==0,run.stdout+run.stderr
