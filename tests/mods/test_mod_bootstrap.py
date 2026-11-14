"""Real capsule/save lease plus storage state, with narrow native UI/kernel adapters."""
import base64
import json
import os
import subprocess

import pytest
from test_mod_capsule import VECTOR
from test_script_process import process_tools


@pytest.fixture
def launch(tmp_path):
    original=tmp_path/'Saves';original.mkdir()
    (original/'personal.sav').write_bytes(b'original progress')
    capsule=tmp_path/'launch.bin';capsule.write_bytes(base64.b64decode(VECTOR['wire']))
    environment={k:v for k,v in os.environ.items() if not k.startswith('TSFP_MOD_SCRIPT_CAPSULE')}
    environment.update(TSFP_MOD_SCRIPT_CAPSULE=str(capsule),TSFP_MOD_SCRIPT_CAPSULE_SHA256=VECTOR['sha256'])
    return original,capsule,environment


def command(tools,original,mode='run',preference='-'):
    worker=tools['worker']
    return [str(worker.with_name('tsfp-mod-bootstrap-driver'+worker.suffix)),mode,str(original),str(preference)]


def run(tools,launch,mode='run',preference='-'):
    original,capsule,environment=launch
    p=subprocess.run(command(tools,original,mode,preference),env=environment,capture_output=True,timeout=15)
    assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
    result=json.loads(p.stdout)
    assert not result['premature'] and not result['escape']
    assert (original/'personal.sav').read_bytes()==b'original progress'
    assert list(original.iterdir())==[original/'personal.sav']
    return result


def test_verified_source_is_only_published_after_isolated_storage_commit(process_tools,launch):
    r=run(process_tools,launch)
    assert r==dict(prepared=True,committed=True,ready=True,rechecked=True,locked=True,fence=1,escape=False,premature=False,
                   bytes=len(base64.b64decode(VECTOR['source'])),profile=VECTOR['profile'])
    root=launch[0].parent/'ModSaves'/VECTOR['profile']
    assert root.is_dir() and not (root/'personal.sav').exists()
    (root/'progress.sav').write_bytes(b'mod progress')
    assert run(process_tools,launch)==r
    assert (root/'progress.sav').read_bytes()==b'mod progress'


def test_absent_handoff_keeps_ordinary_storage_unfenced(process_tools,launch):
    env=launch[2];env.pop('TSFP_MOD_SCRIPT_CAPSULE');env.pop('TSFP_MOD_SCRIPT_CAPSULE_SHA256')
    r=run(process_tools,launch)
    assert r['prepared'] and r['committed'] and not r['ready'] and not r['locked'] and r['fence']==0
    assert not (launch[0].parent/'ModSaves').exists()


@pytest.mark.parametrize('kind',['missing-path','missing-pin','empty-pin','wrong-pin','missing-file','no-root','bad-preference','late'])
def test_invalid_request_closes_storage_before_any_source_is_available(process_tools,launch,kind):
    env=launch[2];mode=kind if kind in ('no-root','late') else 'run';preference='-'
    if kind=='missing-path':env.pop('TSFP_MOD_SCRIPT_CAPSULE')
    if kind=='missing-pin':env.pop('TSFP_MOD_SCRIPT_CAPSULE_SHA256')
    if kind=='empty-pin':env['TSFP_MOD_SCRIPT_CAPSULE_SHA256']=''
    if kind=='wrong-pin':env['TSFP_MOD_SCRIPT_CAPSULE_SHA256']='0'*64
    if kind=='missing-file':launch[1].unlink()
    if kind=='bad-preference':preference='fail'
    r=run(process_tools,launch,mode,preference)
    assert not r['prepared'] and not r['committed'] and not r['ready'] and r['locked'] and r['fence']==-1
    assert not r['profile'] and not r['bytes']


@pytest.mark.parametrize('mode',['no-storage','wrong-root','repeat'])
def test_missing_wrong_or_repeated_commit_never_publishes_source(process_tools,launch,mode):
    r=run(process_tools,launch,mode)
    assert r['prepared'] and not r['ready'] and not r['rechecked'] and r['locked']
    if mode!='repeat':assert not r['committed']


def test_selected_original_save_drive_is_resolved_before_picker_lock(process_tools,launch,tmp_path):
    selected=tmp_path/'Other drive'/'Saves';selected.parent.mkdir();selected.mkdir()
    (selected/'keep.sav').write_bytes(b'other progress')
    assert run(process_tools,launch,preference=selected)['ready']
    assert (selected.parent/'ModSaves'/VECTOR['profile']).is_dir()
    assert not (launch[0].parent/'ModSaves').exists()
    assert (selected/'keep.sav').read_bytes()==b'other progress'


def test_lease_survives_startup_and_recheck_withholds_source_after_marker_tamper(process_tools,launch):
    p=subprocess.Popen(command(process_tools,launch[0],'hold'),env=launch[2],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    try:
        assert json.loads(p.stdout.readline())=={'holding':True}
        # A second real game bootstrap cannot acquire this namespace.
        assert not run(process_tools,launch)['prepared']
        root=launch[0].parent/'ModSaves'/VECTOR['profile']
        (root/'.tsfp-mod-saves').write_text('changed marker')
        out,err=p.communicate(b'\n',timeout=10)
        assert p.returncode==0,(out,err)
        r=json.loads(out);assert not r['ready'] and not r['rechecked'] and r['locked']
    finally:
        if p.poll() is None:p.communicate(b'\n',timeout=10)
