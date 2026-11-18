"""Root-only routing/stop history; observations never replace current OS proof."""
from contextlib import contextmanager
from itertools import islice
import json
import hashlib
import os
import re
import stat
import time

import mod_host_cutover as cutover
import mod_host_routing as routes
from mod_host_migration import boot_identity, jobs
from mod_package import ModError, read_json, require

MAX_JOURNALS = 32
MAX_BYTES = 131072
MAX_EVENTS = 256
SHA = re.compile('[0-9a-f]{64}\\Z')


def wire(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':'))+'\n').encode()


def stop_order():
    labels = jobs()
    return [v for v in labels if v.endswith('.monitor')] + ['org.timesplitters.workshop.backup'] + [
        v for v in labels if not v.endswith(('.monitor', '.backup'))]


def start_order():
    labels=jobs()
    return [value for value in labels if value.endswith('.monitor')]+['org.timesplitters.workshop.backup']+[
        value for value in labels if not value.endswith(('.monitor','.backup'))]


def restart_order():
    labels=jobs()
    return [value for value in labels if not value.endswith('.monitor') and value.split('.')[3] in ('api','gateway','publication')]+[
        value for value in labels if value.endswith('.monitor')]


def routing_receipt(value, baseline):
    require(isinstance(value,dict) and set(value)=={*baseline,'expose'} and type(value['expose']) is bool
            and type(value['schema']) is int and value['schema']==1 and value['origin']==baseline['origin']
            and value['unrelated_sha256']==baseline['unrelated_sha256'] and value['phase'] in ('public','private','absent')
            and isinstance(value['config_sha256'],str) and SHA.fullmatch(value['config_sha256'])
            and (value['phase']!='public' or value['config_sha256']==baseline['config_sha256']),
            'Routing observation differs from the saved authority')
    return value


