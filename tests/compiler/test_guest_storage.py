"""Original storage state/format contracts and actual native exclusive files."""
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

@pytest.mark.parametrize('mutation',[None,
 ('UINT32_C(0x80000000)','UINT32_C(0x40000000)'),
 ('part==1?0x465400u','part==1?0xfa000u'),
 ('++report.existing;continue;','++report.created;continue;'),
 ('ops->context,token,ok','ops->context,token,1'),
 ('++report.uncertain;ok=0;','++report.uncertain;ok=1;'),
 ('s->paths.save,paths->save,b+1','s->paths.save,paths->game,b+1'),
 ('if(!save && environment && environment[0])','if(environment && environment[0])'),
 ('if(!s->ready)','if(s->ready==2)'),
])
def test_original_storage_policy(tmp_path,mutation):
    for name in ('guest_storage.c','guest_storage.h','guest_paths.h','storage_contract.c'):
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    if mutation:
        p=tmp_path/'guest_storage.c';s=p.read_text(encoding='utf-8');a,b=mutation
        assert s.count(a)==1;p.write_bytes(s.replace(a,b).encode('utf-8'))
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'storage_contract.c',extra=[tmp_path/'guest_storage.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
    if mutation:assert run.returncode!=0,'Broken storage policy passed'
    else:assert run.returncode==0,run.stdout+run.stderr

def test_actual_native_storage_initialization_preserves_existing_images(tmp_path):
    providers=('guest_storage','guest_paths','guest_symlinks','guest_kernel_symlinks','guest_kernel_storage')
    for name in [*(p+'.c' for p in providers),*(p+'.h' for p in providers if p!='guest_kernel_storage'),'guest_kernel_paths.inc','storage_host_contract.c']:
        p=HERE/name
        if not p.is_file():p=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(p.read_bytes())
    (tmp_path/'kernel.h').write_bytes((HERE/'storage_kernel_facade.h').read_bytes())
    (tmp_path/'xbox_watchpoint.h').write_text('void xbox_watch_note_path(const char *);\n',encoding='utf-8')
    (tmp_path/'recomp_config.h').write_text('#include <stdint.h>\nuint32_t recomp_config_title_id(void);\n',encoding='utf-8')
    flags=['/W4','shell32.lib'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'storage_host_contract.c',extra=[*[tmp_path/(p+'.c') for p in providers],*flags])
    root=tmp_path/'isolated';root.mkdir()
    for folder in ('game','save','other','environment','profile'):(root/folder).mkdir()
    try:
        run=subprocess.run([str(exe),str(root)],capture_output=True,text=True,timeout=40)
        assert run.returncode==0,run.stdout+run.stderr
    finally:
        # Child is terminal; recycle only this fixture's six named images.
        for folder in ('save','other','environment'):
            for part in range(6):
                p=root/folder/f'Partition{part}.img'
                assert not p.is_symlink() and p.resolve().is_relative_to(root.resolve())
                p.unlink(missing_ok=True)
