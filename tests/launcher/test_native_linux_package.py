"""Linux delivery boundaries: immutable payload, ELF closure and writable state."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('linux_package',ROOT/'tools/release/package-native-linux.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)

def payload(root):
    names=['Bin/libdxcompiler.so','Bin/futureperfect_recomp','TimeSplitters.Launcher','Launch.sh','Settings/pc-controls.json',
        'Support/Python/bin/python3','Support/Online/online.py','Support/Online/network-identity.json','Source/pc-port-source.zip',
        'Bin/libavcodec.so.60','Bin/libavutil.so.58','Source/FFmpeg/pin.json','Source/FFmpeg/linux_ffmpeg.py','Source/FFmpeg/source.tar.gz',
        'Licenses/SDL.txt','Licenses/DXC.txt','Licenses/DXC-ThirdParty.txt','Licenses/Runtime-GPL-3.0.txt','Licenses/FFmpeg-COPYING.LGPLv2.1.txt','Support/Runtime/dependency-audit.json',
        'Bin/libICE.so.6','Bin/libSM.so.6','Support/Desktop/gdk-pixbuf-query-loaders',
        'Support/Desktop/pixbuf/libpixbufloader-svg.so','Support/Desktop/fonts.conf',
        'Support/Desktop/share/mime/mime.cache','Support/Desktop/share/mime/packages/freedesktop.org.xml']
    for name in names:
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'fixture')
    (root/'BUILD.json').write_text('{}')
    (root/'Support/Runtime/dependency-audit.json').write_text(json.dumps(dict(components={},source_packages={},data_components={'Support/Desktop/share/mime/packages/freedesktop.org.xml':dict(sha256=package.digest(root/'Support/Desktop/share/mime/packages/freedesktop.org.xml'))})))
    return dict(media=dict(archive='source.tar.gz',sha256=package.digest(root/'Source/FFmpeg/source.tar.gz')),files={name:package.digest(root/name) for name in names})

def test_launcher_sidecars_cannot_be_silently_rehashed(tmp_path):
    (tmp_path/'Support/Launcher').mkdir(parents=True)
    (tmp_path/'TimeSplitters.Launcher').write_bytes(b'exe')
    side=tmp_path/'Support/Launcher/notice.txt';side.write_bytes(b'notice')
    files={p.relative_to(tmp_path).as_posix():package.digest(p) for p in tmp_path.rglob('*') if p.is_file()}
    (tmp_path/'Support/Launcher/manifest.json').write_text(json.dumps(dict(files=files)))
    package.verify_launcher(tmp_path);side.write_bytes(b'changed')
    with pytest.raises(ValueError,match='checksum'):package.verify_launcher(tmp_path)

def test_complete_payload_and_corruption(tmp_path):
    manifest=payload(tmp_path);package.verify_payload(tmp_path,manifest)
    (tmp_path/'Bin/futureperfect_recomp').write_bytes(b'changed')
    with pytest.raises(ValueError,match='checksum'):package.verify_payload(tmp_path,manifest)


def test_release_assets_pin_actual_preview_and_launcher_without_publishing(tmp_path):
    archive=tmp_path/'TimeSplitters-Future-Perfect-PC-v0.4.2-linux-experimental.1-linux-x64.zip'
    archive.write_bytes(b'archive')
    manifest=dict(version='v0.4.2-linux-experimental.1',packaging_commit='a'*40,
        release_channel=dict(schema=1,channel='linux-experimental',name='Linux Experimental Preview',minimum_launcher='1.4.0'))
    package.write_release_assets(manifest,archive)
    notes=archive.with_suffix('.notes.md').read_text()
    metadata=json.loads(notes.split('<!-- tsfp-release ',1)[1].split(' -->',1)[0])
    assert metadata['channel']=='linux-experimental' and metadata['minimum_launcher']=='1.4.0'
    assert metadata['platforms']==['linux-x64'] and metadata['source_commit']=='a'*40
    assert json.loads(archive.with_suffix('.BUILD.json').read_text())==manifest
    assert archive.with_suffix('.zip.sha256').read_text()==package.digest(archive)+'  '+archive.name+'\n'

@pytest.mark.parametrize('name',['../outside','/absolute','Data/default.xbe','data/file','Saves/save','private/data','Textures/image','Settings/runtime.conf','Bin/movie.xmv','Support/a.img','Support\\bad',''])
def test_protected_payload_paths(tmp_path,name):
    manifest=payload(tmp_path);manifest['files'][name]='0'*64
    with pytest.raises(ValueError,match='Protected|Personal'):package.verify_payload(tmp_path,manifest)

def test_unlisted_member_and_missing_component(tmp_path):
    manifest=payload(tmp_path);(tmp_path/'unknown').write_bytes(b'unknown')
    with pytest.raises(ValueError,match='Unlisted'):package.verify_payload(tmp_path,manifest)
    del manifest['files']['Launch.sh']
    with pytest.raises(ValueError,match='Incomplete'):package.verify_payload(tmp_path,manifest)

@pytest.mark.parametrize('valid',[True,False])
def test_x64_elf_header(tmp_path,valid):
    header=bytearray(20);header[:6]=b'\x7fELF\x02\x01';header[18]=62 if valid else 183
    p=tmp_path/'elf';p.write_bytes(header);assert package.elf(p)==valid

def test_dependency_resolution_and_unresolved_soname(monkeypatch,tmp_path):
    monkeypatch.setattr(package,'run',lambda *a:'libgtk-3.so.0 => /usr/lib/libgtk-3.so.0 (0x123)\nlinux-vdso.so.1 (0x234)')
    assert package.dependencies(tmp_path/'game')==[('libgtk-3.so.0',Path('/usr/lib/libgtk-3.so.0').resolve())]
    monkeypatch.setattr(package,'run',lambda *a:'libgtk-3.so.0 => not found')
    with pytest.raises(ValueError,match='Unresolved'):package.dependencies(tmp_path/'game')

@pytest.mark.skipif(os.name=='nt',reason='POSIX executable and XDG paths')
def test_game_entrypoint_uses_xdg_and_preserves_settings(tmp_path):
    root=tmp_path/'read only package';root.mkdir();(root/'Bin').mkdir();(root/'Settings').mkdir()
    (root/'Settings/pc-controls.json').write_text('{"default":true}')
    game=root/'Bin/futureperfect_recomp';game.write_text('#!/bin/sh\nprintf \"%s\\n\" \"$RECOMP_INPUT_CONFIG\" \"$RECOMP_SAVE_DIR\" \"$RECOMP_GAME_DIR\" \"$TSFP_ONLINE_SCRIPT\" \"$@\"\n');game.chmod(0o755)
    script=root/'Launch.sh';script.write_bytes((ROOT/'tools/release/linux-launch.sh').read_bytes());script.chmod(0o755)
    env=dict(os.environ,HOME=str(tmp_path/'home'),XDG_CONFIG_HOME=str(tmp_path/'config space'),XDG_DATA_HOME=str(tmp_path/'data'),XDG_STATE_HOME=str(tmp_path/'state'),XDG_CACHE_HOME=str(tmp_path/'cache'))
    result=subprocess.run([str(script),'--game','--data',str(tmp_path/'Game Data'),'--settings-self-test'],env=env,text=True,capture_output=True,check=True)
    lines=result.stdout.splitlines();config=Path(lines[0]);assert config.read_text()=='{"default":true}'
    assert lines[1]==str(tmp_path/'data/timesplitters-future-perfect/Saves')
    assert lines[2]==str(tmp_path/'Game Data') and lines[-1]=='--settings-self-test'
    config.write_text('{"personal":true}')
    subprocess.run([str(script),'--game'],env=env,capture_output=True,check=True)
    assert config.read_text()=='{"personal":true}'
    bad=subprocess.run([str(script),'--game','--bad'],env=env,capture_output=True);assert bad.returncode==2


def test_companion_source_layout_is_complete():
    for name in package.COMPANION:
        assert package.repo_file(ROOT,'tools',name).is_file(),name


def test_desktop_data_provenance_is_required(tmp_path):
    manifest=payload(tmp_path)
    audit=tmp_path/'Support/Runtime/dependency-audit.json'
    report=json.loads(audit.read_text());report['data_components']={}
    audit.write_text(json.dumps(report));manifest['files'][audit.relative_to(tmp_path).as_posix()]=package.digest(audit)
    with pytest.raises(ValueError,match='desktop data audit'):package.verify_payload(tmp_path,manifest)