def replay(events, clock, baseline, before, after,uid,backup_identity,backup_base_release,plan_sha256):
    routes.baseline(baseline)
    require(isinstance(events, list) and len(events) <= MAX_EVENTS, 'Cutover event budget exceeded')
    stopped = [];pending = None;quiescent = False;settled = False;previous = 0
    suspended=False;route_pending=None;restored=False;access=None;access_time=None
    quiesced_at=None;checked_at=None;backup=None
    config='before';config_pending=None;layout_sha=None;layout_pending=None;layout_complete=None
    monitor='before';boots={label:'before' for label in jobs() if not label.endswith('.backup')}
    retired={};retire_pending=None;scanner=None;scanner_time=None
    started=[];start_pending=None;activation_generation=None
    private_baseline=None;catalog_floor=1;private_ready=None;private_time=None;restarted=[];restart_pending=None;activation_ready=False
    shutdown=None;shutdown_stopped=[];shutdown_pending=None;shutdown_complete=False;monitor_checkpoints={}
    order = stop_order()
    for event in events:
        require(isinstance(event, dict) and set(event) == {'kind','label','time','data'}
                and type(event['time']) is int and previous <= event['time'] <= clock()
                and not settled, 'Invalid cutover event history')
        previous = event['time']
        kind, label = event['kind'], event['label']
        data=event['data']
        if kind=='baseline-private-verified':
            require(label is None and private_baseline is None and not stopped and not suspended and route_pending is None
                    and not restored and layout_sha is None and isinstance(data,dict),
                    'Private baseline must precede shutdown and protected changes')
            from mod_host_upgrade_process import checked_private_result
            checked_private_result(data,before,uid,dict(origin=baseline['origin'],trusted_root_sha256=data.get('trusted_root_sha256')))
            private_baseline=data;catalog_floor=data['catalog_version']
        elif kind in ('routing-intent','routing-observed','routing-unchanged'):
            require(label is None and pending is None,'Routing cannot cross a pending service stop')
            receipt=routing_receipt(data,baseline);expose=receipt['expose']
            require(not expose or (config_pending is None and layout_pending is None and retire_pending is None
                    and start_pending is None and restart_pending is None and (shutdown is None or shutdown_complete)
                    and (not started or activation_ready)),
                    'Public restoration cannot cross a pending protected operation')
            if expose and started:
                require(private_ready is not None and access is not None and 0<=event['time']-private_time<=300
                        and 0<=event['time']-scanner_time<=300
                        and access['sha256']==hashlib.sha256(wire(private_ready)).hexdigest(),
                        'Public restoration admission expired or changed')
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
                    quiescent=False;quiesced_at=None;checked_at=None;backup=None
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
                    and retire_pending is None and start_pending is None and restart_pending is None
                    and (shutdown is None or shutdown_complete)
                    and (not started or activation_ready)
                    and isinstance(data,dict) and set(data)=={'release','sha256'}
                    and data['release'] in (before,after) and isinstance(data['sha256'],str) and SHA.fullmatch(data['sha256'])
                    and (data['release']==before or quiescent or activation_ready),'Private access verification lacks the required stopped state')
            if started:
                require(private_ready is not None and 0<=event['time']-private_time<=300
                        and 0<=event['time']-scanner_time<=300
                        and data['sha256']==hashlib.sha256(wire(private_ready)).hexdigest(),
                        'Public access lacks fresh private/AV evidence')
            if layout_sha is not None:
                require(config_pending is None and layout_pending is None and layout_complete==config
                        and monitor==config and all(value==config for value in boots.values())
                        and data['release']==(before if config=='before' else after),
                        'Private access does not match completed configuration/layout')
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
                    and label is None and pending is None and stopped == order and not started and start_pending is None,
                    'Cutover quiescence lacks complete stop observations')
            quiescent = True
            quiesced_at=event['time'];checked_at=event['time']
        elif kind=='quiescence-rechecked':
            require(data is None and label is None and suspended and route_pending is None
                    and pending is None and quiescent and stopped==order and not started and start_pending is None,
                    'Quiescence recheck lacks stopped state')
            checked_at=event['time']
        elif kind=='backup-verified':
            require(label is None and suspended and route_pending is None and pending is None
                    and retire_pending is None and start_pending is None and not started
                    and quiescent,'Backup verification requires observed quiescence')
            from mod_host_upgrade_process import checked_backup_receipt
            backup=checked_backup_receipt(data,backup_base_release,uid,backup_identity,quiesced_at,clock=lambda:event['time'])
            retired={};scanner=None;scanner_time=None
        elif kind=='layout-prepared':
            require(label is None and isinstance(data,str) and SHA.fullmatch(data) and layout_sha is None
                    and quiescent and suspended and route_pending is None and pending is None
                    and config_pending is None and backup is not None,
                    'Prepared layout lacks verified shutdown/backup context')
            layout_sha=data;layout_complete='before'
        elif kind in ('migration-intent','migration-observed'):
            require(label in ('apply','rollback') and quiescent and suspended and route_pending is None
                    and pending is None and layout_pending is None and layout_sha is not None,
                    'Private migration lacks protected shutdown/layout context')
            target='after' if label=='apply' else 'before'
            if kind=='migration-intent':
                require(data==plan_sha256 and config_pending is None and config!=target and monitor=='before'
                        and all(value=='before' for value in boots.values()) and backup is not None
                        and 0<=event['time']-backup['finished']<=300,
                        'Private migration intent changes the plan or precedes file restoration')
                config_pending=label;access=None;access_time=None
            else:
                from mod_host_upgrade_process import checked_migration_result
                require(config_pending==label,'Private migration observation lacks its intent')
                checked_migration_result(data,before,after,plan_sha256,rollback=label=='rollback')
                config=target;config_pending=None
        elif kind in ('layout-intent','layout-observed'):
            require(isinstance(label,str) and data==layout_sha and layout_sha is not None and config=='after'
                    and config_pending is None and quiescent and suspended and route_pending is None and pending is None,
                    'Protected file exchange lacks completed private migration')
            direction,separator,operation=label.partition('-')
            require(separator and direction in ('apply','rollback') and operation in {'archive','install',*boots},
                    'Unmanaged protected file exchange')
            source='before' if direction=='apply' else 'after';target='after' if direction=='apply' else 'before'
            if kind=='layout-intent':
                require(layout_pending is None,'Another protected effect remains unresolved')
                require((operation=='archive' and monitor==source)
                        or (operation=='install' and monitor=='absent')
                        or (operation in boots and monitor==target and boots[operation]==source),
                        'Protected effect is out of order or already completed')
                layout_pending=label;layout_complete=None;access=None;access_time=None
            else:
                require(layout_pending==label,'Protected observation lacks its matching durable intent')
                if operation=='archive':monitor='absent'
                elif operation=='install':monitor=target
                else:boots[operation]=target
                layout_pending=None
        elif kind=='layout-complete':
            require(label in ('apply','rollback') and config=='after' and config_pending is None and layout_pending is None
                    and quiescent and suspended and pending is None and route_pending is None,
                    'Protected layout completion lacks shutdown context')
            target='after' if label=='apply' else 'before'
            require(layout_sha is not None and monitor==target and all(value==target for value in boots.values())
                    and isinstance(data,dict) and set(data)=={'phase','layout_sha256','boot_files','backup_changed','services_started'}
                    and data['phase']=='protected-layout-'+('applied' if label=='apply' else 'rolled-back')
                    and data['layout_sha256']==layout_sha and type(data['boot_files']) is int and data['boot_files']==9
                    and data['backup_changed'] is False and data['services_started'] is False,
                    'Protected layout result differs from complete observed effects')
            layout_complete=target
        elif kind in ('proof-retire-intent','proof-retire-observed'):
            require(label in ('before','after') and label==config==monitor==layout_complete
                    and layout_sha is not None and all(value==label for value in boots.values())
                    and quiescent and suspended and pending is None and route_pending is None
                    and config_pending is None and layout_pending is None and not started and start_pending is None
                    and (data is None or (isinstance(data,str) and SHA.fullmatch(data))),
                    'Proof retirement lacks complete quiescent generation context')
            if kind=='proof-retire-intent':
                require(retire_pending is None and label not in retired,'Another proof retirement remains unresolved')
                retire_pending=dict(generation=label,sha256=data)
            else:
                require(retire_pending==dict(generation=label,sha256=data),'Proof retirement observation lacks intent')
                retired[label]=data;retire_pending=None
        elif kind=='scanner-verified':
            require(label in retired and label==config==monitor==layout_complete and suspended and route_pending is None
                    and config_pending is None and layout_pending is None and retire_pending is None
                    and (shutdown is None or shutdown_complete)
                    and (quiescent or activation_generation==label),'Scanner admission lacks retired-proof generation context')
            from mod_host_upgrade_process import checked_scanner_result
            checked_scanner_result(data,before if label=='before' else after,uid)
            scanner=data;scanner_time=event['time'];private_ready=None;private_time=None;activation_ready=False
        elif kind in ('start-intent','start-observed'):
            require(label in jobs() and data is not None and config_pending is None and layout_pending is None
                    and retire_pending is None and suspended and route_pending is None and pending is None
                    and (shutdown is None or shutdown_complete)
                    and layout_complete==config==monitor and config in retired and scanner is not None and private_baseline is not None,
                    'Private startup lacks complete admitted generation context')
            if kind=='start-intent':
                require(data==config and start_pending is None and len(started)<len(start_order())
                        and label==start_order()[len(started)] and (quiescent or activation_generation==config)
                        and 0<=event['time']-scanner_time<=300,'Private startup is out of order or scanner admission expired')
                start_pending=label;activation_generation=config;quiescent=False;checked_at=None;access=None;access_time=None
                shutdown=None;shutdown_stopped=[];shutdown_pending=None;shutdown_complete=False
            else:
                require(start_pending==label and isinstance(data,dict) and set(data)=={'path','pid'}
                        and isinstance(data['path'],str) and 0<len(data['path'])<=2048
                        and (data['pid'] is None or (type(data['pid']) is int and 1<data['pid']<2**31)),
                        'Private startup observation lacks its fixed pending intent')
                started.append(label);start_pending=None
        elif kind=='private-readiness-verified':
            require(label==config==monitor==layout_complete and activation_generation==config and started==start_order()
                    and start_pending is None and restart_pending is None and config_pending is None and layout_pending is None
                    and retire_pending is None and route_pending is None and suspended and private_baseline is not None and shutdown is None
                    and scanner is not None and 0<=event['time']-scanner_time<=300,
                    'Private readiness lacks complete fresh private startup context')
            from mod_host_upgrade_process import checked_private_result
            checked_private_result(data,before if label=='before' else after,uid,private_baseline,floor=catalog_floor)
            private_ready=data;private_time=event['time'];catalog_floor=data['catalog_version']
            activation_ready=restarted==restart_order()
        elif kind in ('restart-intent','restart-observed'):
            require(label in restart_order() and started==start_order() and start_pending is None and suspended
                    and route_pending is None and config_pending is None and layout_pending is None and retire_pending is None and shutdown is None,
                    'Restart lacks complete private job context')
            if kind=='restart-intent':
                require(restart_pending is None and len(restarted)<len(restart_order()) and label==restart_order()[len(restarted)]
                        and private_ready is not None and 0<=event['time']-private_time<=300
                        and isinstance(data,dict) and set(data)=={'pid','tracked'}
                        and type(data['pid']) is int and 1<data['pid']<2**31,'Restart intent is out of order or unbound')
                cutover.checked_tracking(data['tracked'])
                require(data['pid'] in data['tracked']['pids'],'Restart tree omits its owned job root')
                restart_pending=dict(label=label,**data);private_ready=None;private_time=None;activation_ready=False;access=None;access_time=None
            else:
                require(restart_pending is not None and restart_pending['label']==label and isinstance(data,dict)
                        and set(data)=={'pid','old_tree_exited'} and type(data['pid']) is int and 1<data['pid']<2**31
                        and data['pid']!=restart_pending['pid'] and data['old_tree_exited'] is True,
                        'Restart observation lacks a new process and old-tree cleanup')
                restarted.append(label);restart_pending=None
        elif kind=='shutdown-intent':
            require(label==config==monitor==layout_complete and activation_generation==config and shutdown is None
                    and (started or start_pending is not None) and suspended and route_pending is None
                    and config_pending is None and layout_pending is None and retire_pending is None
                    and isinstance(data,dict) and set(data)=={'tracked','loaded'} and isinstance(data['tracked'],list)
                    and 1<=len(data['tracked'])<=2 and isinstance(data['loaded'],list) and data['loaded']==sorted(set(data['loaded']))
                    and set(data['loaded'])<=set(started+([start_pending] if start_pending else [])),
                    'Shutdown context differs from known private startup')
            for tracked in data['tracked']:cutover.checked_tracking(tracked)
            shutdown=dict(generation=label,**data);shutdown_stopped=[];shutdown_pending=None;shutdown_complete=False
            activation_ready=False;private_ready=None;private_time=None;access=None;access_time=None
        elif kind in ('shutdown-stop-intent','shutdown-stop-observed'):
            require(shutdown is not None and not shutdown_complete and data is None and suspended and route_pending is None,
                    'Private shutdown lacks its durable generation context')
            if kind=='shutdown-stop-intent':
                require(shutdown_pending is None and len(shutdown_stopped)<len(order) and label==order[len(shutdown_stopped)],
                        'Private shutdown stop order differs')
                shutdown_pending=label
            else:
                require(shutdown_pending==label,'Private shutdown observation lacks its intent')
                shutdown_stopped.append(label);shutdown_pending=None
        elif kind=='shutdown-complete':
            require(shutdown is not None and label==shutdown['generation']==config and data is None
                    and shutdown_stopped==order and shutdown_pending is None and not shutdown_complete
                    and suspended and route_pending is None,'Private shutdown lacks all fixed stop observations')
            shutdown_complete=True;started=[];start_pending=None;activation_generation=None;scanner=None;scanner_time=None
            restarted=[];restart_pending=None;private_ready=None;private_time=None;activation_ready=False;retired={};backup=None
            quiescent=True;quiesced_at=event['time'];checked_at=event['time']
        elif kind=='monitor-checkpoint':
            require(label==config==monitor==layout_complete and shutdown_complete and shutdown is not None
                    and quiescent and not started and start_pending is None and shutdown_pending is None
                    and suspended and route_pending is None and isinstance(data,str) and SHA.fullmatch(data),
                    'Monitor history checkpoint lacks completed private shutdown')
            monitor_checkpoints[label]=data
        else:
            require(kind == 'settled' and data is None and label is None and pending is None
                    and route_pending is None and not suspended and config_pending is None and layout_pending is None
                    and retire_pending is None and start_pending is None and restart_pending is None
                    and (shutdown is None or shutdown_complete)
                    and (not started or activation_ready)
                    and (layout_sha is None or (layout_complete==config and monitor==config
                        and all(value==config for value in boots.values()))),'Invalid cutover settlement')
            settled = True
    return dict(stopped=stopped,pending=pending,quiescent=quiescent,settled=settled,
                routing_suspended=suspended,routing_pending=route_pending,routing_restored=restored,
                access=access,access_time=access_time,quiesced_at=quiesced_at,checked_at=checked_at,backup=backup,
                config=config,config_pending=config_pending,layout_sha256=layout_sha,layout_pending=layout_pending,
                layout_complete=layout_complete,monitor=monitor,boots=boots,retired_proofs=retired,
                retire_pending=retire_pending,scanner=scanner,scanner_time=scanner_time,started=started,
                start_pending=start_pending,activation_generation=activation_generation,private_baseline=private_baseline,
                catalog_floor=catalog_floor,private_ready=private_ready,private_time=private_time,restarted=restarted,
                restart_pending=restart_pending,activation_ready=activation_ready,shutdown=shutdown,
                shutdown_stopped=shutdown_stopped,shutdown_pending=shutdown_pending,shutdown_complete=shutdown_complete,
                monitor_checkpoints=monitor_checkpoints)


