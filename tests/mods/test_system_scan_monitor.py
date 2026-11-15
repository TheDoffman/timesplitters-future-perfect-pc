"""Fixed system-job control and real POSIX heartbeat file protections."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mods'))
from mod_package import ModError
spec = importlib.util.spec_from_file_location('system_monitor',ROOT/'tools/mods/hosting/system-scan-monitor-mac.py')
monitor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monitor)


def command_fixture(rows='456 1 1024 600',printed='pid = 456',kill=0):
    calls=[]
    def run(args):
        calls.append(args)
        if args[0]=='/bin/ps':return 0,rows
        if args[1]=='print':return 0,printed
        return kill,''
    return run,calls


def test_fixed_target_healthy_and_idle_and_restarted_worker():
    run,calls=command_fixture()
    state,report=monitor.watch_once(600,{},run,lambda:10)
    assert state==dict(pid=456,start=10) and report==dict(status='watching',pid=456,rss_bytes=1048576)
    assert calls[0]==['/bin/launchctl','print','system/'+monitor.label()]
    state,report=monitor.watch_once(600,dict(pid=123,start=-1000),run,lambda:20)
    assert state['start']==20 and report['status']=='watching'
    run,_=command_fixture(printed='state = waiting')
    assert monitor.watch_once(600,{},run)[1]==dict(status='idle')


@pytest.mark.parametrize('fault',['memory','wall','owner','malformed','process-count','missing','ambiguous','timeout'])
def test_faults_control_only_owned_launchd_label(fault):
    rows='456 1 1024 600'
    if fault=='memory':rows='456 1 '+str(monitor.MEMORY_BYTES//1024+1)+' 600'
    if fault=='owner':rows='456 1 1 501'
    if fault=='malformed':rows='untrusted process data'
    if fault=='process-count':rows+='\n'+'\n'.join(str(i)+' 456 1 600' for i in range(600,608))
    run,calls=command_fixture(rows,printed='pid = 456\npid = 123' if fault=='ambiguous' else 'pid = 456')
    def command(args):
        if args[1]=='print' and fault=='missing':calls.append(args);return 1,'not loaded'
        if args[1]=='print' and fault=='timeout':calls.append(args);raise subprocess.TimeoutExpired(args,3)
        return run(args)
    _,report=monitor.watch_once(600,dict(pid=456,start=0),command,lambda:481 if fault=='wall' else 1)
    assert report['status']==('terminated' if fault in ('memory','wall') else 'error')
    assert calls[-1]==['/bin/launchctl','kill','SIGKILL','system/'+monitor.label()]
    assert not any('456' in args for args in calls)


def test_permission_denial_never_claims_termination():
    run,calls=command_fixture('456 1 '+str(monitor.MEMORY_BYTES//1024+1)+' 600',kill=1)
    state,report=monitor.watch_once(600,{},run)
    assert state=={} and report['status']=='error'
    assert all(args[-1]=='system/'+monitor.label() for args in calls if args[0]=='/bin/launchctl')


@pytest.mark.parametrize('uid',[0,501,True,'600'])
def test_root_and_invalid_worker_identity_refused(uid):
    with pytest.raises(ModError):monitor.watch_once(uid,{},lambda _:pytest.fail('invalid identity reached OS'))


def test_plists_separate_privileges_and_initialize_restricted_credentials():
    release=monitor.ROOT/'releases'/('a'*64)
    python=release/'runtime/bin/python3.12'
    worker,watch=monitor.plists(release,dict(uid=600,gid=600),python)
    assert worker['UserName']=='_tsfpworkshop' and watch['UserName']=='root'
    assert worker['InitGroups'] is True and watch['InitGroups'] is False
    assert worker['ProgramArguments']==[str(python),'-I','-B',str(release/'source/tools/mods/hosting/scan-service-mac.py'),'--system-worker']
    assert watch['ProgramArguments'][-1]=='--watchdog' and '--config' not in watch['ProgramArguments']
    assert worker['AbandonProcessGroup'] is False and watch['AbandonProcessGroup'] is False
    assert worker['StartInterval']==60 and watch['KeepAlive'] and watch['ThrottleInterval']==5
    assert set(watch['EnvironmentVariables'])=={'PATH','HOME','TMPDIR'}


@pytest.fixture
def heartbeat(tmp_path,monkeypatch):
    if os.name!='posix':pytest.skip('Actual POSIX fd, permissions, links and FIFO')
    monkeypatch.setattr(monitor,'ROOT',tmp_path)
    monkeypatch.setattr(monitor,'ROOT_UID',os.geteuid())
    directory=tmp_path/'monitor';directory.mkdir(mode=0o755)
    # File-layer test seam: production ancestor policy is tested separately.
    monkeypatch.setattr(monitor,'root_directory',lambda p:os.open(p,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW))
    (directory/'monitor.json').write_text(json.dumps(dict(schema=1,producer='tsfp-system-scan-monitor-v1',
                                                       uid=600,service=monitor.label())))
    monitor.write_health(600,dict(status='idle'))
    return directory/'scan.json'


def test_heartbeat_atomic_readable_and_error_stale_identity_fail_closed(heartbeat):
    assert heartbeat.stat().st_mode&0o777==0o644
    monitor.healthy(600)
    original=heartbeat.read_bytes()
    for change in [dict(schema=True),dict(uid=501),dict(uid=True),dict(service='other'),dict(policy='old'),dict(status='terminated'),
                   dict(time=True),dict(time=int(time.time())-60),dict(time=int(time.time())+60)]:
        heartbeat.write_bytes(original)
        monitor.write_health(600,dict(status='idle'))
        value=json.loads(heartbeat.read_bytes());value.update(change);heartbeat.write_text(json.dumps(value))
        with pytest.raises(ModError):monitor.healthy(600)
    monitor.write_health(600,dict(status='error'))
    with pytest.raises(ModError):monitor.healthy(600)
    assert sorted(p.name for p in heartbeat.parent.iterdir())==['monitor.json','scan.json']


def test_writer_preserves_unknown_root_state(heartbeat):
    heartbeat.write_text('{"personal":"preserve"}')
    with pytest.raises(ModError,match='Preserving'):monitor.write_health(600,dict(status='idle'))
    assert heartbeat.read_text()=='{"personal":"preserve"}'


@pytest.mark.parametrize('damage',['symlink','hardlink','writable','fifo','oversized','duplicate'])
def test_untrusted_heartbeat_cannot_admit_worker(heartbeat,tmp_path,damage):
    if damage=='symlink':
        other=tmp_path/'other';heartbeat.rename(other);heartbeat.symlink_to(other)
    elif damage=='hardlink':os.link(heartbeat,tmp_path/'other')
    elif damage=='writable':heartbeat.chmod(0o666)
    elif damage=='fifo':heartbeat.unlink();os.mkfifo(heartbeat)
    elif damage=='oversized':heartbeat.write_bytes(b'x'*8193)
    else:heartbeat.write_text('{"schema":1,"schema":1}')
    with pytest.raises((ModError,OSError)):monitor.healthy(600)


@pytest.mark.skipif(os.name!='posix',reason='Actual nofollow root-directory traversal')
def test_real_unprivileged_or_shared_ancestors_refused(tmp_path):
    if os.geteuid()==0:pytest.skip('Requires ordinary account')
    with pytest.raises((ModError,OSError)):monitor.root_directory(tmp_path)


def test_cli_install_gate_refuses_non_mac_or_non_root(monkeypatch):
    monkeypatch.setattr(monitor.sys,'platform','win32')
    with pytest.raises(ModError,match='administrator'):monitor.installed()


def test_cli_rejects_user_supplied_service_target_before_any_privileged_action(monkeypatch):
    monkeypatch.setattr(sys,'argv',['monitor','--watchdog','--target','system/other.service'])
    with pytest.raises(SystemExit) as failure:monitor.main()
    assert failure.value.code==2


def test_root_invocation_from_uninstalled_source_refused(monkeypatch):
    monkeypatch.setattr(monitor.sys,'platform','darwin')
    monkeypatch.setattr(monitor.os,'geteuid',lambda:0,raising=False)
    with pytest.raises(ModError,match='immutable installed'):monitor.installed()
