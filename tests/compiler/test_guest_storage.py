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


def test_storage_startup_fence_metadata(tmp_path):
    for name in ('guest_storage.c','guest_storage.h','guest_paths.h','storage_fence_contract.c'):
        source=HERE/name
        if not source.is_file():source=ROOT/'src/runtime'/name
        (tmp_path/name).write_bytes(source.read_bytes())
    flags=['/W4'] if os.name=='nt' else ['-Wall','-Wextra','-pthread']
    exe=_compile(tmp_path,tmp_path/'storage_fence_contract.c',extra=[tmp_path/'guest_storage.c',*flags])
    run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
    assert run.returncode==0,run.stdout+run.stderr


@pytest.fixture(scope='module')
def native_fence(tmp_path_factory):
    root=tmp_path_factory.mktemp('native-storage-fence')
    providers=('guest_storage','guest_paths','guest_symlinks','guest_kernel_symlinks','guest_kernel_storage')
    for name in [*(p+'.c' for p in providers),*(p+'.h' for p in providers if p!='guest_kernel_storage'),
                 'guest_kernel_paths.inc','storage_host_contract.c']:
        source=HERE/name
        if not source.is_file():source=ROOT/'src/runtime'/name
        (root/name).write_bytes(source.read_bytes())
    (root/'kernel.h').write_bytes((HERE/'storage_kernel_facade.h').read_bytes())
    (root/'xbox_watchpoint.h').write_text('void xbox_watch_note_path(const char *);\n')
    (root/'recomp_config.h').write_text('#include <stdint.h>\nuint32_t recomp_config_title_id(void);\n')
    # A broken fence must still NEVER write into the runner's personal defaults.
    # Redirect only the Windows default-folder collaborator to this owned fixture.
    if os.name=='nt':
        backend=root/'guest_kernel_storage.c';text=backend.read_text()
        assert text.count('SHGetFolderPathW(')==1
        text=text.replace('SHGetFolderPathW(','tsfp_test_default_folder(')
        text='#include <windows.h>\nHRESULT WINAPI tsfp_test_default_folder(HWND,int,HANDLE,DWORD,LPWSTR);\n'+text
        backend.write_text(text)
    flags=['/W4','/DTSFP_STORAGE_FENCE_TEST=1','shell32.lib'] if os.name=='nt' else ['-Wall','-Wextra','-pthread','-DTSFP_STORAGE_FENCE_TEST=1']
    return _compile(root,root/'storage_host_contract.c',extra=[*[root/(p+'.c') for p in providers],*flags])


@pytest.mark.parametrize('mode',['valid','null','empty','relative','oversize','missing','late'])
def test_native_storage_fence_blocks_original_and_lazy_default_writes(native_fence,tmp_path,mode):
    root=tmp_path/'isolated';root.mkdir()
    for folder in ('game','save','other','environment','profile'):(root/folder).mkdir()
    for folder in ('save','environment','profile'):(root/folder/'personal.sav').write_bytes(b'personal progress canary')
    try:
        run=subprocess.run([str(native_fence),str(root),mode],capture_output=True,text=True,timeout=40,cwd=root)
        assert run.returncode==0,run.stdout+run.stderr
        for folder in ('save','environment','profile'):
            assert (root/folder/'personal.sav').read_bytes()==b'personal progress canary'
            if folder!='save' or mode!='late':assert {p.name for p in (root/folder).iterdir()}=={'personal.sav'}
        assert not (root/'default').exists() and not (root/'SaveData').exists()
        if mode=='valid':assert all((root/'other'/f'Partition{i}.img').is_file() for i in range(6))
        else:assert not list((root/'other').iterdir())
    finally:
        for folder in ('save','other','environment','profile','default/xboxrecomp','SaveData'):
            for part in range(6):
                image=root/folder/f'Partition{part}.img'
                assert not image.is_symlink() and image.resolve().is_relative_to(root.resolve())
                image.unlink(missing_ok=True)

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


@pytest.mark.skipif(os.name != 'nt', reason='Windows sparse-filesystem adapter')
@pytest.mark.parametrize('error', [1, 50, 5, 112, 999, 998])
def test_windows_nonsparse_storage_and_failures(tmp_path, error):
    providers = ('guest_storage', 'guest_paths', 'guest_symlinks', 'guest_kernel_symlinks')
    for name in [*(p+'.c' for p in providers), *(p+'.h' for p in providers),
                 'guest_kernel_storage.c', 'guest_kernel_paths.inc', 'storage_fallback_windows.c']:
        source = HERE / name
        if not source.is_file(): source = ROOT / 'src/runtime' / name
        (tmp_path/name).write_bytes(source.read_bytes())
    (tmp_path/'kernel.h').write_bytes((HERE/'storage_kernel_facade.h').read_bytes())
    (tmp_path/'xbox_watchpoint.h').write_text('void xbox_watch_note_path(const char *);\n')
    (tmp_path/'recomp_config.h').write_text('#include <stdint.h>\nuint32_t recomp_config_title_id(void);\n')
    exe = _compile(tmp_path, tmp_path/'storage_fallback_windows.c',
                   extra=[*[tmp_path/(p+'.c') for p in providers], '/W4', 'shell32.lib'])
    root = tmp_path/'isolated'; root.mkdir(); (root/'game').mkdir(); (root/'save').mkdir()
    run = subprocess.run([str(exe), str(root), str(error)], capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stdout + run.stderr
    if error in (1, 50):
        assert 'Sparse files unavailable; creating regular storage image' in run.stderr
        assert 'Storage initialization failed' not in run.stderr
    else:
        assert 'Storage initialization failed' in run.stderr
        assert 'Windows error' in run.stderr
