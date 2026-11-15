"""Root-only fixed API/publication/gateway job control; no worker config or keys."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import stat
import subprocess
import sys
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_host_watch import managed_pid, run_command, sample_state
from mod_package import ModError, read_json, require
from mod_system_host import paths, POLICY, ROLES, ROOT

spec = importlib.util.spec_from_file_location('host_scan_monitor', Path(__file__).with_name('system-scan-monitor-mac.py'))
scan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan)
MEMORY_BYTES, MAX_PROCESSES = 2*1024**3, 8
STARTUP_SECONDS, HEARTBEAT_SECONDS, WALL_SECONDS = 10, 15, 3600


def healthy(role, uid, pid, release, clock=time.time):
    value = scan.root_json(paths(role)['monitor'], 'health.json')
    stamp = value.get('time')
    require(type(uid) is int and uid >= 600 and type(pid) is int and pid > 1
            and type(value.get('schema')) is int and value['schema'] == 1
            and type(value.get('uid')) is int and value['uid'] == uid
            and value.get('service') == paths(role)['label'] and value.get('role') == role
            and value.get('policy') == POLICY and value.get('release') == str(release)
            and type(value.get('pid')) is int and value['pid'] == pid
            and type(stamp) is int and 0 <= clock()-stamp <= 10
            and value.get('status') == 'watching', 'System host monitor is not healthy')


def worker_heartbeat(role, uid, pid, clock=time.time):
    # Read only this bounded inert status record, through held nofollow FDs.
    # No worker-supplied path, config, command, key or executable is interpreted.
    path = paths(role)['state']
    fd = os.open(path.anchor, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:
        for part in path.parts[1:]:
            child = os.open(part, os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = child
            info = os.fstat(fd)
            require(info.st_uid in (0, uid) and not info.st_mode & 0o022, 'Unsafe host status ancestor')
        info = os.fstat(fd)
        require(info.st_uid == uid and stat.S_IMODE(info.st_mode) == 0o700, 'Unsafe host status directory')
        child = os.open(role+'.json', os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK, dir_fd=fd)
    finally:
        os.close(fd)
    with os.fdopen(child, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid == uid and before.st_nlink == 1
                and stat.S_IMODE(before.st_mode) == 0o600 and 0 < before.st_size <= 8192,
                'Unsafe host controller status')
        wire = stream.read(8193)
        after = os.fstat(stream.fileno())
        fields = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        require(len(wire) == before.st_size and fields(before) == fields(after), 'Host controller status changed')
    value = read_json(wire)
    stamp = value.get('time') if isinstance(value, dict) else None
    require(isinstance(value, dict) and type(value.get('schema')) is int and value['schema'] == 1
            and value.get('service') == paths(role)['label'] and value.get('component') == role
            and type(value.get('pid')) is int and value['pid'] == pid
            and type(stamp) is int and 0 <= clock()-stamp <= HEARTBEAT_SECONDS
            and value.get('status') in ('waiting', 'serving', 'withheld'), 'Host controller heartbeat expired')


def stop(role, command):
    code, _ = command(['/bin/launchctl', 'kill', 'SIGKILL', 'system/'+paths(role)['label']])
    require(code == 0, 'Cannot stop fixed system host service')


def watch_once(role, uid, observed, command=run_command, clock=time.monotonic):
    paths(role)
    require(type(uid) is int and uid >= 600, 'Invalid restricted host identity')
    try:
        code, output = command(['/bin/launchctl', 'print', 'system/'+paths(role)['label']])
        require(code == 0, 'System host service is not loaded')
        pid = managed_pid(output)
        if pid is None: return {}, dict(status='idle')
        now = clock()
        start = observed['start'] if observed.get('pid') == pid else now
        snapshot = sample_state(pid, command, None, MAX_PROCESSES)
        if snapshot is None: return {}, dict(status='idle')
        memory, owner = snapshot
        restricted = observed.get('restricted', False) if observed.get('pid') == pid else False
        if owner == 0:
            require(not restricted, 'Restricted host regained root')
            reason = 'startup-timeout' if now-start > STARTUP_SECONDS else 'memory' if memory > MEMORY_BYTES else None
            report = dict(status='starting', pid=pid, rss_bytes=memory)
        else:
            require(owner == uid, 'System host owner changed')
            reason = 'memory' if memory > MEMORY_BYTES else 'wall-time' if now-start > WALL_SECONDS else None
            if not reason and now-start > HEARTBEAT_SECONDS:
                try: worker_heartbeat(role, uid, pid)
                except (OSError, ValueError, ModError): reason = 'heartbeat'
            report = dict(status='watching', pid=pid, rss_bytes=memory)
        if reason:
            stop(role, command)
            return {}, dict(status='terminated', reason=reason, rss_bytes=memory)
        return dict(pid=pid, start=start, restricted=owner == uid), report
    except (OSError, ValueError, ModError, subprocess.SubprocessError):
        try: stop(role, command)
        except (OSError, ValueError, ModError, subprocess.SubprocessError): pass
        return {}, dict(status='error', reason='System host supervision failed')


def write_health(role, uid, report):
    require(os.geteuid() == scan.ROOT_UID, 'System host heartbeat requires root')
    directory = paths(role)['monitor']
    marker = scan.root_json(directory, 'monitor.json')
    require(marker.get('producer') == POLICY and type(marker.get('schema')) is int and marker['schema'] == 1
            and marker.get('role') == role and marker.get('service') == paths(role)['label']
            and type(marker.get('uid')) is int and marker['uid'] == uid, 'Preserving unknown host monitor')
    try:
        old = scan.root_json(directory, 'health.json')
        require(all(old.get(k) == marker[k] for k in ('service', 'uid', 'role', 'release'))
                and old.get('policy') == POLICY, 'Preserving unknown host heartbeat')
    except FileNotFoundError: pass
    require(set(report) <= {'status', 'pid', 'rss_bytes', 'reason'}, 'Invalid host monitor report')
    wire = json.dumps(dict(schema=1, uid=uid, service=paths(role)['label'], role=role, policy=POLICY,
                           release=marker['release'], time=int(time.time()), **report), separators=(',', ':')).encode()
    require(len(wire) <= 8192, 'Host monitor status exceeds budget')
    fd = scan.root_directory(directory)
    name = '.health-'+uuid.uuid4().hex
    try:
        child = os.open(name, os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW|os.O_WRONLY, 0o600, dir_fd=fd)
        with os.fdopen(child, 'wb') as output:
            output.write(wire); output.flush(); os.fchmod(output.fileno(), 0o644); os.fsync(output.fileno())
        os.replace(name, 'health.json', src_dir_fd=fd, dst_dir_fd=fd); os.fsync(fd)
    finally:
        try: os.unlink(name, dir_fd=fd)
        except FileNotFoundError: pass
        os.close(fd)


def plists(role, release, python):
    label = paths(role)['label']
    common = dict(RunAtLoad=True, KeepAlive=True, ProcessType='Background', AbandonProcessGroup=False,
                  UserName='root', GroupName='wheel', InitGroups=False, Umask=0o077,
                  StandardOutPath='/dev/null', StandardErrorPath='/dev/null',
                  EnvironmentVariables=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin', HOME=str(ROOT), TMPDIR='/private/tmp'))
    hosting = release/'source/tools/mods/hosting'
    worker = dict(common, Label=label, ThrottleInterval=60,
                  ProgramArguments=[str(python), '-I', '-S', '-B', str(hosting/'locked-service-bootstrap-mac.py'), '--role', role],
                  HardResourceLimits=dict(Core=0, FileSize=512*1024**2, NumberOfFiles=256))
    controller = dict(common, Label=label+'.monitor', ThrottleInterval=5,
                      ProgramArguments=[str(python), '-I', '-S', '-B', str(hosting/'system-host-monitor-mac.py'), '--role', role, '--watchdog'],
                      HardResourceLimits=dict(Core=0, FileSize=8192, NumberOfFiles=64))
    return worker, controller


def generate(role, release, account):
    # Configuration, signing and repository checks happen only after dropping UID.
    result = subprocess.run([str(Path(sys.executable).resolve()), '-I', '-B',
                             str(release/'source/tools/mods/hosting/system-host-service-mac.py'), '--role', role, '--check'],
                            user=account['uid'], group=account['gid'], extra_groups=[], stdin=subprocess.DEVNULL,
                            capture_output=True, timeout=30, cwd=release,
                            env=dict(PATH='/usr/bin:/bin', HOME=str(ROOT/'data'), TMPDIR=str(ROOT/'data')))
    require(result.returncode == 0 and len(result.stdout) <= 8192 and len(result.stderr) <= 8192,
            'Complete private host configuration as the locked account first')
    ready = read_json(result.stdout)
    require(isinstance(ready, dict) and ready == dict(schema=1, role=role, release=str(release),
            phase='ready', services_started=False, network_started=False), 'Private host generation differs')
    parent = ROOT/'monitor'
    marker = scan.root_json(parent, 'monitor.json')
    require(marker.get('producer') == 'tsfp-system-scan-monitor-v1' and marker.get('release') == str(release)
            and marker.get('uid') == account['uid'], 'Generate the matching protected scan monitor first')
    fd = scan.root_directory(parent); os.close(fd)
    directory = paths(role)['monitor']
    require(not directory.exists() and not directory.is_symlink(), 'Preserving existing host monitor generation')
    directory.mkdir(mode=0o755)
    for name, value in zip(('worker.plist', 'monitor.plist'), plists(role, release, Path(sys.executable).resolve())):
        with os.fdopen(os.open(directory/name, os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600), 'wb') as output:
            plistlib.dump(value, output); output.flush(); os.fsync(output.fileno())
    with os.fdopen(os.open(directory/'monitor.json', os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600), 'w') as output:
        json.dump(dict(schema=1, producer=POLICY, uid=account['uid'], role=role, service=paths(role)['label'], release=str(release)), output)
        output.flush(); os.fsync(output.fileno())
    fd = scan.root_directory(directory); os.fsync(fd); os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--role', required=True, choices=ROLES)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--generate', action='store_true')
    mode.add_argument('--watchdog', action='store_true')
    args = parser.parse_args()
    try:
        release, account = scan.installed()
        require(Path(__file__).absolute() == release/'source/tools/mods/hosting/system-host-monitor-mac.py',
                'Use the immutable installed host monitor')
        if args.generate:
            generate(args.role, release, account)
            print(json.dumps(dict(phase='generated-only', services_started=False, public_endpoint_started=False)))
        else:
            marker = scan.root_json(paths(args.role)['monitor'], 'monitor.json')
            require(marker.get('release') == str(release) and marker.get('uid') == account['uid']
                    and marker.get('role') == args.role and marker.get('producer') == POLICY,
                    'Host monitor deployment binding changed')
            observed, previous, written = {}, None, 0
            while True:
                observed, report = watch_once(args.role, account['uid'], observed)
                key = (report['status'], report.get('pid'), report.get('reason'))
                if key != previous or time.monotonic()-written >= 5:
                    write_health(args.role, account['uid'], report)
                    previous, written = key, time.monotonic()
                time.sleep(.5)
        return 0
    except (OSError, ValueError, ModError, subprocess.SubprocessError) as error:
        print('System host monitor: '+str(error)[:2000], file=sys.stderr); return 2


if __name__ == '__main__':
    raise SystemExit(main())
