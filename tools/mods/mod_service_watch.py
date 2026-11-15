"""Shared managed-controller health and owned launchd descendant supervision."""
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import time

from mod_backup import private_directory, regular
from mod_package import ModError, read_json, require
from mod_host_watch import managed_pid, run_command, sample


class Supervisor:
    def __init__(self, data, kind, binding, policy, memory_bytes=2*1024**3,
                 controller_seconds=15, health_seconds=10):
        require((kind, binding) in (('publication', 'repository'), ('gateway', 'origin')),
                'Invalid managed service role')
        self.data, self.kind, self.binding, self.policy = data, kind, binding, policy
        self.memory_bytes, self.controller_seconds, self.health_seconds = memory_bytes, controller_seconds, health_seconds

    def status(self, value, component=None):
        data = self.data
        component = component or self.kind
        require(component in (self.kind, 'watchdog'), 'Invalid publication status component')
        path = data['state'] / (component + '.json')
        if path.exists() or path.is_symlink():
            regular(path, 8192)
            old = read_json(path.read_bytes())
            require(isinstance(old, dict) and old.get('schema') == 1 and old.get(self.binding) == str(data[self.binding])
                    and old.get('service') in (None, data['label']),
                    'Preserving unknown publication status')
        else:
            old = {}
        result = dict(schema=1, **{self.binding: str(data[self.binding])}, service=data['label'],
                      component=component, pid=os.getpid(), time=int(time.time()), **value)
        if value['status'] in ('error', 'terminated'):
            result['last_fault'] = dict(time=result['time'], reason=value.get('reason',
                                       'Live publication stopped; private operator inspection required'))
        elif 'last_fault' in old:
            result['last_fault'] = old['last_fault']
        wire = json.dumps(result, separators=(',', ':')).encode()
        require(len(wire) <= 8192, 'Publication status exceeds budget')
        fd, temporary = tempfile.mkstemp(prefix='.publication-status-', dir=data['state'])
        try:
            with os.fdopen(fd, 'wb') as stream:
                stream.write(wire)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)


    def healthy(self):
        data = self.data
        path = data['state'] / 'watchdog.json'
        regular(path, 8192)
        value = read_json(path.read_bytes())
        stamp = value.get('time') if isinstance(value, dict) else None
        require(isinstance(value, dict) and value.get('schema') == 1 and value.get('service') == data['label']
                and value.get(self.binding) == str(data[self.binding])
                and value.get('component') == 'watchdog' and value.get('supervision_policy') == self.policy
                and value.get('status') == 'watching' and value.get('watched_pid') == os.getpid()
                and type(stamp) is int and 0 <= time.time() - stamp <= self.health_seconds,
                'Publication watchdog is not healthy')


    def target(self):
        data = self.data
        return 'gui/' + str(data['uid']) + '/' + data['label']


    def watch_once(self, observed, command=run_command, clock=time.monotonic):
        data = self.data
        code, output = command(['/bin/launchctl', 'print', self.target()])
        if code != 0:
            command(['/bin/launchctl', 'kill', 'SIGKILL', self.target()])
            return {}, dict(status='error', reason='Controller service is not loaded')
        pid = managed_pid(output)
        if pid is None:
            return {}, dict(status='idle', supervision_policy=self.policy)
        now = clock()
        start = observed.get('start', now) if observed.get('pid') == pid else now
        memory = sample(pid, command, data['uid'])
        if memory is None:
            return {}, dict(status='idle', supervision_policy=self.policy)
        reason = 'memory' if memory > self.memory_bytes else None
        if not reason and now - start > self.controller_seconds:
            try:
                path = data['state'] / (self.kind + '.json')
                regular(path, 8192)
                value = read_json(path.read_bytes())
                stamp = value.get('time') if isinstance(value, dict) else None
                require(isinstance(value, dict) and value.get('schema') == 1 and value.get('service') == data['label']
                        and value.get(self.binding) == str(data[self.binding])
                        and value.get('pid') == pid and type(stamp) is int
                        and 0 <= time.time() - stamp <= self.controller_seconds, 'Controller heartbeat expired')
            except (ModError, OSError, ValueError):
                reason = 'heartbeat'
        if reason:
            command(['/bin/launchctl', 'kill', 'SIGKILL', self.target()])
            return {}, dict(status='terminated', reason=reason, watched_pid=pid, rss_bytes=memory)
        return dict(pid=pid, start=start), dict(status='watching', watched_pid=pid, rss_bytes=memory,
                                              supervision_policy=self.policy)


    def watchdog(self):
        data = self.data
        observed, written, previous = {}, 0, None
        while True:
            try:
                observed, report = self.watch_once(observed)
            except (ModError, OSError, ValueError, subprocess.SubprocessError):
                observed, report = {}, dict(status='error', reason='Publication supervision failed')
                # Withhold/cancel before attempting to write a possibly damaged
                # status file; metadata errors must never leave downloads running.
                run_command(['/bin/launchctl', 'kill', 'SIGKILL', self.target()])
            key = (report['status'], report.get('watched_pid'), report.get('reason'))
            if key != previous or time.monotonic() - written >= 5:
                try:
                    self.status(report, 'watchdog')
                except (ModError, OSError, ValueError):
                    run_command(['/bin/launchctl', 'kill', 'SIGKILL', self.target()])
                    raise
                previous, written = key, time.monotonic()
            time.sleep(0.5)


    def generate(self, source, output):
        data = self.data
        output = Path(os.path.abspath(output))
        private_directory(output.parent)
        require(not output.exists() and not output.is_symlink(), 'Choose a new publication agent bundle')
        output.mkdir(mode=0o700)
        arguments = [str(Path(sys.executable).absolute()), '-I', '-B', str(Path(source).resolve()), '--config', str(data['config'])]
        for component in (self.kind, 'watchdog'):
            label = data['label'] + ('.watchdog' if component == 'watchdog' else '')
            value = dict(Label=label, ProgramArguments=arguments + ['--managed' if component == self.kind else '--watchdog'],
                         RunAtLoad=True, KeepAlive=True, ThrottleInterval=60 if component == self.kind else 5,
                         ProcessType='Background', Umask=0o077, AbandonProcessGroup=False,
                         HardResourceLimits=dict(Core=0, FileSize=512 * 1024**2),
                         StandardOutPath='/dev/null', StandardErrorPath='/dev/null')
            path = output / (label + '.plist')
            with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), 'wb') as stream:
                plistlib.dump(value, stream)
                stream.flush()
                os.fsync(stream.fileno())
        return dict(bundle=str(output), controller_label=data['label'], watchdog_label=data['label'] + '.watchdog')
