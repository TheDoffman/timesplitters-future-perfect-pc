"""Root-only durable stop intents; no service commands or migration authority."""
from contextlib import contextmanager
from itertools import islice
import json
import os
import re
import stat
import time

import mod_host_cutover as cutover
from mod_host_migration import boot_identity, jobs
from mod_package import ModError, read_json, require

MAX_JOURNALS = 32
MAX_BYTES = 32768
SHA = re.compile('[0-9a-f]{64}\\Z')


def wire(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':'))+'\n').encode()


def stop_order():
    labels = jobs()
    return [v for v in labels if v.endswith('.monitor')] + ['org.timesplitters.workshop.backup'] + [
        v for v in labels if not v.endswith(('.monitor', '.backup'))]


def replay(events, clock):
    require(isinstance(events, list) and len(events) <= 22, 'Cutover event budget exceeded')
    stopped = [];pending = None;quiescent = False;settled = False;previous = 0
    order = stop_order()
    for event in events:
        require(isinstance(event, dict) and set(event) == {'kind','label','time'}
                and type(event['time']) is int and previous <= event['time'] <= clock()
                and not settled, 'Invalid cutover event history')
        previous = event['time']
        kind, label = event['kind'], event['label']
        if kind == 'stop-intent':
            require(not quiescent and pending is None and len(stopped) < len(order) and label == order[len(stopped)],
                    'Cutover stop intent is out of order')
            pending = label
        elif kind == 'stop-observed':
            require(not quiescent and pending is not None and label == pending, 'Cutover stop observation lacks its intent')
            stopped.append(label);pending = None
        elif kind == 'quiescent':
            require(not quiescent and label is None and pending is None and stopped == order,
                    'Cutover quiescence lacks complete stop observations')
            quiescent = True
        else:
            require(kind == 'settled' and label is None and pending is None, 'Invalid cutover settlement')
            settled = True
    return dict(stopped=stopped, pending=pending, quiescent=quiescent, settled=settled)


def checked(data, clock):
    value = read_json(data)
    require(isinstance(value, dict) and set(value) == {'schema','producer','before','after','uid','nonce',
            'plan_sha256','preflight_sha256','boot_id','events'} and type(value['schema']) is int
            and value['schema'] == 1 and value['producer'] == 'tsfp-root-cutover-stop-v1'
            and type(value['uid']) is int and value['uid'] >= 600 and value['before'] != value['after']
            and all(isinstance(value[k], str) and SHA.fullmatch(value[k]) for k in
                    ('before','after','nonce','plan_sha256','preflight_sha256','boot_id'))
            and data == wire(value), 'Preserving unknown cutover history')
    replay(value['events'], clock)
    return value


