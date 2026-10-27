"""SDK pin, integrity and isolated probe environment; no download/install."""


from pathlib import Path
import io
import sys
import zipfile
import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import vulkan_sdk as sdk


def root_fixture(tmp_path):
    (tmp_path/'config/build').mkdir(parents=True)
    (tmp_path/'config/build/vulkan-sdk.json').write_bytes((ROOT/'config/build/vulkan-sdk.json').read_bytes())
    return tmp_path


def test_sdk_receipt_detects_changed_installed_file(tmp_path):
    root=root_fixture(tmp_path)
    installed=sdk.install_path(root)
    for name in sdk.CRITICAL:
        path=installed/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(name.encode())
    (installed/'Licenses').mkdir()
    sdk.record_install(root)
    assert sdk.verify_install(root)==installed/'macOS'
    (installed/'macOS/lib/libdxcompiler.dylib').write_bytes(b'changed')
    with pytest.raises(RuntimeError,match='installed file changed'):
        sdk.verify_install(root)


def test_unknown_existing_install_is_preserved(tmp_path):
    root=root_fixture(tmp_path);installed=sdk.install_path(root);installed.mkdir(parents=True)
    keep=installed/'keep';keep.write_bytes(b'preserve')
    with pytest.raises(RuntimeError,match='missing/unverified'):
        sdk.setup(root)
    assert keep.read_bytes()==b'preserve'


def test_download_identifies_client_but_still_rejects_unpinned_bytes(tmp_path,monkeypatch):
    root=root_fixture(tmp_path)
    spec=sdk.pin(root)
    requests=[]
    def open_download(request,timeout):
        requests.append(request)
        assert request.full_url==spec['url']
        assert request.get_header('User-agent')=='TSFP-SDK-Setup/1.0'
        assert timeout==60
        return io.BytesIO(b'upstream error page instead of the pinned archive')
    monkeypatch.setattr(sdk.urllib.request,'urlopen',open_download)
    monkeypatch.setattr(sdk.shutil,'disk_usage',lambda _:type('Usage',(),{'free':6*1024**3})())
    def no_installer(*args,**kwargs):
        pytest.fail('Unverified downloads must never be extracted or executed')
    monkeypatch.setattr(sdk.subprocess,'run',no_installer)
    with pytest.raises(RuntimeError,match='checksum pin'):
        sdk.setup(root)
    assert len(requests)==1
    assert not sdk.install_path(root).exists()


@pytest.mark.parametrize('member',['safe/data','../escape','/absolute'])
def test_archive_pin_and_paths(tmp_path,member):
    archive=tmp_path/'sdk.zip'
    with zipfile.ZipFile(archive,'w') as z:z.writestr(member,b'data')
    spec=dict(bytes=archive.stat().st_size,sha256=sdk.sha256(archive))
    if member=='safe/data':sdk.verify_archive(archive,spec)
    else:
        with pytest.raises(RuntimeError,match='Unsafe path'):sdk.verify_archive(archive,spec)
    spec['sha256']='0'*64
    with pytest.raises(RuntimeError,match='checksum pin'):sdk.verify_archive(archive,spec)


def test_probe_environment_is_local_and_pinned(tmp_path,monkeypatch):
    for key in ('VK_DRIVER_FILES','VK_LAYER_PATH','RECOMP_SAVE_DIR','TSFP_DISPLAY_CONFIG',
                'DYLD_INSERT_LIBRARIES','SDL_VULKAN_LIBRARY'):
        monkeypatch.setenv(key,'/personal/override')
    env=sdk.probe_environment(tmp_path/'sdk','moltenvk',tmp_path)
    assert env['VK_DRIVER_FILES']==str(tmp_path/'sdk/share/vulkan/icd.d/MoltenVK_icd.json')
    assert env['SDL_VULKAN_LIBRARY']==str(tmp_path/'sdk/lib/libvulkan.1.dylib')
    assert 'RECOMP_SAVE_DIR' not in env and 'TSFP_DISPLAY_CONFIG' not in env
    assert 'DYLD_INSERT_LIBRARIES' not in env
    assert env['RECOMP_SHADER_CACHE']=='0'
    assert Path(env['XDG_CONFIG_HOME']).is_relative_to(tmp_path/'build/macos/rhi-tests')
    with pytest.raises(ValueError):sdk.probe_environment(tmp_path/'sdk','unknown',tmp_path)