def checked(data, clock):
    value = read_json(data)
    require(isinstance(value, dict) and set(value) == {'schema','producer','before','after','uid','nonce',
            'plan_sha256','preflight_sha256','boot_id','tracked','routing','backup_identity','backup_base_release','events'} and type(value['schema']) is int
            and value['schema'] == 2 and value['producer'] == 'tsfp-root-cutover-v2'
            and type(value['uid']) is int and value['uid'] >= 600 and value['before'] != value['after']
            and all(isinstance(value[k], str) and SHA.fullmatch(value[k]) for k in
                    ('before','after','nonce','plan_sha256','preflight_sha256','boot_id','backup_identity','backup_base_release'))
            and data == wire(value), 'Preserving unknown cutover history')
    replay(value['events'],clock,value['routing'],value['before'],value['after'],value['uid'],value['backup_identity'],value['backup_base_release'],value['plan_sha256'])
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
        return replay(events,self.clock,self.base['routing'],self.base['before'],self.base['after'],self.base['uid'],
                      self.base['backup_identity'],self.base['backup_base_release'],self.base['plan_sha256'])

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

    def layout_event(self,kind,operation,identity):
        """Durable adapter for the fixed protected-file exchange primitive."""
        state=self.state()
        require(kind in ('intent','observed') and identity==state['layout_sha256'],'Foreign protected layout callback')
        if kind=='intent' and state['layout_pending']==operation:return state
        return self.record('layout-'+kind,operation,identity)

    def require_suspended(self,invoke):
        state=self.state()
        require(state['routing_suspended'] and state['routing_pending'] is None and not state['settled'],
                'Public routing suspension must be durably observed')
        current=routes.snapshot(invoke('status'),self.base['routing']['origin'])
        require(current['phase']!='public' and current['unrelated_sha256']==self.base['routing']['unrelated_sha256'],
                'Current routing no longer matches the suspended workshop')
        return current


