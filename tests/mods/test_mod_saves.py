"""Native persistent save namespaces; personal saves are only disposable canaries."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

import pytest
from test_script_process import process_tools

PROFILE='a'*64


def command(tools,originals,profile=PROFILE,mode='open'):
    worker=tools['worker']
    return [str(worker.with_name('tsfp-mod-saves-driver'+worker.suffix)),mode,str(originals),profile]


def call(tools,originals,profile=PROFILE,mode='open'):
    p=subprocess.run(command(tools,originals,profile,mode),capture_output=True,timeout=10)
    assert p.returncode==0 and p.stderr==b'',(p.returncode,p.stdout,p.stderr)
    return json.loads(p.stdout)['accepted']


@contextlib.contextmanager
def held(tools,originals,profile=PROFILE):
    p=subprocess.Popen(command(tools,originals,profile,'hold'),stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        assert json.loads(p.stdout.readline())=={'accepted':True}
        yield p
    finally:
        if p.poll() is None:
            out,err=p.communicate(b'\n',timeout=10)
            assert p.returncode==0 and not err,(p.returncode,out,err)


@pytest.fixture
def saves(tmp_path):
    originals=tmp_path/'Saves';originals.mkdir();(originals/'personal.sav').write_bytes(b'original progress')
    return originals,tmp_path/'ModSaves'/PROFILE


def test_persistent_namespace_never_changes_or_copies_originals(process_tools,saves):
    originals,root=saves;before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in originals.iterdir()}
    assert call(process_tools,originals)
    assert root.is_dir() and not (root/'personal.sav').exists()
    (root/'UserData').mkdir();(root/'UserData/progress.sav').write_bytes(b'isolated progress')
    assert call(process_tools,originals) and (root/'UserData/progress.sav').read_bytes()==b'isolated progress'
    assert before=={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in originals.iterdir()}


def test_absent_original_root_is_not_created(process_tools,tmp_path):
    originals=tmp_path/'Absent Saves'
    assert call(process_tools,originals) and not originals.exists()


def test_unicode_and_space_paths_work_without_original_mutation(process_tools,tmp_path):
    parent=tmp_path/'Player \u03a9 data';parent.mkdir();originals=parent/'Normal saves'
    originals.mkdir();(originals/'progress').write_text('original')
    assert call(process_tools,originals) and (originals/'progress').read_text()=='original'


def test_explicit_same_filesystem_relocation_preserves_both_namespaces(process_tools,saves):
    originals,root=saves;assert call(process_tools,originals)
    (root/'progress.sav').write_text('mod progress')
    source=originals.parent;destination=source.with_name(source.name+' relocated')
    assert source.resolve().is_relative_to(source.parent.resolve())
    assert destination.resolve().is_relative_to(source.parent.resolve()) and not destination.exists()
    source.rename(destination)
    assert call(process_tools,destination/'Saves')
    assert (destination/'ModSaves'/PROFILE/'progress.sav').read_text()=='mod progress'
    assert (destination/'Saves/personal.sav').read_bytes()==b'original progress'


def test_exclusive_session_and_different_profile_independence(process_tools,saves):
    originals,root=saves
    with held(process_tools,originals) as p:
        assert not call(process_tools,originals)
        assert call(process_tools,originals,'b'*64)
        out,err=p.communicate(b'\n',timeout=10)
        assert p.returncode==0 and not err and json.loads(out)=={'rechecked':True}
    assert call(process_tools,originals)


def test_abrupt_parent_exit_releases_lease_and_preserves_namespace(process_tools,saves):
    originals,root=saves
    p=subprocess.run(command(process_tools,originals,mode='crash'),capture_output=True,timeout=10)
    assert p.returncode==19 and not p.stderr and json.loads(p.stdout)=={'accepted':True}
    (root/'keep.sav').write_text('progress after crash')
    assert call(process_tools,originals) and (root/'keep.sav').read_text()=='progress after crash'


@pytest.mark.parametrize('profile',['','a'*63,'a'*65,'A'*64,'g'*64,'../'+'a'*64,'a'*63+'\n'])
def test_bad_profile_is_refused_without_namespace_creation(process_tools,saves,profile):
    originals,root=saves
    assert not call(process_tools,originals,profile) and not root.parent.exists()


@pytest.mark.parametrize('kind',['relative','dot','dotdot','reserved','reserved-case','file'])
def test_ambiguous_original_roots_are_refused(process_tools,saves,kind):
    originals,root=saves
    path={'relative':'Saves','dot':str(originals.parent)+os.sep+'.'+os.sep+'Saves',
          'dotdot':str(originals.parent)+os.sep+'..'+os.sep+'Saves','reserved':originals.parent/'ModSaves',
          'reserved-case':originals.parent/'mOdSaVeS','file':originals/'personal.sav'}[kind]
    assert not call(process_tools,path) and not root.parent.exists()


@pytest.mark.parametrize('kind',['base-incomplete','namespace-incomplete','base-marker','namespace-marker'])
def test_unknown_or_tampered_owned_markers_are_preserved_and_refused(process_tools,saves,kind):
    originals,root=saves
    if 'incomplete' in kind:
        if kind=='base-incomplete':root.parent.mkdir()
        else:
            assert call(process_tools,originals)
            (root/'.tsfp-mod-saves').unlink()
    else:
        assert call(process_tools,originals)
        path=root.parent/'.tsfp-mod-saves' if kind=='base-marker' else root/'.tsfp-mod-saves'
        path.write_text('unknown marker; do not repair automatically')
    keep=root.parent/'keep.txt';keep.write_text('personal data')
    assert not call(process_tools,originals) and keep.read_text()=='personal data'


@pytest.mark.parametrize('kind',['link','hardlink','directory-link'])
def test_aliases_to_originals_are_refused_without_mutation(process_tools,saves,kind):
    originals,root=saves;assert call(process_tools,originals)
    target=originals if kind=='directory-link' else originals/'personal.sav';alias=root/'alias'
    try:
        if kind=='hardlink':os.link(target,alias)
        else:alias.symlink_to(target,target_is_directory=kind=='directory-link')
    except OSError as error:pytest.skip('Host cannot create this link: '+str(error))
    assert not call(process_tools,originals) and alias.exists() and (originals/'personal.sav').read_bytes()==b'original progress'


@pytest.mark.parametrize('kind',['marker','hardlink','unknown','deep'])
def test_live_recheck_detects_changes_and_preserves_unknown_files(process_tools,saves,kind):
    originals,root=saves
    with held(process_tools,originals) as p:
        if kind=='marker':(root/'.tsfp-mod-saves').write_text('modified')
        if kind=='hardlink':os.link(originals/'personal.sav',root/'alias')
        if kind=='unknown':(root/'keep.txt').write_text('unrecognized personal file')
        if kind=='deep':
            path=root
            for _ in range(17):path=path/'nested';path.mkdir()
        out,err=p.communicate(b'\n',timeout=10)
        assert not err and p.returncode==0 and json.loads(out)=={'rechecked':kind=='unknown'}
    assert (originals/'personal.sav').read_bytes()==b'original progress'
    if kind=='unknown':assert (root/'keep.txt').read_text()=='unrecognized personal file'


@pytest.mark.parametrize('kind',['original','parent','namespace'])
def test_linked_roots_cannot_redirect_namespace_creation(process_tools,saves,kind):
    originals,root=saves
    alias=originals.parent/'redirect'
    try:
        if kind=='original':alias.symlink_to(originals,target_is_directory=True);path=alias
        elif kind=='parent':alias.symlink_to(originals.parent,target_is_directory=True);path=alias/'Saves'
        else:alias=root.parent;alias.symlink_to(originals,target_is_directory=True);path=originals
    except OSError as error:pytest.skip('Host cannot create directory links: '+str(error))
    assert not call(process_tools,path)
    assert (originals/'personal.sav').read_bytes()==b'original progress'
    assert set(p.name for p in originals.iterdir())=={'personal.sav'}


@pytest.mark.skipif(os.name=='nt',reason='POSIX filesystem modes and special objects')
@pytest.mark.parametrize('kind',['parent-mode','namespace-mode','file-mode','fifo','large-sparse'])
def test_posix_private_tree_and_metadata_only_validation(process_tools,saves,kind):
    originals,root=saves;assert call(process_tools,originals)
    changed=None
    if kind=='parent-mode':changed=originals.parent
    if kind=='namespace-mode':changed=root
    if kind=='file-mode':changed=root/'unsafe';changed.write_text('keep')
    if changed:changed.chmod(0o777)
    if kind=='fifo':os.mkfifo(root/'pipe')
    if kind=='large-sparse':
        with (root/'synthetic-disk').open('wb') as f:f.truncate(64*1024**3)
        assert (root/'synthetic-disk').stat().st_blocks==0
    try:assert call(process_tools,originals)==(kind=='large-sparse')
    finally:
        if changed:changed.chmod(0o700)


@pytest.mark.skipif(os.name!='nt',reason='Windows namespace aliases')
@pytest.mark.parametrize('suffix',['.',' ',':data'])
def test_windows_ambiguous_original_leaf_is_refused(process_tools,saves,suffix):
    originals,root=saves
    assert not call(process_tools,str(originals)+suffix) and not root.parent.exists()


@pytest.mark.skipif(os.name!='nt',reason='Native Windows ACL enforcement')
@pytest.mark.parametrize('kind',['parent','namespace','file'])
def test_windows_broad_write_acl_is_refused(process_tools,saves,kind):
    originals,root=saves;assert call(process_tools,originals)
    changed={'parent':originals.parent,'namespace':root,'file':root/'shared.sav'}[kind]
    if kind=='file':changed.write_text('preserve this file')
    grant='*S-1-1-0:' + ('M' if kind=='file' else '(OI)(CI)M')
    result=subprocess.run(['icacls',str(changed),'/grant',grant],capture_output=True,timeout=10)
    assert result.returncode==0,(result.stdout,result.stderr)
    assert not call(process_tools,originals)
    assert (originals/'personal.sav').read_bytes()==b'original progress'
