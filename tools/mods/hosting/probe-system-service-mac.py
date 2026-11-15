"""Bounded, explicit launchd system-domain permission probe; no network or production jobs."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import signal
import stat
import subprocess
import sys
import time
import uuid

spec = importlib.util.spec_from_file_location('service_identity', Path(__file__).with_name('provision-service-mac.py'))
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)

WORKER = r'''import json,os,re,subprocess,sys,time
from pathlib import Path
state,role,target=Path(sys.argv[1]),sys.argv[2],sys.argv[3]
assert role in ('target','watchdog') and re.fullmatch(r'org\.timesplitters\.workshop\.permissions\.[0-9a-f]{32}\.target',target)
os.umask(0o077)
result=dict(role=role,uid=os.getuid(),gid=os.getgid(),pid=os.getpid())
assert result['uid']!=0
def write(name,value):
 wire=json.dumps(value,sort_keys=True).encode();assert len(wire)<4096
 fd=os.open(str(state/name),os.O_CREAT|os.O_EXCL|os.O_WRONLY|os.O_NOFOLLOW,0o600)
 with os.fdopen(fd,'wb') as out:out.write(wire)
if role=='target':
 write('target.json',result)
 time.sleep(30)
else:
 for operation,args in [('print',['print','system/'+target]),('kill',['kill','SIGKILL','system/'+target])]:
  p=subprocess.run(['/bin/launchctl',*args],stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,timeout=5,start_new_session=True)
  assert len(p.stderr)<65536
  result[operation+'_exit']=p.returncode
  result[operation+'_error']=p.stderr.decode('utf-8','replace')[:400]
 write('watchdog.json',result)
'''


def job(label, script, state, role, target):
    identity.require(re.fullmatch(r'org\.timesplitters\.workshop\.permissions\.[0-9a-f]{32}\.target', target)
                     and role in ('target', 'watchdog') and label == target.rsplit('.', 1)[0] + '.' + role,
                     'Invalid probe job identity')
    return dict(Label=label, UserName=identity.ACCOUNT, GroupName=identity.ACCOUNT,
                ProgramArguments=['/usr/bin/python3', '-I', '-B', str(script), str(state), role, target],
                WorkingDirectory=str(script.parent), RunAtLoad=True, KeepAlive=False,
                ProcessType='Background', Umask=0o077, AbandonProcessGroup=False, ExitTimeOut=5,
                EnvironmentVariables=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin'),
                HardResourceLimits=dict(Core=0, FileSize=65536, NumberOfFiles=64),
                StandardOutPath='/dev/null', StandardErrorPath='/dev/null')


def launchctl(*arguments):
    result = subprocess.run(['/bin/launchctl', *arguments], stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
    identity.require(len(result.stdout) + len(result.stderr) <= 1024**2, 'Probe command output exceeded budget')
    return result


def read_result(directory_fd, name, uid):
    identity.require(name in ('target.json', 'watchdog.json'), 'Unexpected probe result name')
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory_fd)
    with os.fdopen(fd, 'rb') as stream:
        info = os.fstat(stream.fileno())
        identity.require(stat.S_ISREG(info.st_mode) and info.st_uid == uid and info.st_nlink == 1
                         and stat.S_IMODE(info.st_mode) == 0o600 and 0 < info.st_size <= 4096,
                         'Preserving unrecognized probe result')
        wire = stream.read(4097)
        identity.require(len(wire) <= 4096, 'Probe result exceeded budget')
    value = json.loads(wire)
    identity.require(isinstance(value, dict) and value.get('uid') == uid and value.get('gid') == uid
                     and type(value.get('pid')) is int and value['pid'] > 1, 'Invalid probe worker identity')
    return value


def await_result(directory_fd, name, uid):
    deadline = time.monotonic() + 15
    while True:
        try:
            return read_result(directory_fd, name, uid)
        except FileNotFoundError:
            identity.require(time.monotonic() < deadline, 'Probe worker did not report before deadline')
            time.sleep(0.1)


def apply():
    identity.require(sys.platform == 'darwin' and os.geteuid() == 0,
                     'Run --apply through local administrator sudo on the Mac')
    receipt = identity.existing()
    uid = receipt['uid']
    executable = Path('/usr/bin/python3').stat()
    identity.require(executable.st_uid == 0 and not executable.st_mode & 0o022,
                     'Probe requires the root-owned Apple Python interpreter')
    nonce = uuid.uuid4().hex
    release = identity.ROOT / 'releases' / ('system-permission-probe-' + nonce)
    state = identity.ROOT / 'data' / ('system-permission-probe-' + nonce)
    release.mkdir(mode=0o755)
    os.chmod(release, 0o755)
    state.mkdir(mode=0o700)
    directory_fd = os.open(state, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    os.chown(state, uid, uid)
    os.chmod(state, 0o700)
    script = release / 'worker.py'
    with script.open('x') as stream:
        stream.write(WORKER)
    os.chmod(script, 0o644)
    prefix = 'org.timesplitters.workshop.permissions.' + nonce
    target = prefix + '.target'
    attempted = []
    try:
        for role in ('target', 'watchdog'):
            label = prefix + '.' + role
            identity.require(launchctl('print', 'system/' + label).returncode != 0, 'Preserving existing probe label')
            path = release / (role + '.plist')
            with path.open('xb') as stream:
                plistlib.dump(job(label, script, state, role, target), stream)
            os.chmod(path, 0o600)
            attempted.append(label)
            result = launchctl('bootstrap', 'system', str(path))
            identity.require(result.returncode == 0, 'System probe bootstrap failed: ' + result.stderr.decode('utf-8', 'replace')[:400])
            observed = await_result(directory_fd, role + '.json', uid)
            identity.require(observed['role'] == role, 'Probe role mismatch')
            if role == 'target':
                target_worker = observed
            else:
                watchdog_worker = observed
        deadline = time.monotonic() + 3
        while True:
            status = launchctl('print', 'system/' + target)
            identity.require(status.returncode == 0, 'Cannot inspect registered target job')
            target_stopped = not re.search(rb'^\s*pid = [0-9]+\s*$', status.stdout, re.M)
            if target_stopped or time.monotonic() >= deadline:
                break
            time.sleep(0.1)
        value = dict(schema=1, phase='probe-complete', account=identity.ACCOUNT, uid=uid,
                     target_worker=target_worker, watchdog_worker=watchdog_worker,
                     account_can_control_system_job=watchdog_worker['kill_exit'] == 0 and target_stopped,
                     target_stopped_before_root_cleanup=target_stopped,
                     release=str(release), state=str(state), network_started=False, production_services_deployed=False)
    finally:
        try:
            for label in reversed(attempted):
                launchctl('bootout', 'system/' + label)
            for label in attempted:
                identity.require(launchctl('print', 'system/' + label).returncode != 0,
                                 'Owned probe job remains loaded; administrator inspection required')
        finally:
            os.close(directory_fd)
    value['all_temporary_jobs_unloaded'] = True
    identity.write_new(release / 'acceptance.json', value)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    try:
        if args.apply:
            identity.require(sys.platform == 'darwin', 'Run --apply through local administrator sudo on the Mac')
            def interrupt(*_):
                raise KeyboardInterrupt()
            for name in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
                signal.signal(name, interrupt)
            value = apply()
        else:
            value = dict(schema=1, phase='plan-only', account=identity.ACCOUNT, domain='system',
                         temporary_jobs=2, network_started=False, production_services_deployed=False)
        print(json.dumps(value, indent=2))
        return 0
    except KeyboardInterrupt:
        return 130
    except (identity.ProvisionError, OSError, ValueError, subprocess.SubprocessError) as error:
        print('System service probe: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