class Journal:
    def __init__(self, directory, fd, lock, name, base, clock):
        self.directory, self.fd, self.name, self.base, self.clock = directory, fd, name, base, clock
        self.lock = lock
        try:
            self.current = self._read()
        except FileNotFoundError:
            self.current = wire(dict(base, events=[]))
            self._new(name, self.current)
        self._decode(self.current)

    def _visible(self):
        require(cutover.stamp(os.stat('.cutover.lock', dir_fd=self.fd, follow_symlinks=False))
                == cutover.stamp(os.fstat(self.lock)), 'Cutover journal lease changed')
        current = cutover.protected_directory(self.directory)
        try:
            require((os.fstat(current).st_dev, os.fstat(current).st_ino)
                    == (os.fstat(self.fd).st_dev, os.fstat(self.fd).st_ino), 'Cutover journal directory changed')
        finally:os.close(current)

    def _read(self):
        self._visible()
        data, _ = cutover.protected_bytes(self.directory/self.name, 0o600, MAX_BYTES)
        self._visible()
        return data

    def _decode(self, data):
        value = checked(data, self.clock)
        require(isinstance(value, dict) and set(value) == {*self.base, 'events'}
                and all(value.get(k) == v for k,v in self.base.items()), 'Preserving foreign cutover journal')
        return value

    def _new(self, name, data):
        self._visible()
        require(0 < len(data) <= MAX_BYTES, 'Cutover journal exceeds storage budget')
        child = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=self.fd)
        with os.fdopen(child, 'wb') as output:
            os.fchmod(output.fileno(), 0o600);output.write(data);output.flush();os.fsync(output.fileno())
        os.fsync(self.fd)

    def state(self):
        require(self._read() == self.current, 'Cutover journal changed concurrently')
        return replay(self._decode(self.current)['events'], self.clock)

    def record(self, kind, label=None):
        """Persist intent before a stop, and observation only after OS verification.

        This stores the coordinator's observations; it does not perform them or
        constitute the separately required root-owned migration admission.
        Settlement is recorded only after the caller verifies restoration or
        final activation, or when a prepared transaction made no service change.
        """
        self.state()
        value = self._decode(self.current)
        value['events'].append(dict(kind=kind,label=label,time=int(self.clock())))
        replay(value['events'], self.clock)
        data = wire(value)
        pending = self.name+'.pending'
        self._new(pending, data)
        # Preserve a complete/partial pending file if the compare fails. A new
        # invocation refuses it rather than guessing which operation took place.
        require(self._read() == self.current, 'Cutover journal changed before commit')
        self._visible()
        os.replace(pending, self.name, src_dir_fd=self.fd, dst_dir_fd=self.fd);os.fsync(self.fd)
        self.current = data
        return self.state()


@contextmanager
def transaction(before, after, uid, nonce, plan_sha256, preflight_sha256, *, clock=time.time):
    require(os.geteuid() == cutover.ROOT_UID, 'Cutover journal requires root')
    require(all(isinstance(v, str) and SHA.fullmatch(v) for v in (before,after,nonce,plan_sha256,preflight_sha256))
            and before != after and type(uid) is int and uid >= 600, 'Invalid cutover transaction identity')
    import fcntl
    parent = cutover.protected_directory(cutover.ROOT)
    try:
        try:os.mkdir('upgrades', mode=0o700, dir_fd=parent);os.fsync(parent)
        except FileExistsError:pass
    finally:os.close(parent)
    directory = cutover.ROOT/'upgrades'
    fd = cutover.protected_directory(directory);lock = None
    try:
        require(stat.S_IMODE(os.fstat(fd).st_mode) == 0o700, 'Unsafe cutover journal directory')
        lock = os.open('.cutover.lock', os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW | os.O_NONBLOCK, 0o600, dir_fd=fd)
        info = os.fstat(lock)
        require(stat.S_ISREG(info.st_mode) and info.st_uid == cutover.ROOT_UID and info.st_nlink == 1
                and info.st_size == 0 and stat.S_IMODE(info.st_mode) == 0o600, 'Unsafe cutover journal lease')
        try:fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:raise ModError('Another cutover holds the lease') from error
        with os.scandir(fd) as scan:names = [entry.name for entry in islice(scan, MAX_JOURNALS+2)]
        require(len(names) <= MAX_JOURNALS+1 and all(name == '.cutover.lock' or re.fullmatch('[0-9a-f]{64}\\.json', name) for name in names),
                'Preserving unexpected or unfinished cutover artifacts')
        name = nonce+'.json'
        require(name in names or len(names) < MAX_JOURNALS+1, 'Cutover journal capacity exhausted')
        for other in names:
            if other in ('.cutover.lock', name):continue
            data, _ = cutover.protected_bytes(directory/other, 0o600, MAX_BYTES)
            history = checked(data, clock)
            require(history['nonce']+'.json' == other and replay(history['events'], clock)['settled'],
                    'Existing cutover requires recovery before another transaction')
        base = dict(schema=1,producer='tsfp-root-cutover-stop-v1',before=before,after=after,uid=uid,
                    nonce=nonce,plan_sha256=plan_sha256,preflight_sha256=preflight_sha256,boot_id=boot_identity())
        yield Journal(directory, fd, lock, name, base, clock)
    finally:
        if lock is not None:os.close(lock)
        os.close(fd)
