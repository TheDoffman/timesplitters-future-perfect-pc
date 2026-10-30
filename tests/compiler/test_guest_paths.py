"""Original shared path policy and host boundary contracts."""
from pathlib import Path
import os,sys,subprocess
import pytest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from project_paths import configure
configure()
sys.path.insert(0,str(HERE))
from test_translation_product import _compile

def sources(tmp_path,fixture):
    providers=('guest_paths','guest_symlinks','guest_kernel_symlinks')
    for name in [*(p+'.c' for p in providers),*(p+'.h' for p in providers),'guest_storage.h','guest_kernel_paths.inc',fixture]:
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    return [tmp_path/(p+'.c') for p in providers]

@pytest.mark.parametrize('mutation',[None,
 ('strlen(word)!=n','strlen(word)<n'),
 ('r.root=TSFP_PATH_PROFILE','r.root=TSFP_PATH_SAVE'),
 ('if(title) {prefix[a++]','if(!title) {prefix[a++]'),
 ('"0123456789ABCDEF"','"0123456789abcdef"'),
 ('if(total>=capacity)return 0;','if(total>capacity)return 0;'),
 ('candidate.root==TSFP_PATH_PASSTHROUGH','candidate.root>TSFP_PATH_PROFILE'),
 ('floor=3;','floor=2;'),
])
def test_original_shared_path_policy(tmp_path,mutation):
    extra=sources(tmp_path,'path_contract.c')
    if mutation:
        p=tmp_path/'guest_paths.c';s=p.read_text(encoding='utf-8');old,new=mutation
        assert old in s;p.write_bytes(s.replace(old,new).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    exe=_compile(tmp_path,tmp_path/'path_contract.c',extra=[*extra,*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    if mutation:assert run.returncode!=0,'Broken path policy passed'
    else:assert run.returncode==0,run.stdout+run.stderr

@pytest.mark.parametrize('posix',[True,False])
def test_original_host_path_adapter(tmp_path,posix):
    if os.name!='nt' and not posix:pytest.skip('Windows ACP/UTF16 API case')
    extra=sources(tmp_path,'kernel_path_contract.c')
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra']
    if posix:flags+=['/DPATH_POSIX_TEST=1'] if os.name=='nt' else ['-DPATH_POSIX_TEST=1']
    exe=_compile(tmp_path,tmp_path/'kernel_path_contract.c',extra=[*extra,*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
    assert run.returncode==0,run.stdout+run.stderr
