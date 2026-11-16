"""Root-only routing/stop history; observations never replace current OS proof."""
from contextlib import contextmanager
from itertools import islice
import json
import os
import re
import stat
import time

import mod_host_cutover as cutover
import mod_host_routing as routes
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


def routing_receipt(value, baseline):
    require(isinstance(value,dict) and set(value)=={*baseline,'expose'} and type(value['expose']) is bool
            and type(value['schema']) is int and value['schema']==1 and value['origin']==baseline['origin']
            and value['unrelated_sha256']==baseline['unrelated_sha256'] and value['phase'] in ('public','private','absent')
            and isinstance(value['config_sha256'],str) and SHA.fullmatch(value['config_sha256'])
            and (value['phase']!='public' or value['config_sha256']==baseline['config_sha256']),
            'Routing observation differs from the saved authority')
    return value


def replay(events, clock, baseline, before, after):
    routes.baseline(baseline)
    require(isinstance(events, list) and len(events) <= 64, 'Cutover event budget exceeded')
    stopped = [];pending = None;quiescent = False;settled = False;previous = 0
    suspended=False;route_pending=None;restored=False;access=None;access_time=None
    order = stop_order()
    for event in events:
        require(isinstance(event, dict) and set(event) == {'kind','label','time','data'}
                and type(event['time']) is int and previous <= event['time'] <= clock()
                and not settled, 'Invalid cutover event history')
        previous = event['time']
        kind, label = event['kind'], event['label']
        data=event['data']
        if kind in ('routing-intent','routing-observed','routing-unchanged'):
            require(label is None and pending is None,'Routing cannot cross a pending service stop')
            receipt=routing_receipt(data,baseline);expose=receipt['expose']
            if kind=='routing-unchanged':
                require(route_pending is False and not stopped and not suspended and not expose
                        and receipt['phase']=='public','Unconfirmed routing change cannot be discarded')
                route_pending=None
            elif kind=='routing-intent':
                require(route_pending is None,'Another routing intent remains unresolved')
                if expose:
                    require(suspended and access is not None and 0<=event['time']-access_time<=300
                            and receipt['phase']!='public','Public restoration requires fresh verified private access')
                else:
                    require(receipt['phase']=='public','Suspension intent requires the saved public route')
                    # Re-exposure during an interrupted upgrade can be suspended
                    # again, but invalidates any earlier private admission.
                    suspended=False;restored=False;access=None;access_time=None
                route_pending=expose
            else:
                require(route_pending is expose,'Routing observation lacks its matching durable intent')
                if expose:
                    require(receipt['phase']=='public' and access is not None
                            and 0<=event['time']-access_time<=300,'Public restoration admission is stale or missing')
                    suspended=False;restored=True
                else:
                    require(receipt['phase']!='public','Public routing remains exposed')
                    suspended=True
                route_pending=None
        elif kind=='access-verified':
            require(label is None and pending is None and suspended and route_pending is not False
                    and isinstance(data,dict) and set(data)=={'release','sha256'}
                    and data['release'] in (before,after) and isinstance(data['sha256'],str) and SHA.fullmatch(data['sha256'])
                    and (data['release']==before or quiescent),'Private access verification lacks the required stopped state')
            access=data;access_time=event['time']
        elif kind == 'stop-intent':
            require(data is None and suspended and route_pending is None and not restored and not quiescent
                    and pending is None and len(stopped) < len(order) and label == order[len(stopped)],
                    'Cutover stop intent is out of order')
            pending = label
        elif kind == 'stop-observed':
            require(data is None and suspended and route_pending is None and not quiescent
                    and pending is not None and label == pending, 'Cutover stop observation lacks its intent')
            stopped.append(label);pending = None
        elif kind == 'quiescent':
            require(data is None and suspended and route_pending is None and not quiescent
                    and label is None and pending is None and stopped == order,
                    'Cutover quiescence lacks complete stop observations')
            quiescent = True
        else:
            require(kind == 'settled' and data is None and label is None and pending is None
                    and route_pending is None and not suspended,'Invalid cutover settlement')
            settled = True
    return dict(stopped=stopped,pending=pending,quiescent=quiescent,settled=settled,
                routing_suspended=suspended,routing_pending=route_pending,routing_restored=restored,
                access=access,access_time=access_time)


def checked(data, clock):
    value = read_json(data)
    require(isinstance(value, dict) and set(value) == {'schema','producer','before','after','uid','nonce',
            'plan_sha256','preflight_sha256','boot_id','tracked','routing','events'} and type(value['schema']) is int
            and value['schema'] == 2 and value['producer'] == 'tsfp-root-cutover-v2'
            and type(value['uid']) is int and value['uid'] >= 600 and value['before'] != value['after']
            and all(isinstance(value[k], str) and SHA.fullmatch(value[k]) for k in
                    ('before','after','nonce','plan_sha256','preflight_sha256','boot_id'))
            and data == wire(value), 'Preserving unknown cutover history')
    replay(value['events'],clock,value['routing'],value['before'],value['after'])
    cutover.checked_tracking(value['tracked'])
    return value


