"""Credential drop ordering, irreversible privilege loss and fixed worker roles."""
import importlib.util
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('service_bootstrap',ROOT/'tools/mods/hosting/locked-service-bootstrap-mac.py')
bootstrap=importlib.util.module_from_spec(spec);spec.loader.exec_module(bootstrap)
from mod_package import ModError


@pytest.mark.parametrize('role,program,mode',[('scan','scan-service-mac.py','--system-worker'),
                                          ('definitions','definitions-service-mac.py','--system-once')])
def test_production_roles_are_fixed_and_have_no_supplied_command(role,program,mode):
    release=bootstrap.monitor.ROOT/'releases'/('a'*64)
    assert bootstrap.arguments(release,role)==['-I','-B',str(release/'source/tools/mods/hosting'/program),mode]
    with pytest.raises(ModError):bootstrap.arguments(release,role,'a'*32)
    with pytest.raises(ModError):bootstrap.arguments(release,'shell')


@pytest.mark.parametrize('recoverable',[False,True])
def test_groups_are_cleared_before_gid_uid_and_no_root_can_be_restored(monkeypatch,recoverable):
    calls=[]
    def seteuid(uid):
        calls.append(('restore',uid))
        if not recoverable:raise PermissionError('irreversible drop')
    fake=SimpleNamespace(geteuid=lambda:0,setgroups=lambda g:calls.append(('groups',g)),
                         setregid=lambda real,effective:calls.append(('gid',(real,effective))),
                         setreuid=lambda real,effective:calls.append(('uid',(real,effective))),seteuid=seteuid)
    monkeypatch.setattr(bootstrap,'os',fake)
    monkeypatch.setattr(bootstrap,'require_locked_process',lambda:calls.append(('check',600)))
    if recoverable:
        with pytest.raises(ModError,match='recoverable root'):bootstrap.drop(600,600)
    else:bootstrap.drop(600,600)
    assert calls==[('groups',[]),('gid',(600,600)),('uid',(600,600)),('check',600),('restore',0)]


@pytest.mark.parametrize('uid,gid',[(0,0),(501,501),(600,0),(600,601),(True,True)])
def test_invalid_target_identity_never_changes_credentials(monkeypatch,uid,gid):
    monkeypatch.setattr(bootstrap,'os',SimpleNamespace(geteuid=lambda:0))
    with pytest.raises(ModError):bootstrap.drop(uid,gid)


@pytest.mark.parametrize('failed',['groups','gid','uid'])
def test_credential_failure_stops_before_worker_verification_or_exec(monkeypatch,failed):
    calls=[]
    def operation(name,value):
        calls.append(name)
        if name==failed:raise PermissionError('injected drop failure')
    monkeypatch.setattr(bootstrap,'os',SimpleNamespace(geteuid=lambda:0,setgroups=lambda g:operation('groups',g),
                         setregid=lambda r,e:operation('gid',(r,e)),setreuid=lambda r,e:operation('uid',(r,e))))
    monkeypatch.setattr(bootstrap,'require_locked_process',lambda:pytest.fail('verified after failed drop'))
    with pytest.raises(PermissionError):bootstrap.drop(600,600)
    assert calls==['groups','gid','uid'][:['groups','gid','uid'].index(failed)+1]


def test_unprivileged_or_uninstalled_entry_is_refused(monkeypatch):
    monkeypatch.setattr(bootstrap.sys,'platform','win32')
    with pytest.raises(ModError,match='administrator'):bootstrap.monitor.installed()


def test_cli_has_no_executable_or_command_override(monkeypatch):
    monkeypatch.setattr(sys,'argv',['bootstrap','--role','scan','--command','anything'])
    with pytest.raises(SystemExit) as error:bootstrap.main()
    assert error.value.code==2
