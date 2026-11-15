"""Bounded native monitor permission proof using inert temporary system jobs.

Run --apply only from the installed immutable release with its isolated Python.
No production job/configuration, network listener or uploaded content is used.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import signal
import subprocess
import sys
import time
import uuid


def load(name, path):
    spec = importlib.util.spec_from_file_location(name,path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


HERE = Path(__file__).absolute().parent
monitor = load('proof_monitor',HERE/'system-scan-monitor-mac.py')
identity = load('proof_identity',HERE/'provision-service-mac.py')

WORKER = r'''import importlib.util,json,os,subprocess,sys,time,uuid
from pathlib import Path
role,state,code,fixture=sys.argv[1],Path(sys.argv[2]),Path(sys.argv[3]),Path(sys.argv[4])
assert role in ('target','reader')
spec=importlib.util.spec_from_file_location('proof_monitor',code)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.ROOT=fixture
from mod_host_identity import kernel_groups
value=dict(uid=os.getuid(),euid=os.geteuid(),gid=os.getgid(),egid=os.getegid(),groups=os.getgroups(),kernel_groups=kernel_groups(),pid=os.getpid(),pgid=os.getpgrp())
def write():
 name='.ready-'+uuid.uuid4().hex
 with os.fdopen(os.open(state/name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600),'w') as out:
  json.dump(value,out);out.flush();os.fsync(out.fileno())
 os.replace(state/name,state/'target.json')
value['identity_ok']=value['uid']==value['euid']==value['gid']==value['egid']>=600 and set(value['kernel_groups'])<={value['uid']} and not {0,80}.intersection(value['groups'])
if not value['identity_ok']:
 value['phase']='identity-refused'
 if role=='target':write()
 sys.exit(2)
try:os.seteuid(0);value['root_restore_denied']=False
except PermissionError:value['root_restore_denied']=True
if not value['root_restore_denied']:
 value['phase']='root-restoration-permitted'
 if role=='target':write()
 sys.exit(2)
deadline=time.monotonic()+5
while True:
 try:m.healthy(value['uid']);value['healthy']=True;break
 except (OSError,ValueError):value['healthy']=False
 if role=='reader' or time.monotonic()>=deadline:break
 time.sleep(0.05)
if role=='reader':print(json.dumps(value));sys.exit(0)
if not value['healthy']:value['phase']='heartbeat-refused';write();sys.exit(2)
try:
 with (fixture/'monitor/scan.json').open('wb') as out:out.write(b'forged')
 value['heartbeat_write_denied']=False
except PermissionError:value['heartbeat_write_denied']=True
if not value['heartbeat_write_denied']:value['phase']='heartbeat-write-permitted';write();sys.exit(2)
child=subprocess.Popen([sys.executable,'-I','-S','-B','-c','import time; time.sleep(90)'],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
value['child_pid']=child.pid
value['phase']='ready';write()
time.sleep(90)
'''


def fixture_path(nonce):
    identity.require(isinstance(nonce,str) and re.fullmatch('[0-9a-f]{32}',nonce), 'Invalid monitor proof nonce')
    return identity.ROOT/'releases'/('system-monitor-probe-'+nonce)


def launchctl(*args):
    result = subprocess.run(['/bin/launchctl',*args],stdin=subprocess.DEVNULL,capture_output=True,timeout=10)
    identity.require(len(result.stdout)+len(result.stderr)<=1024**2,'Monitor proof command exceeded budget')
    return result


def job(role, release, fixture, python):
    identity.require(role in ('target','controller') and fixture == monitor.ROOT, 'Invalid monitor proof job')
    identity.require(release.parent==identity.ROOT/'releases' and re.fullmatch('[0-9a-f]{64}',release.name)
                     and python.is_relative_to(release/'runtime') and '..' not in python.parts,'Invalid installed proof executable')
    nonce = fixture.name.removeprefix('system-monitor-probe-')
    identity.require(fixture == fixture_path(nonce),'Invalid monitor proof scope')
    target = monitor.label()
    code = release/'source/tools/mods/hosting'
    args = ([str(code/'locked-service-bootstrap-mac.py'),'--role','proof-target','--nonce',nonce]
            if role=='target' else [str(code/'probe-system-scan-monitor-mac.py'),'--controller',nonce])
    return dict(Label=target+('.proof-monitor' if role=='controller' else ''),
                UserName='root',GroupName='wheel',InitGroups=False,
                ProgramArguments=[str(python),'-I','-S','-B',*args],RunAtLoad=True,KeepAlive=False,
                AbandonProcessGroup=False,ProcessType='Background',ExitTimeOut=5,Umask=0o077,
                EnvironmentVariables=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(fixture),TMPDIR='/private/tmp'),
                HardResourceLimits=dict(Core=0,CPU=30,FileSize=8192,NumberOfFiles=64),
                StandardOutPath='/dev/null',StandardErrorPath=str(fixture/(role+'-stderr.txt')))


def ready(path, uid):
    fd = os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    with os.fdopen(fd,'rb') as stream:
        import stat
        before = os.fstat(stream.fileno())
        identity.require(stat.S_ISREG(before.st_mode) and before.st_uid==uid and before.st_nlink==1
                         and before.st_mode&0o777==0o600 and 0<before.st_size<=4096,'Unsafe monitor proof result')
        wire = stream.read(4097)
        after = os.fstat(stream.fileno())
        identity.require(len(wire)==before.st_size and (before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)
                         ==(after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),'Monitor proof result changed')
    value = monitor.read_json(wire)
    identity.require(isinstance(value,dict),'Invalid monitor proof result')
    return value


def await_ready(path, uid):
    deadline=time.monotonic()+15
    while True:
        try:return ready(path,uid)
        except FileNotFoundError:
            identity.require(time.monotonic()<deadline,'Native monitor proof timed out waiting for job')
            time.sleep(0.1)


def cleanup(attempted):
    for label in reversed(attempted):
        try:launchctl('bootout','system/'+label)
        except (OSError,subprocess.SubprocessError):pass
    remaining=[]
    for label in attempted:
        try:
            if launchctl('print','system/'+label).returncode==0:remaining.append(label)
        except (OSError,subprocess.SubprocessError):remaining.append(label)
    identity.require(not remaining,'Owned system proof jobs not confirmed unloaded: '+','.join(remaining))


def diagnostics(fixture):
    import stat
    messages=[]
    for role in ('target','controller'):
        try:
            fd=os.open(fixture/(role+'-stderr.txt'),os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
            with os.fdopen(fd,'rb') as stream:
                info=os.fstat(stream.fileno())
                if stat.S_ISREG(info.st_mode) and info.st_uid==0 and info.st_nlink==1 and not info.st_mode&0o022:
                    text=stream.read(4096).decode('utf-8','replace').strip()
                    if text:messages.append(role+': '+text)
        except OSError:pass
    return '; '.join(messages)


def controller(nonce):
    release,account=monitor.installed()
    fixture=fixture_path(nonce)
    marker=monitor.root_json(fixture,'probe.json')
    monitor.ROOT=fixture
    identity.require(marker.get('producer')=='tsfp-system-monitor-proof-v1' and marker.get('uid')==account['uid']
                     and marker.get('release')==str(release) and marker.get('service')==monitor.label(),'Unknown monitor proof')
    state={};saw_starting=False;deadline=time.monotonic()+10
    while True:
        state,watching=monitor.watch_once(account['uid'],state)
        identity.require(watching['status'] in ('starting','watching'),'Native controller lost the expected worker phase')
        saw_starting=saw_starting or watching['status']=='starting'
        monitor.write_health(account['uid'],watching)
        if watching['status']=='watching':
            try:
                target=ready(fixture/'state/target.json',account['uid'])
                identity.require(target.get('phase')=='ready' and target.get('pid')==state['pid'],
                                 'Restricted target did not finish readiness')
                break
            except FileNotFoundError:pass
        identity.require(time.monotonic()<deadline,'Native controller startup/readiness deadline exceeded')
        time.sleep(0.05)
    identity.require(saw_starting,'Native controller did not observe the trusted root bootstrap phase')
    # Only the trusted inert proof controls its clock; no OS or production clock changes.
    _,stopped=monitor.watch_once(account['uid'],dict(pid=state['pid'],start=0,restricted=True),clock=lambda:481)
    identity.require(stopped.get('status')=='terminated' and stopped.get('reason')=='wall-time',
                     'Native controller could not stop its fixed system job')
    monitor.write_health(account['uid'],stopped)
    result=dict(schema=1,phase='controller-complete',uid=os.getuid(),euid=os.geteuid(),pid=os.getpid(),
                target_pid=state['pid'],saw_root_startup=saw_starting,watching=watching,stopped=stopped)
    identity.write_new(fixture/'.controller.json',result)
    os.replace(fixture/'.controller.json',fixture/'controller.json')


def reader(fixture, python, uid):
    result=subprocess.run([str(python),'-I','-S','-B',str(fixture/'worker.py'),'reader',str(fixture/'state'),
                           str(HERE/'system-scan-monitor-mac.py'),str(fixture)],
                          user=uid,group=uid,extra_groups=[],stdin=subprocess.DEVNULL,capture_output=True,timeout=10)
    identity.require(result.returncode==0 and len(result.stdout)<=4096 and len(result.stderr)<=4096,
                     'Restricted heartbeat reader failed')
    return monitor.read_json(result.stdout)


def apply():
    release,account=monitor.installed()
    identity.require(Path(__file__).absolute()==release/'source/tools/mods/hosting/probe-system-scan-monitor-mac.py',
                     'Run the immutable installed monitor proof')
    original=monitor.ROOT
    fixture=fixture_path(uuid.uuid4().hex)
    fixture.mkdir(mode=0o700);fixture.chmod(0o755)
    (fixture/'monitor').mkdir(mode=0o700);(fixture/'monitor').chmod(0o755)
    (fixture/'state').mkdir(mode=0o700);os.chown(fixture/'state',account['uid'],account['gid'])
    with (fixture/'worker.py').open('x') as output:output.write(WORKER)
    (fixture/'worker.py').chmod(0o644)
    monitor.ROOT=fixture
    marker=dict(schema=1,producer='tsfp-system-monitor-proof-v1',uid=account['uid'],service=monitor.label(),release=str(release))
    identity.write_new(fixture/'probe.json',marker)
    identity.write_new(fixture/'monitor/monitor.json',dict(schema=1,producer='tsfp-system-scan-monitor-v1',
                                                         uid=account['uid'],service=monitor.label()))
    monitor.write_health(account['uid'],dict(status='idle'))
    python=Path(sys.executable).resolve()
    attempted=[]
    try:
        for role in ('target','controller'):
            value=job(role,release,fixture,python);label=value['Label']
            identity.require(launchctl('print','system/'+label).returncode!=0,'Preserving existing system proof job')
            path=fixture/(role+'.plist')
            with path.open('xb') as output:plistlib.dump(value,output)
            path.chmod(0o600)
            attempted.append(label)
            identity.require(launchctl('bootstrap','system',str(path)).returncode==0,'System proof job bootstrap failed')
            if role=='target':
                bootstrap=await_ready(fixture/'bootstrap.json',0)
                identity.require(bootstrap.get('uid')==bootstrap.get('euid')==0 and type(bootstrap.get('pid')) is int,
                                 'Trusted bootstrap identity changed')
            else:
                controlled=await_ready(fixture/'controller.json',0)
                target=await_ready(fixture/'state/target.json',account['uid'])
                identity.require(target.get('phase')=='ready' and target.get('uid')==target.get('euid')==target.get('gid')==target.get('egid')==account['uid']
                                 and target.get('pid')==target.get('pgid') and set(target.get('kernel_groups',[]))<={account['uid']}
                                 and not {0,80}.intersection(target.get('groups',[]))
                                 and target.get('healthy') is True and target.get('heartbeat_write_denied') is True
                                 and target.get('root_restore_denied') is True,
                                 'Native locked job identity or root heartbeat enforcement failed: '+json.dumps(target)[:500])
                identity.require(type(target.get('child_pid')) is int and target['child_pid']>1
                                 and type(target.get('pid')) is int and target['pid']>1,'Invalid native proof process identity')
                identity.require(controlled.get('uid')==controlled.get('euid')==0
                                 and controlled.get('target_pid')==target['pid']==bootstrap['pid']
                                 and controlled.get('saw_root_startup') is True,'Native root controller identity changed')
        deadline=time.monotonic()+10
        while True:
            printed=launchctl('print','system/'+monitor.label())
            child=subprocess.run(['/bin/ps','-p',str(target['child_pid']),'-o','pid='],capture_output=True,timeout=3)
            if printed.returncode==0 and monitor.managed_pid(printed.stdout.decode()) is None and not child.stdout.strip():break
            identity.require(time.monotonic()<deadline,'System job/group did not stop before deadline')
            time.sleep(0.1)
        blocked=reader(fixture,python,account['uid'])
        identity.require(blocked.get('uid')==account['uid'] and blocked.get('healthy') is False,
                         'Restricted worker admitted a terminated heartbeat')
        value=dict(schema=1,phase='probe-complete',release=str(release),fixture=str(fixture),target=target,
                   controller=controlled,root_heartbeat_write_denied=True,terminated_heartbeat_rejected=True,
                   target_and_child_exited=True,controlled_test_clock_only=True,network_started=False,
                   production_services_deployed=False)
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        detail=diagnostics(fixture)
        raise identity.ProvisionError(str(error)+('; '+detail if detail else '')) from error
    finally:
        try:
            cleanup(attempted)
        finally:monitor.ROOT=original
    value['all_temporary_jobs_unloaded']=True
    identity.write_new(fixture/'acceptance.json',value)
    return value


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--apply',action='store_true')
    mode.add_argument('--controller')
    args=parser.parse_args()
    try:
        if args.controller:
            controller(args.controller)
        elif args.apply:
            identity.require(sys.platform=='darwin' and os.geteuid()==0,'Use local Mac administrator authentication')
            def interrupted(*_):raise KeyboardInterrupt()
            for sig in (signal.SIGTERM,signal.SIGINT,signal.SIGHUP,signal.SIGALRM):signal.signal(sig,interrupted)
            signal.alarm(120)
            try:print(json.dumps(apply(),indent=2))
            finally:signal.alarm(0)
        else:
            print(json.dumps(dict(schema=1,phase='plan-only',temporary_jobs=2,network_started=False,
                                  production_services_deployed=False)))
        return 0
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print('System monitor proof: '+str(error),file=sys.stderr)
        return 2
    except KeyboardInterrupt:return 130


if __name__=='__main__':
    raise SystemExit(main())