class Journal:
    def __init__(self, directory, fd, lock, name, base, clock, recover_pending=False):
        self.directory, self.fd, self.name, self.base, self.clock = directory, fd, name, base, clock
        self.lock = lock
        try:
            self.current = self._read()
        except FileNotFoundError:
            self.current = wire(dict(base, events=[]))
            self._new(name, self.current)
        self._decode(self.current)
        if recover_pending:self.recover_pending()

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

    def recover_pending(self):
        """Explicitly promote only one complete validated append to known history.

        This recovers durable metadata, not a current OS observation. Callers
        must still re-observe routing/jobs/processes before acting on recovery.
        Truncated/foreign/multiple-event pending files are never removed/adopted.
        """
        self.state()
        pending=self.name+'.pending'
        try:data,identity=cutover.protected_bytes(self.directory/pending,0o600,MAX_BYTES)
        except FileNotFoundError:return False
        old=self._decode(self.current);new=self._decode(data)
        require(len(new['events'])==len(old['events'])+1 and new['events'][:-1]==old['events'],
                'Pending cutover history is not one known complete append')
        again,stamp=cutover.protected_bytes(self.directory/pending,0o600,MAX_BYTES)
        require(again==data and stamp==identity and self._read()==self.current,
                'Pending cutover history changed during recovery')
        self._visible()
        os.replace(pending,self.name,src_dir_fd=self.fd,dst_dir_fd=self.fd);os.fsync(self.fd)
        self.current=data;self.state()
        return True

    def state(self):
        require(self._read() == self.current, 'Cutover journal changed concurrently')
        return self._replay(self._decode(self.current)['events'])

    def _replay(self,events):
        return replay(events,self.clock,self.base['routing'],self.base['before'],self.base['after'])

    def record(self, kind, label=None, data=None):
        """Persist intent before a stop, and observation only after OS verification.

        This stores the coordinator's observations; it does not perform them or
        constitute the separately required root-owned migration admission.
        Settlement is recorded only after the caller verifies restoration or
        final activation, or when a prepared transaction made no service change.
        """
        self.state()
        value = self._decode(self.current)
        value['events'].append(dict(kind=kind,label=label,time=int(self.clock()),data=data))
        self._replay(value['events'])
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

    def routing_event(self,kind,data):
        """Adapter for routes.transition; repeated recovery observations are inert."""
        state=self.state();receipt=routing_receipt(data,self.base['routing'])
        require(kind in ('routing-intent','routing-observed'),'Invalid routing callback')
        if kind=='routing-intent' and state['routing_pending'] is receipt['expose']:
            require((receipt['phase']!='public') if receipt['expose'] else (receipt['phase']=='public'),
                    'Repeated routing intent differs from its observed source state')
            if receipt['expose']:
                require(state['access'] is not None and 0<=self.clock()-state['access_time']<=300,
                        'Public restoration admission expired')
            return state
        if kind=='routing-observed' and state['routing_pending'] is None:
            desired=receipt['phase']=='public' if receipt['expose'] else receipt['phase']!='public'
            require(desired and ((state['routing_restored'] and receipt['expose'])
                    or (state['routing_suspended'] and not receipt['expose'])),
                    'Routing observation lacks a recorded transition')
            return state
        return self.record(kind,data=data)

    def require_suspended(self,invoke):
        state=self.state()
        require(state['routing_suspended'] and state['routing_pending'] is None and not state['settled'],
                'Public routing suspension must be durably observed')
        current=routes.snapshot(invoke('status'),self.base['routing']['origin'])
        require(current['phase']!='public' and current['unrelated_sha256']==self.base['routing']['unrelated_sha256'],
                'Current routing no longer matches the suspended workshop')
        return current


@contextmanager
def transaction(before, after, uid, nonce, plan_sha256, preflight_sha256, tracked, routing, *, clock=time.time,recover_pending=False):
    require(os.geteuid() == cutover.ROOT_UID, 'Cutover journal requires root')
    require(all(isinstance(v, str) and SHA.fullmatch(v) for v in (before,after,nonce,plan_sha256,preflight_sha256))
            and before != after and type(uid) is int and uid >= 600, 'Invalid cutover transaction identity')
    cutover.checked_tracking(tracked)
    routes.baseline(routing)
    require(type(recover_pending) is bool,'Choose explicit pending-history recovery')
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
        name=nonce+'.json';pending=name+'.pending'
        with os.scandir(fd) as scan:names = [entry.name for entry in islice(scan, MAX_JOURNALS+3)]
        require(len(names) <= MAX_JOURNALS+2 and all(member == '.cutover.lock'
                or re.fullmatch('[0-9a-f]{64}\\.json',member)
                or (recover_pending and member==pending and name in names) for member in names),
                'Preserving unexpected or unfinished cutover artifacts')
        require(name in names or len([member for member in names if member.endswith('.json')]) < MAX_JOURNALS,
                'Cutover journal capacity exhausted')
        for other in names:
            if other in ('.cutover.lock',name,pending):continue
            data, _ = cutover.protected_bytes(directory/other, 0o600, MAX_BYTES)
            history = checked(data, clock)
            require(history['nonce']+'.json' == other and replay(history['events'],clock,history['routing'],history['before'],history['after'])['settled'],
                    'Existing cutover requires recovery before another transaction')
        base = dict(schema=2,producer='tsfp-root-cutover-v2',before=before,after=after,uid=uid,
                    nonce=nonce,plan_sha256=plan_sha256,preflight_sha256=preflight_sha256,
                    tracked=tracked,routing=routing,boot_id=boot_identity())
        yield Journal(directory,fd,lock,name,base,clock,recover_pending)
    finally:
        if lock is not None:os.close(lock)
        os.close(fd)