def test_gpu_overlay_preserves_runner_isolation(tmp_path,monkeypatch):
    monkeypatch.setenv('RECOMP_SAVE_DIR','/personal/saves')
    existing=dict(RECOMP_SAVE_DIR='/isolated/saves',RECOMP_INPUT_CONFIG='/isolated/input',
                  TSFP_DISPLAY_CONFIG='/isolated/pc.ini',RECOMP_DISPLAY_CONFIG='/isolated/runtime.ini',
                  XDG_CONFIG_HOME='/isolated/xdg',XDG_CACHE_HOME='/isolated/cache',VK_DRIVER_FILES='/wrong/driver',
                  DYLD_INSERT_LIBRARIES='/wrong/library',RECOMP_SHADER_CACHE='1')
    env=sdk.with_probe_gpu_environment(existing,tmp_path/'sdk','kosmickrisp',tmp_path)
    for key in ('RECOMP_SAVE_DIR','RECOMP_INPUT_CONFIG','TSFP_DISPLAY_CONFIG','RECOMP_DISPLAY_CONFIG','XDG_CONFIG_HOME','XDG_CACHE_HOME'):
        assert env[key]==existing[key]
    assert Path(env['VK_DRIVER_FILES'])==tmp_path/'sdk/share/vulkan/icd.d/libkosmickrisp_icd.json'
    assert 'DYLD_INSERT_LIBRARIES' not in env
    assert env['RECOMP_SHADER_CACHE']=='0'
    assert existing['VK_DRIVER_FILES']=='/wrong/driver'
    assert not list(tmp_path.iterdir())  # also safe for an interactive --check


def test_digest_works_with_system_python_api(tmp_path,monkeypatch):
    monkeypatch.delattr(sdk.hashlib,'file_digest',raising=False)
    data=b'known receipt contents'*100000
    path=tmp_path/'library';path.write_bytes(data)
    assert sdk.sha256(path)==sdk.hashlib.sha256(data).hexdigest()


@pytest.mark.parametrize('output',[
    'validation off',
    'validation on\n[VKVAL] vkCmdDraw(): PointSize is not written',
    'validation on\n[VKVAL error] error'])
def test_validation_errors_fail_the_contract(output):
    with pytest.raises(RuntimeError):sdk.require_clean_validation(output)


def test_validation_warning_is_distinct():
    sdk.require_clean_validation('validation on\n[VKVAL warning] optional capability')


@pytest.mark.parametrize('manifest,override,expected', [
    ({},None,'kosmickrisp'),  # old binaries keep their original driver
    ({'vulkan_driver':'moltenvk'},None,'moltenvk'),
    ({'vulkan_driver':'kosmickrisp'},None,'kosmickrisp'),
    ({'vulkan_driver':'moltenvk'},'kosmickrisp','kosmickrisp'),
])
def test_manifest_driver_and_temporary_override_are_read_only(tmp_path,monkeypatch,manifest,override,expected):
    before=dict(manifest)
    monkeypatch.setattr(sdk,'verify_install',lambda root:root/'sdk')
    env=sdk.launch_environment(tmp_path,manifest,override)
    assert Path(env['VK_DRIVER_FILES']).name==sdk.DRIVERS[expected]
    assert manifest==before and not list(tmp_path.iterdir())


@pytest.mark.parametrize('invalid',[None,'',True,[],{},'other','../other'])
def test_invalid_manifest_driver_rejected_before_sdk_access(tmp_path,monkeypatch,invalid):
    def unexpected(root):pytest.fail('Invalid driver reached SDK access')
    monkeypatch.setattr(sdk,'verify_install',unexpected)
    with pytest.raises(ValueError,match='Unknown pinned Vulkan driver'):
        sdk.launch_environment(tmp_path,{'vulkan_driver':invalid})
