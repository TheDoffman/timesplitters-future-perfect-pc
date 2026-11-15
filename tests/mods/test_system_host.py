"""Fixed system host control, root health admission and bounded untrusted status."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/mods'))
import mod_system_host as host
from mod_package import ModError


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT/'tools/mods/hosting'/name)
    result = importlib.util.module_from_spec(spec); spec.loader.exec_module(result)
    return result


monitor = load('system-host-monitor-mac.py')
worker = load('system-host-service-mac.py')


def commands(rows='456 1 1024 600', printed='pid = 456', kill=0):
    calls=[]
    def run(args):
        calls.append(args)
        if args[0]=='/bin/ps': return 0,rows
        if args[1]=='print': return 0,printed
        return kill,''
    return run,calls


@pytest.mark.parametrize('role',host.ROLES)
def test_host_tree_owner_transition_and_only_fixed_label_control(role,monkeypatch):
    run,calls=commands('456 1 100 0')
    state,report=monitor.watch_once(role,600,{},run,lambda:1)
    assert report['status']=='starting' and not state['restricted']
    run,calls=commands()
    state,report=monitor.watch_once(role,600,state,run,lambda:2)
    assert report['status']=='watching' and state['restricted']
    assert calls[0]==['/bin/launchctl','print','system/'+host.paths(role)['label']]
    observed=[]
    monkeypatch.setattr(monitor,'worker_heartbeat',lambda r,u,p:observed.append((r,u,p)))
    state,report=monitor.watch_once(role,600,state,run,lambda:20)
    assert report['status']=='watching' and observed==[(role,600,456)]
    run,calls=commands('456 1 100 0')
    _,report=monitor.watch_once(role,600,state,run,lambda:21)
    assert report['status']=='error'
    assert calls[-1]==['/bin/launchctl','kill','SIGKILL','system/'+host.paths(role)['label']]


@pytest.mark.parametrize('role',host.ROLES)
@pytest.mark.parametrize('fault',['memory','deadline','heartbeat','owner','mixed-tree','count','missing','ambiguous','denied','root-startup'])
def test_host_faults_never_stop_sampled_pids_or_claim_denied_termination(role,fault,monkeypatch):
    rows='456 1 100 600'
    if fault in ('memory','denied'): rows='456 1 '+str(monitor.MEMORY_BYTES//1024+1)+' 600'
    if fault=='owner': rows='456 1 100 501'
    if fault=='mixed-tree': rows+='\n457 456 100 0'
    if fault=='count': rows+='\n'+'\n'.join(str(i)+' 456 100 600' for i in range(600,608))
    if fault=='root-startup': rows='456 1 100 0'
    run,calls=commands(rows,'pid = 456\npid = 457' if fault=='ambiguous' else 'state = running' if fault=='missing' else 'pid = 456',
                       kill=1 if fault=='denied' else 0)
    monkeypatch.setattr(monitor,'worker_heartbeat',lambda *_: (_ for _ in ()).throw(ModError('stale')))
    _,report=monitor.watch_once(role,600,dict(pid=456,start=0),run,lambda:3601 if fault=='deadline' else 20)
    if fault=='missing': assert report==dict(status='idle')
    else:
        assert report['status']==('terminated' if fault in ('memory','deadline','heartbeat','root-startup') else 'error')
        assert calls[-1]==['/bin/launchctl','kill','SIGKILL','system/'+host.paths(role)['label']]
    assert not any('456' in args for args in calls if args[0]=='/bin/launchctl')


@pytest.mark.parametrize('bad',['shell','scan','definitions','../api','api\n',None])
def test_host_roles_have_no_generic_target_or_command(bad):
    with pytest.raises(ModError):monitor.watch_once(bad,600,{},lambda _:pytest.fail('invalid role reached OS'))


@pytest.mark.parametrize('role',host.ROLES)
def test_plists_have_only_fixed_root_prelude_and_fixed_root_monitor(role):
    release=host.ROOT/'releases'/('a'*64); python=release/'runtime/bin/python3.12'
    service,controller=monitor.plists(role,release,python)
    assert service['ProgramArguments']==[str(python),'-I','-S','-B',str(release/'source/tools/mods/hosting/locked-service-bootstrap-mac.py'),'--role',role]
    assert controller['ProgramArguments'][-3:]==['--role',role,'--watchdog']
    for value in (service,controller):
        assert value['UserName']=='root' and value['GroupName']=='wheel' and value['InitGroups'] is False
        assert value['AbandonProcessGroup'] is False and value['KeepAlive'] is True
        assert '--config' not in value['ProgramArguments'] and '--command' not in value['ProgramArguments']


@pytest.fixture
def layout(tmp_path,monkeypatch):
    if os.name!='posix':pytest.skip('Actual POSIX nofollow/owner/mode/FIFO protections')
    tmp_path.chmod(0o700)
    monkeypatch.setattr(host,'ROOT',tmp_path)
    monkeypatch.setattr(worker,'ROOT',tmp_path)
    monkeypatch.setattr(monitor,'ROOT',tmp_path)
    # Tests run without administrator access. Keep the root owner check, substituting
    # only its UID and ancestor root; native protected jobs are a separate proof.
    monkeypatch.setattr(monitor.scan,'ROOT_UID',os.geteuid())
    monkeypatch.setattr(monitor.scan,'root_directory',lambda p:os.open(p,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW))
    for role in host.ROLES:
        expected=host.paths(role)
        expected['state'].mkdir(mode=0o700,parents=True)
        expected['config'].parent.chmod(0o700)
        expected['monitor'].mkdir(mode=0o755,parents=True)
        marker=dict(schema=1,producer=host.POLICY,uid=600,role=role,service=expected['label'],release='/release')
        (expected['monitor']/'monitor.json').write_text(json.dumps(marker))
        monitor.write_health(role,600,dict(status='watching',pid=456,rss_bytes=1024))
    return tmp_path


def test_root_heartbeats_are_role_release_pid_uid_policy_bound_and_readonly(layout):
    monitor.healthy('api',600,456,'/release')
    path=host.paths('api')['monitor']/'health.json'
    assert path.stat().st_mode&0o777==0o644
    original=path.read_bytes()
    for field,value in [('schema',True),('role','gateway'),('release','/other'),('pid',457),('uid',True),
                        ('policy','old'),('status','idle'),('status','starting'),('status','terminated'),
                        ('time',True),('time',int(time.time())-11),('time',int(time.time())+1)]:
        record=json.loads(original); record[field]=value; path.write_text(json.dumps(record))
        with pytest.raises(ModError):monitor.healthy('api',600,456,'/release')
    path.write_bytes(original)
    with pytest.raises(ModError):monitor.healthy('gateway',600,456,'/other')
    path.write_text('{"personal":"preserve"}')
    with pytest.raises(ModError):monitor.write_health('api',600,dict(status='idle'))
    assert path.read_text()=='{"personal":"preserve"}'


@pytest.mark.parametrize('damage',['stale','pid','component','service','status','mode','symlink','hardlink','fifo','oversized','duplicate'])
def test_worker_heartbeat_is_only_bounded_inert_data(layout,damage):
    role='api'; path=host.paths(role)['state']/'api.json'; uid=os.geteuid()
    value=dict(schema=1,pid=456,time=int(time.time()),service=host.paths(role)['label'],component=role,status='serving')
    path.write_text(json.dumps(value));path.chmod(0o600)
    monitor.worker_heartbeat(role,uid,456)
    if damage=='stale':value['time']-=16
    if damage=='pid':value['pid']=457
    if damage=='component':value['component']='gateway'
    if damage=='service':value['service']='other'
    if damage=='status':value['status']='error'
    path.write_text(json.dumps(value))
    if damage=='mode':path.chmod(0o644)
    if damage=='symlink':
        target=path.with_name('original');path.rename(target);path.symlink_to(target)
    if damage=='hardlink':os.link(path,path.with_name('alias'))
    if damage=='fifo':path.unlink();os.mkfifo(path,0o600)
    if damage=='oversized':path.write_bytes(b'x'*8193)
    if damage=='duplicate':path.write_text('{"schema":1,"schema":1}')
    with pytest.raises((OSError,ModError)):monitor.worker_heartbeat(role,uid,456)


def test_system_adapter_ignores_worker_writable_gui_watchdog(layout,monkeypatch):
    expected=host.paths('api')
    data=dict(expected,origin='https://workshop.example.test',system_monitor=True,system_release='/release')
    adapter=host.supervisor(data,'api','origin')
    calls=[]
    monkeypatch.setattr(host,'monitor',lambda:SimpleNamespace(healthy=lambda *args:calls.append(args)))
    adapter.healthy()
    assert calls==[('api',os.geteuid(),os.getpid(),'/release')]
    with pytest.raises(ValueError):adapter.target()
    with pytest.raises(ValueError):adapter.generate('anything','anywhere')
    adapter.status(dict(status='serving'))
    monitor.worker_heartbeat('api',os.geteuid(),os.getpid())


@pytest.mark.parametrize('mode',['bad-exit','wrong-role','wrong-release','side-effects','oversized'])
def test_generator_private_checks_drop_privileges_and_refuse_bad_results(monkeypatch,mode):
    release=host.ROOT/'releases'/('a'*64)
    def run(args,**options):
        assert args[-3:]==['--role','gateway','--check']
        assert options['user']==options['group']==600 and options['extra_groups']==[]
        value=dict(schema=1,role='api' if mode=='wrong-role' else 'gateway',release='/other' if mode=='wrong-release' else str(release),
                   phase='ready',services_started=mode=='side-effects',network_started=False)
        return SimpleNamespace(returncode=2 if mode=='bad-exit' else 0,stderr=b'',
                               stdout=b'x'*8193 if mode=='oversized' else json.dumps(value).encode())
    monkeypatch.setattr(monitor.subprocess,'run',run)
    with pytest.raises(ModError):monitor.generate('gateway',release,dict(uid=600,gid=600))


def test_api_controller_withholds_process_creation_without_root_health(layout,monkeypatch):
    data=dict(host.paths('api'),origin='https://workshop.example.test',system_monitor=True,system_release='/release')
    monkeypatch.setattr(worker,'wait_healthy',lambda *_:(_ for _ in ()).throw(ModError('no root health')))
    monkeypatch.setattr(worker.subprocess,'Popen',lambda *_a,**_k:pytest.fail('unsupervised API started'))
    import resource
    monkeypatch.setattr(resource,'setrlimit',lambda *_:None)
    with pytest.raises(ModError):worker.serve_api(data)


def test_api_config_requires_exact_fixed_repository_state_and_private_fields(layout):
    repository=layout/'data/scan/repository';repository.mkdir(parents=True,mode=0o700)
    expected=host.paths('api')
    value=dict(schema=1,origin='https://workshop.example.test:8443',repository=str(repository),state=str(expected['state']))
    expected['config'].write_text(json.dumps(value));expected['config'].chmod(0o600)
    assert worker.api_config()['origin']==value['origin']
    for change in [dict(repository='/other'),dict(state='/other'),dict(schema=True),dict(origin='http://bad.test'),dict(command='shell')]:
        expected['config'].write_text(json.dumps(dict(value,**change)))
        with pytest.raises(ModError):worker.api_config()


@pytest.mark.parametrize('role',host.ROLES)
def test_fixed_host_configuration_checks_actual_catalog_key_and_shared_origin(layout,trust_keys,role):
    from cryptography.hazmat.primitives import serialization
    from mod_trust import make_root
    repository=layout/'data/scan/repository';repository.mkdir(parents=True,mode=0o700)
    base=layout/'data/host';release=layout/'releases'/('a'*64)
    origin='https://workshop.example.test:8443'
    def write(path,value):
        path.write_text(json.dumps(value));path.chmod(0o600)
    write(host.paths('api')['config'],dict(schema=1,origin=origin,repository=str(repository),state=str(host.paths('api')['state'])))
    write(host.paths('gateway')['config'],dict(schema=1,origin=origin,caddy=str(release/'runtime/bin/caddy'),state=str(host.paths('gateway')['state'])))
    pinned=base/'trusted-root.json'
    pinned.write_bytes(make_root(trust_keys[:2],[trust_keys[4]],1,origin,int(time.time())-60))
    private=base/'catalog-1.pem'
    private.write_bytes(trust_keys[4].private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
    private.chmod(0o600)
    value=dict(schema=1,repository=str(repository),state=str(host.paths('publication')['state']),
               caddy=str(release/'runtime/bin/caddy'),trusted_root=str(pinned),catalog_keys=[str(private)],root_updates=[])
    write(host.paths('publication')['config'],value)
    data=worker.config(role,release)
    assert data['system_monitor'] is True and data['system_release']==str(release)
    if role=='publication':
        for change in [dict(caddy='/other'),dict(trusted_root='/other'),dict(catalog_keys=[str(base/'root-key.pem')]),dict(repository=str(base))]:
            write(host.paths(role)['config'],dict(value,**change))
            with pytest.raises(ModError):worker.config(role,release)
    elif role=='gateway':
        value=json.loads(host.paths(role)['config'].read_bytes());value['origin']='https://other.example.test'
        write(host.paths(role)['config'],value)
        with pytest.raises(ModError):worker.config(role,release)


def test_worker_does_not_read_config_before_installed_credential_checks(monkeypatch):
    monkeypatch.setattr(worker.sys,'argv',['worker','--role','api','--check'])
    monkeypatch.setattr(worker,'installed_release',lambda:(_ for _ in ()).throw(ModError('not restricted')))
    monkeypatch.setattr(worker,'config',lambda *_:pytest.fail('untrusted config read before identity gate'))
    assert worker.main()==2


@pytest.mark.parametrize('program',[worker,monitor])
def test_fixed_host_clis_reject_arbitrary_config_commands_and_targets(program,monkeypatch):
    monkeypatch.setattr(program.sys,'argv',['host','--role','api','--command','shell'])
    with pytest.raises(SystemExit) as error:program.main()
    assert error.value.code==2