def recovery_arguments(nonce,before,after,uid,backup_identity,*,clock=time.time):
    """Read bounded historical arguments; lease acquisition must revalidate them.

    This creates nothing and performs no service operation. A changed base
    between this read and acquiring the lease is refused by Journal._decode.
    """
    require(os.geteuid()==cutover.ROOT_UID and isinstance(nonce,str) and SHA.fullmatch(nonce),
            'An exact Root recovery transaction is required')
    path=cutover.ROOT/'upgrades'/(nonce+'.json')
    payload,_=cutover.protected_bytes(path,0o600,MAX_BYTES);value=checked(payload,clock)
    require(value['nonce']==nonce and value['boot_id']==boot_identity()
            and all(type(value[key]) is type(expected) and value[key]==expected for key,expected in
                    (('before',before),('after',after),('uid',uid),('backup_identity',backup_identity))),
            'Recovery history differs from this boot, release or service identity')
    return {key:value[key] for key in ('before','after','uid','nonce','plan_sha256','preflight_sha256',
        'tracked','routing','backup_identity','backup_base_release')}


@contextmanager
def transaction(before, after, uid, nonce, plan_sha256, preflight_sha256, tracked, routing,backup_identity,backup_base_release, *, clock=time.time,recover_pending=False,existing_only=False,new_only=False):
    require(os.geteuid() == cutover.ROOT_UID, 'Cutover journal requires root')
    require(all(isinstance(v, str) and SHA.fullmatch(v) for v in (before,after,nonce,plan_sha256,preflight_sha256,backup_identity,backup_base_release))
            and before != after and type(uid) is int and uid >= 600, 'Invalid cutover transaction identity')
    cutover.checked_tracking(tracked)
    routes.baseline(routing)
    require(all(type(v) is bool for v in (recover_pending,existing_only,new_only)) and not (existing_only and new_only),
            'Choose explicit journal creation or recovery')
    import fcntl
    if not existing_only:
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
        require((not existing_only or name in names) and (not new_only or name not in names),
                'Recovery requires existing history; a new apply requires an unused nonce')
        require(name in names or len([member for member in names if member.endswith('.json')]) < MAX_JOURNALS,
                'Cutover journal capacity exhausted')
        for other in names:
            if other in ('.cutover.lock',name,pending):continue
            data, _ = cutover.protected_bytes(directory/other, 0o600, MAX_BYTES)
            history = checked(data, clock)
            require(history['nonce']+'.json' == other and replay(history['events'],clock,history['routing'],history['before'],history['after'],
                    history['uid'],history['backup_identity'],history['backup_base_release'],history['plan_sha256'])['settled'],
                    'Existing cutover requires recovery before another transaction')
        base = dict(schema=2,producer='tsfp-root-cutover-v2',before=before,after=after,uid=uid,
                    nonce=nonce,plan_sha256=plan_sha256,preflight_sha256=preflight_sha256,
                    tracked=tracked,routing=routing,backup_identity=backup_identity,backup_base_release=backup_base_release,boot_id=boot_identity())
        yield Journal(directory,fd,lock,name,base,clock,recover_pending)
    finally:
        if lock is not None:os.close(lock)
        os.close(fd)
