"""No privilege or launchd side effects: contracts for the native monitor proof."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'tools/mods/hosting/probe-system-scan-monitor-mac.py'
spec=importlib.util.spec_from_file_location('monitor_proof',SOURCE)
probe=importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def test_default_mode_is_plan_only_without_identity_or_privilege():
    result=subprocess.run([sys.executable,'-B',str(SOURCE)],capture_output=True,check=True)
    assert json.loads(result.stdout)==dict(schema=1,phase='plan-only',temporary_jobs=2,
                                         network_started=False,production_services_deployed=False)


@pytest.mark.parametrize('nonce',['','../outside','A'*32,'a'*31,'g'*32,None])
def test_nonce_does_not_admit_arbitrary_paths(nonce):
    with pytest.raises(probe.identity.ProvisionError):probe.fixture_path(nonce)


@pytest.mark.parametrize('role',['target','controller'])
def test_roles_are_nonpersistent_isolated_and_bound_to_the_fixture(role,monkeypatch):
    fixture=probe.fixture_path('a'*32)
    monkeypatch.setattr(probe.monitor,'ROOT',fixture)
    release=probe.identity.ROOT/'releases'/('b'*64)
    python=release/'runtime/bin/python3.12'
    value=probe.job(role,release,fixture,python)
    assert value['Label']==probe.monitor.label()+('.proof-monitor' if role=='controller' else '')
    assert value['UserName']==('root' if role=='controller' else '_tsfpworkshop')
    assert value['InitGroups'] is False and value['KeepAlive'] is False and value['AbandonProcessGroup'] is False
    assert value['ProgramArguments'][:4]==[str(python),'-I','-S','-B']
    assert value['HardResourceLimits']==dict(Core=0,CPU=30,FileSize=8192,NumberOfFiles=64)
    assert value['StandardOutPath']==value['StandardErrorPath']=='/dev/null'
    assert '--system-worker' not in value['ProgramArguments'] and '--watchdog' not in value['ProgramArguments']
    for bad in [('other',release,fixture,python),(role,release,fixture,Path('/untrusted/python')),
                (role,Path('/untrusted/release'),fixture,python)]:
        with pytest.raises(probe.identity.ProvisionError):probe.job(*bad)


def test_apply_requires_installed_root_gate_before_creating_files(monkeypatch):
    def denied():raise probe.identity.ProvisionError('installed root required')
    monkeypatch.setattr(probe.monitor,'installed',denied)
    with pytest.raises(probe.identity.ProvisionError,match='installed root'):probe.apply()


def test_cleanup_attempts_all_owned_labels_even_after_a_timeout(monkeypatch):
    calls=[]
    def launch(*args):
        calls.append(args)
        if args==('bootout','system/two'):raise subprocess.TimeoutExpired(args,10)
        return SimpleNamespace(returncode=1)
    monkeypatch.setattr(probe,'launchctl',launch)
    probe.cleanup(['one','two'])
    assert calls==[('bootout','system/two'),('bootout','system/one'),('print','system/one'),('print','system/two')]


def test_cleanup_does_not_claim_success_when_a_job_remains(monkeypatch):
    monkeypatch.setattr(probe,'launchctl',lambda *args:SimpleNamespace(returncode=0))
    with pytest.raises(probe.identity.ProvisionError,match='not confirmed unloaded'):probe.cleanup(['owned'])


def test_no_user_target_or_executable_cli_options(monkeypatch):
    monkeypatch.setattr(sys,'argv',['proof','--apply','--target','system/unrelated'])
    with pytest.raises(SystemExit) as error:probe.main()
    assert error.value.code==2


def test_worker_is_valid_python_and_preserves_identity_diagnostics():
    compile(probe.WORKER,'inert proof worker','exec')
    assert "value['identity_ok']" in probe.WORKER and "value['phase']='identity-refused'" in probe.WORKER
