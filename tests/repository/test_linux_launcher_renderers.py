"""Native selector fixtures must never overwrite unrelated or shared files."""
import importlib.util
import json
import os
from pathlib import Path
import pytest

pytest.importorskip('fcntl')
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('linux_selector',ROOT/'tools/testing/test-linux-launcher-renderers.py')
selector=importlib.util.module_from_spec(spec);spec.loader.exec_module(selector)


def test_unowned_session_is_rejected_without_writes(tmp_path):
    session=tmp_path/'launcher-session';session.mkdir();file=session/'keep';file.write_text('untouched')
    with pytest.raises(ValueError,match='not owned'):selector.stage(tmp_path,tmp_path,{})
    assert file.read_text()=='untouched' and list(session.iterdir())==[file]


def test_linked_or_escaping_routes_are_rejected(tmp_path):
    scope=tmp_path/'scope';scope.mkdir();outside=tmp_path/'outside';outside.mkdir()
    (scope/'linked').symlink_to(outside,target_is_directory=True)
    for path in (scope/'../outside/file',scope/'linked/file'):
        with pytest.raises(ValueError):selector.unlinked(path,scope)
    assert not list(outside.iterdir())


def test_dead_owned_session_is_archived_but_live_installation_is_retained(tmp_path):
    session=tmp_path/'session';session.mkdir();(session/'.launcher').mkdir()
    (session/'renderer-test-install.json').write_text(json.dumps(selector.FIXTURE_OWNER))
    record=session/'.launcher/game-session.json';record.write_text(json.dumps({'state':'active','launcher_pid':12,'game_pid':13}))
    evidence=tmp_path/'evidence';evidence.mkdir();proc=tmp_path/'proc';proc.mkdir()
    selector.archive_stale_session(session,evidence,proc)
    assert not record.exists() and json.loads((evidence/'previous-game-session.json').read_text())['game_pid']==13
    record.write_text(json.dumps({'state':'active','launcher_pid':12}))
    identity=proc/'12';identity.mkdir();(identity/'cmdline').write_bytes(b'launcher\0--root\0'+os.fsencode(session)+b'\0')
    before=record.read_bytes()
    with pytest.raises(ValueError,match='live process'):selector.archive_stale_session(session,evidence,proc)
    assert record.read_bytes()==before


def test_unknown_live_process_state_cannot_be_discarded(tmp_path):
    session=tmp_path/'session';session.mkdir();(session/'.launcher').mkdir()
    (session/'renderer-test-install.json').write_text(json.dumps(selector.FIXTURE_OWNER))
    record=session/'.launcher/game-session.json';record.write_text(json.dumps({'state':'active','launcher_pid':12}))
    proc=tmp_path/'proc';identity=proc/'12';identity.mkdir(parents=True)
    (identity/'cmdline').write_bytes(b'');(identity/'status').write_text('State:\tR (running)\n')
    with pytest.raises(ValueError,match='unknown'):selector.archive_stale_session(session,tmp_path,proc)
    assert record.exists()


def test_restage_relinks_without_overwriting_shared_executable(tmp_path):
    root=tmp_path;scope=root/'private/test-saves/native-linux';scope.mkdir(parents=True)
    source=root/'build/linux/game/futureperfect_recomp';source.parent.mkdir(parents=True);source.write_bytes(b'first ELF')
    backup=root/'original';os.link(source,backup)
    game=root/'private/game';game.mkdir();(game/'default.xbe').write_bytes(b'fixture')
    controls=root/'tests/fixtures/pc-controls.json';controls.parent.mkdir(parents=True);controls.write_text('{}')
    manifest=dict(source_revision='test',renderer='vulkan',supported_renderers=['vulkan','opengl'],executable_sha256=selector.digest(source))
    session=selector.stage(root,scope,manifest)
    saved=session/'Settings/pc-controls.json';model=json.loads(saved.read_text());model['unrelated']='keep';saved.write_text(json.dumps(model))
    replacement=source.with_name('replacement');replacement.write_bytes(b'new ELF');replacement.replace(source)
    manifest['executable_sha256']=selector.digest(source)
    selector.stage(root,scope,manifest)
    assert backup.read_bytes()==b'first ELF' and source.read_bytes()==b'new ELF'
    assert (session/'Bin/futureperfect_recomp').samefile(source)
    assert json.loads(saved.read_text())['unrelated']=='keep'
