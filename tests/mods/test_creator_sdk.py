"""Run exported SDK tools outside the repository, with no server/site imports."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

from PIL import Image
import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('sdk_package',ROOT/'tools/release/package-creator-sdk.py')
sdk=importlib.util.module_from_spec(spec);spec.loader.exec_module(sdk)


@pytest.fixture
def exported(tmp_path):
    archive=tmp_path/'sdk.zip'
    sdk.package_sdk(ROOT,archive,require_clean=False)
    with zipfile.ZipFile(archive) as packed:
        packed.extractall(tmp_path/'unpacked')
    folder=tmp_path/'unpacked'/('TimeSplitters-Mod-Creator-SDK-v'+sdk.VERSION)
    return folder,archive


def run(folder,tmp_path,*args,pillow=False,success=True):
    # -I discards injected PYTHONPATH and CWD; -S proves standard-library commands
    # need neither this repo's dependencies nor a running/installed workshop.
    env=dict(os.environ,PYTHONPATH=str(ROOT/'tools/mods'))
    command=[sys.executable,'-I',* ([] if pillow else ['-S']),str(folder/'tools/mods/mod-creator.py'),*map(str,args)]
    result=subprocess.run(command,cwd=tmp_path,env=env,capture_output=True,text=True,timeout=30)
    assert result.returncode==(0 if success else 2),(result.stdout,result.stderr)
    return result


def test_export_is_deterministic_and_contains_only_reviewed_creator_inputs(exported,tmp_path):
    folder,archive=exported
    second=tmp_path/'second.zip';sdk.package_sdk(ROOT,second,require_clean=False)
    assert archive.read_bytes()==second.read_bytes()
    manifest=json.loads((folder/'SDK.json').read_text())
    import hashlib
    assert set(manifest['files'])=={p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}-{'SDK.json'}
    for name,item in manifest['files'].items():
        data=(folder/name).read_bytes()
        assert len(data)==item['size'] and hashlib.sha256(data).hexdigest()==item['sha256']
    assert not any('mod_repository' in name or 'hosting' in name or name.startswith(('private/','config/')) for name in manifest['files'])
    assert (folder/'requirements-sdk.txt').read_text()=='Pillow==12.2.0\n'
    assert '(API.md)' in (folder/'README.md').read_text(encoding='utf-8')
    assert (folder/'API.md').is_file()
    assert archive.with_suffix('.zip.sha256').read_text().startswith(hashlib.sha256(archive.read_bytes()).hexdigest()+'  ')
    with pytest.raises(ValueError,match='new ZIP'):
        sdk.package_sdk(ROOT,archive,require_clean=False)


@pytest.mark.parametrize('template',['gameplay','mixed','audio','textures'])
def test_detached_creator_workflow_without_git_game_or_server(exported,tmp_path,template):
    folder,_=exported;project=tmp_path/'authored project'
    run(folder,tmp_path,'new',project,'--id','authored','--name','Original mode','--author','creator','--template',template)
    if template in ('mixed','textures'):
        source=tmp_path/'original.png';Image.new('RGBA',(4,4),(32,64,96,128)).save(source)
        before=source.read_bytes()
        run(folder,tmp_path,'import-texture',project,source,'--key','0123456789abcdef','--format','RGBA8',pillow=True)
        assert source.read_bytes()==before
    if template=='audio':
        wave=tmp_path/'original.wav';data=(project/'audio/00000001.wav').read_bytes();wave.write_bytes(data)
        result=run(folder,tmp_path,'import-audio',project,wave,'--key','00000003')
        assert json.loads(result.stdout)['frames']==22050
        assert (project/'audio/00000003.wav').read_bytes()==wave.read_bytes()==data
        run(folder,tmp_path,'import-audio',project,wave,'--key','00000003',success=False)
    archive=tmp_path/'authored.zip'
    run(folder,tmp_path,'pack',project,archive)
    result=run(folder,tmp_path,'validate',archive)
    manifest=json.loads(result.stdout)
    assert manifest['id']=='authored' and manifest['author']=='creator'
    assert ('scripts/main.lua' in manifest['files'])==(template!='textures')
    run(folder,tmp_path,'pack',project,archive,success=False)


def test_detached_sdk_links_original_modular_sample_without_site_packages(exported,tmp_path):
    folder,_=exported
    project=tmp_path/'modules';shutil.copytree(folder/'examples/mods/modular-first-to-three',project)
    archive=tmp_path/'modules.zip'
    run(folder,tmp_path,'pack',project,archive,'--modules')
    result=run(folder,tmp_path,'validate',archive)
    assert set(json.loads(result.stdout)['files'])=={'scripts/main.lua'}
    with zipfile.ZipFile(archive) as packed:
        assert b'__tsfp_loaders' in packed.read('scripts/main.lua')


def test_creator_only_cli_excludes_operator_commands(exported,tmp_path):
    folder,_=exported
    help_result=run(folder,tmp_path,'--help')
    assert 'import-audio' in help_result.stdout and 'scan-next' not in help_result.stdout
    run(folder,tmp_path,'approve','--repository',tmp_path,success=False)


def test_release_package_refuses_dirty_source_before_writing(tmp_path,monkeypatch):
    def fake_git(root,*args):
        return 'a'*40 if args[0]=='rev-parse' else ' M tools/mods/mod_creator.py'
    monkeypatch.setattr(sdk,'git',fake_git)
    with pytest.raises(ValueError,match='Commit SDK'):
        sdk.package_sdk(ROOT,tmp_path/'new.zip')
    assert not list(tmp_path.iterdir())


def test_release_package_refuses_linked_source_before_writing(tmp_path,monkeypatch):
    source=tmp_path/'source';source.mkdir()
    outside=tmp_path/'outside';outside.mkdir()
    (outside/'LICENSE').write_text('private sentinel')
    try:(source/'LICENSE').symlink_to(outside/'LICENSE')
    except (OSError,NotImplementedError):pytest.skip('OS does not allow fixture symlinks')
    def fake_git(root,*args):
        return 'a'*40 if args[0]=='rev-parse' else ('\0'.join(sdk.FILES) if args[0]=='ls-files' else '')
    monkeypatch.setattr(sdk,'git',fake_git)
    with pytest.raises(ValueError,match='linked'):
        sdk.package_sdk(source,tmp_path/'new.zip')
    assert not (tmp_path/'new.zip').exists() and (outside/'LICENSE').read_text()=='private sentinel'
