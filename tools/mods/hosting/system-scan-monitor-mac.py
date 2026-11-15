"""Root-only fixed-job monitor; never loads quarantine/configuration from the worker.

The system worker stays unprivileged. Only this monitor may use launchctl kill in
the system domain. Root-owned heartbeat files cannot be forged by that worker.
Generation writes new private plists; it does not bootstrap any service.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import stat
import subprocess
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_host_watch import managed_pid, run_command, sample
from mod_package import ModError, read_json, require

ROOT = Path('/Library/Application Support/TSFP Mod Workshop')
ROOT_UID = 0
POLICY = 'root-system-label-v1'
MEMORY_BYTES, WALL_SECONDS, MAX_PROCESSES = 2 * 1024**3, 480, 8


def config_path():
    return ROOT / 'data/scan/service.json'


def label():
    return 'org.timesplitters.workshop.scan.' + hashlib.sha256(str(config_path()).encode()).hexdigest()[:24]


def root_directory(path):
    path = Path(os.path.abspath(path))
    fd = os.open(path.anchor, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
            info = os.fstat(fd)
            require(stat.S_ISDIR(info.st_mode) and info.st_uid in (0, ROOT_UID)
                    and not info.st_mode & 0o022, 'Unsafe monitor ancestor')
        require(os.fstat(fd).st_uid == ROOT_UID, 'Monitor directory must be root-owned')
        return fd
    except BaseException:
        os.close(fd)
        raise


def root_json(directory, name):
    fd = root_directory(directory)
    try:
        child = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
    finally:
        os.close(fd)
    with os.fdopen(child, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid == ROOT_UID and before.st_nlink == 1
                and not before.st_mode & 0o022 and 0 < before.st_size <= 8192, 'Unsafe monitor record')
        wire = stream.read(8193)
        after = os.fstat(stream.fileno())
        require(len(wire) == before.st_size and (before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns)
                == (after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns), 'Monitor record changed')
    value = read_json(wire)
    require(isinstance(value, dict), 'Invalid monitor record')
    return value


def healthy(uid, clock=time.time):
    value = root_json(ROOT / 'monitor', 'scan.json')
    stamp = value.get('time')
    require(type(value.get('schema')) is int and value['schema'] == 1
            and type(value.get('uid')) is int and value['uid'] == uid
            and value.get('service') == label() and value.get('policy') == POLICY
            and type(stamp) is int and 0 <= clock()-stamp <= 10
            and value.get('status') in ('watching','idle'), 'System scan monitor is not healthy')


def write_health(uid, report):
    require(os.geteuid() == ROOT_UID, 'Monitor heartbeat requires root')
    marker = root_json(ROOT/'monitor','monitor.json')
    require(marker.get('producer') == 'tsfp-system-scan-monitor-v1' and marker.get('schema') == 1
            and type(marker.get('uid')) is int and marker['uid'] == uid
            and marker.get('service') == label(), 'Preserving unknown monitor state')
    try:
        old = root_json(ROOT/'monitor','scan.json')
        require(old.get('schema') == 1 and old.get('service') == label()
                and old.get('policy') == POLICY and old.get('uid') == uid, 'Preserving unknown heartbeat')
    except FileNotFoundError:
        pass
    wire = json.dumps(dict(schema=1, uid=uid, service=label(), policy=POLICY,
                           time=int(time.time()), **report), separators=(',',':')).encode()
    require(len(wire) <= 8192, 'Monitor status exceeds budget')
    fd = root_directory(ROOT / 'monitor')
    name = '.scan-' + uuid.uuid4().hex
    try:
        child = os.open(name, os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW | os.O_WRONLY, 0o600, dir_fd=fd)
        with os.fdopen(child, 'wb') as output:
            output.write(wire)
            output.flush()
            os.fchmod(output.fileno(), 0o644)
            os.fsync(output.fileno())
        os.replace(name, 'scan.json', src_dir_fd=fd, dst_dir_fd=fd)
        os.fsync(fd)
    finally:
        try:
            os.unlink(name, dir_fd=fd)
        except FileNotFoundError:
            pass
        os.close(fd)


def stop(command):
    code, _ = command(['/bin/launchctl','kill','SIGKILL','system/'+label()])
    require(code == 0, 'Cannot stop the fixed system scan service')


def watch_once(uid, observed, command=run_command, clock=time.monotonic):
    require(type(uid) is int and uid >= 600, 'Invalid restricted service identity')
    try:
        code, output = command(['/bin/launchctl','print','system/'+label()])
        require(code == 0, 'System scan service is not loaded')
        pid = managed_pid(output)
        if pid is None:
            return {}, dict(status='idle')
        now = clock()
        start = observed['start'] if observed.get('pid') == pid else now
        memory = sample(pid, command, uid, MAX_PROCESSES)
        if memory is None:
            return {}, dict(status='idle')
        reason = 'memory' if memory > MEMORY_BYTES else 'wall-time' if now-start > WALL_SECONDS else None
        if reason:
            stop(command)
            return {}, dict(status='terminated', reason=reason, rss_bytes=memory)
        return dict(pid=pid,start=start), dict(status='watching',pid=pid,rss_bytes=memory)
    except (OSError, ValueError, ModError, subprocess.SubprocessError):
        # No healthy heartbeat after losing observation or stop permission.
        # Address the launchd label, never a sampled/reusable PID.
        try:
            stop(command)
        except (OSError, ValueError, ModError, subprocess.SubprocessError):
            pass
        return {}, dict(status='error',reason='System scan supervision failed')


def installed():
    require(sys.platform == 'darwin' and os.geteuid() == 0, 'Run through local Mac administrator authentication')
    script = Path(__file__).absolute()
    release = script.parents[4]
    require(script == release/'source/tools/mods/hosting/system-scan-monitor-mac.py'
            and release.parent == ROOT/'releases' and re.fullmatch('[0-9a-f]{64}',release.name),
            'Run the immutable installed monitor')
    fd = root_directory(script.parent)
    os.close(fd)
    info = script.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and info.st_nlink == 1
            and not info.st_mode & 0o022, 'Unsafe monitor program')
    marker = root_json(release,'installation.json')
    require(marker.get('producer') == 'tsfp-installed-host-v1' and marker.get('phase') == 'complete'
            and marker.get('descriptor_sha256') == release.name and marker.get('release') == str(release),
            'Incomplete installed monitor release')
    spec = importlib.util.spec_from_file_location('monitor_identity',script.with_name('provision-service-mac.py'))
    identity = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(identity)
    account = identity.existing()
    require(marker.get('uid') == account['uid'], 'Installed monitor identity changed')
    executable = Path(sys.executable).resolve()
    require(executable.is_relative_to(release/'runtime'), 'Use the installed isolated Python')
    fd = root_directory(executable.parent)
    os.close(fd)
    info = executable.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_uid == 0 and info.st_nlink == 1
            and not info.st_mode & 0o022 and info.st_mode & 0o111, 'Unsafe installed monitor Python')
    return release, account


def plists(release, account, python):
    require(type(account.get('uid')) is int and account['uid'] >= 600
            and account.get('gid') == account['uid'], 'Invalid system worker account')
    worker = release/'source/tools/mods/hosting/scan-service-mac.py'
    monitor = release/'source/tools/mods/hosting/system-scan-monitor-mac.py'
    common = dict(RunAtLoad=True, ProcessType='Background', AbandonProcessGroup=False,
                  Umask=0o077, StandardOutPath='/dev/null', StandardErrorPath='/dev/null',
                  EnvironmentVariables=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(ROOT/'data'),TMPDIR=str(ROOT/'data')))
    scan = dict(common,Label=label(),UserName='_tsfpworkshop',GroupName='_tsfpworkshop',InitGroups=False,
                ProgramArguments=[str(python),'-I','-B',str(worker),'--system-worker'],
                StartInterval=60,HardResourceLimits=dict(Core=0,CPU=420,FileSize=512*1024**2,NumberOfFiles=256))
    watch = dict(common,Label=label()+'.monitor',UserName='root',GroupName='wheel',InitGroups=False,
                 ProgramArguments=[str(python),'-I','-S','-B',str(monitor),'--watchdog'],
                 KeepAlive=True,ThrottleInterval=5,HardResourceLimits=dict(Core=0,FileSize=8192,NumberOfFiles=64))
    # Root monitor never imports the worker or reads repository/scan configuration.
    watch['EnvironmentVariables'] = dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(ROOT),TMPDIR='/private/tmp')
    return scan, watch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--generate',action='store_true')
    mode.add_argument('--watchdog',action='store_true')
    args = parser.parse_args()
    try:
        release, account = installed()
        if args.generate:
            directory = ROOT/'monitor'
            require(not directory.exists() and not directory.is_symlink(), 'Preserving existing monitor state')
            directory.mkdir(mode=0o700)
            directory.chmod(0o755)
            for name,value in zip(('scan.plist','monitor.plist'),plists(release,account,Path(sys.executable).resolve())):
                with os.fdopen(os.open(directory/name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600),'wb') as output:
                    plistlib.dump(value,output)
            with os.fdopen(os.open(directory/'monitor.json',os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600),'w') as output:
                json.dump(dict(schema=1,producer='tsfp-system-scan-monitor-v1',uid=account['uid'],
                               service=label(),release=str(release)),output)
                output.flush()
                os.fsync(output.fileno())
            print(json.dumps(dict(phase='generated-only',services_started=False,public_endpoint_started=False)))
        else:
            marker = root_json(ROOT/'monitor','monitor.json')
            require(marker.get('release') == str(release) and marker.get('uid') == account['uid']
                    and marker.get('producer') == 'tsfp-system-scan-monitor-v1', 'Monitor deployment binding changed')
            observed, previous, written = {}, None, 0
            while True:
                observed, report = watch_once(account['uid'],observed)
                key = (report['status'],report.get('pid'),report.get('reason'))
                if key != previous or time.monotonic()-written >= 5:
                    write_health(account['uid'],report)
                    previous,written = key,time.monotonic()
                time.sleep(0.5)
        return 0
    except (OSError, ValueError, ModError) as error:
        print('System scan monitor: '+str(error),file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
