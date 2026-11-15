"""Private initialization preserves existing data and never starts services."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'tools/mods/hosting/initialize-private-scan-mac.py'
spec=importlib.util.spec_from_file_location('private_scan_init',SOURCE)
init=importlib.util.module_from_spec(spec);spec.loader.exec_module(init)


def test_default_plan_has_no_privileged_or_service_side_effects():
    run=subprocess.run([sys.executable,'-B',str(SOURCE)],check=True,capture_output=True)
    value=json.loads(run.stdout)
    assert value['phase']=='plan-only' and value['account']=='_tsfpworkshop'
    assert value['services_started'] is False and value['network_started'] is False


@pytest.fixture
def layout(tmp_path,monkeypatch):
    if os.name!='posix':pytest.skip('Real private owner/mode and fd-relative operations')
    monkeypatch.setattr(init,'ROOT',tmp_path)
    tmp_path.chmod(0o700);(tmp_path/'data').mkdir(mode=0o700)
    return tmp_path/'releases'/('a'*64),tmp_path/'data/scan'


def test_initialization_reuses_exact_layout_without_rewriting_live_data(layout):
    release,root=layout
    assert init.initialize(release)['phase']=='ready'
    assert set(p.name for p in root.iterdir())=={'service.json','definitions.json','initialization.json',*init.DIRECTORIES}
    before={p.name:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in root.iterdir() if p.is_file()}
    for name in init.DIRECTORIES:assert (root/name).stat().st_mode&0o777==0o700
    for p in root.iterdir():
        if p.is_file():assert p.stat().st_mode&0o777==0o600
    sentinel=root/'repository/preserve-existing-data';sentinel.write_bytes(b'original private data')
    assert init.initialize(release)['phase']=='ready'
    assert before=={p.name:(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in root.iterdir() if p.is_file()}
    assert sentinel.read_bytes()==b'original private data'
    scan=json.loads((root/'service.json').read_bytes());defs=json.loads((root/'definitions.json').read_bytes())
    assert scan['database']==defs['database']==str(root/'database')
    assert scan['repository']==str(root/'repository') and scan['scanner'].startswith(str(release/'runtime'))


@pytest.mark.parametrize('damage',['unknown','changed','partial','new-release','hardlink','writable','fifo','symlink'])
def test_changed_or_foreign_state_is_preserved(layout,damage):
    release,root=layout
    init.initialize(release)
    path=root/'service.json'
    if damage=='unknown':(root/'personal').write_bytes(b'preserve')
    elif damage=='changed':path.write_bytes(b'personal content')
    elif damage=='partial':
        marker=json.loads((root/'initialization.json').read_bytes());marker['phase']='preparing'
        (root/'initialization.json').write_bytes(init.wire(marker))
    elif damage=='new-release':release=release.with_name('b'*64)
    elif damage=='hardlink':os.link(path,root.parent/'linked-record')
    elif damage=='writable':path.chmod(0o644)
    elif damage=='fifo':path.unlink();os.mkfifo(path)
    else:
        outside=root.parent/'outside';path.rename(outside);path.symlink_to(outside)
    before={p.name:p.lstat().st_ino for p in root.iterdir()}
    with pytest.raises((ValueError,OSError)):init.initialize(release)
    assert before=={p.name:p.lstat().st_ino for p in root.iterdir()}
    if damage=='changed':assert path.read_bytes()==b'personal content'


def test_unrecognized_existing_directory_is_not_adopted(layout):
    release,root=layout
    root.mkdir(mode=0o700);sentinel=root/'personal';sentinel.write_bytes(b'keep')
    with pytest.raises((ValueError,OSError)):init.initialize(release)
    assert sentinel.read_bytes()==b'keep' and list(root.iterdir())==[sentinel]


def test_interrupted_initialization_is_withheld_and_preserved(layout,monkeypatch):
    release,root=layout
    original=init.new_member
    def interrupted(fd,name,value):
        if name=='definitions.json':raise OSError('Injected storage interruption')
        original(fd,name,value)
    monkeypatch.setattr(init,'new_member',interrupted)
    with pytest.raises(OSError,match='interruption'):init.initialize(release)
    assert json.loads((root/'initialization.json').read_bytes())['phase']=='preparing'
    with pytest.raises(ValueError,match='incomplete'):init.check(release)
    assert (root/'service.json').is_file() and not (root/'definitions.json').exists()


def test_installed_entry_gate_refuses_wrong_host_before_any_files(monkeypatch):
    monkeypatch.setattr(init.sys,'platform','win32')
    with pytest.raises(ValueError,match='Mac'):init.installed_release()
