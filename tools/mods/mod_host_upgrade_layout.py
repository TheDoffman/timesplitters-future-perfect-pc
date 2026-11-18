"""Protected monitor/boot-file exchange; caller journals intent and gates effects.

Only sealed job factories may supply entries. No CLI uses this internal phase
until configuration migration, activation and recovery are integrated.
"""
import hashlib
from itertools import islice
import os
from pathlib import Path
import plistlib
import stat

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
from mod_host_migration import boot_identity, wire
from mod_package import read_json, require

PRODUCER='tsfp-protected-layout-v1'


def digest(data):return hashlib.sha256(data).hexdigest()


def quiescence_gate(journal,entries,command,routing):
    """Re-observe absence without requiring all boot files to share a generation.

    During exchange the boot files are deliberately a known before/after mix.
    Layout verifies those bytes; this gate proves no process can consume them.
    """
    require(os.geteuid()==cutover.ROOT_UID and storage.operational_boot(journal)==boot_identity()
            and journal.state()['quiescent'],'Protected exchange lacks current root shutdown authority')
    cutover.validate_entries(entries);journal.require_suspended(routing)
    def absent():
        for path,spec in entries:
            require(cutover.label_state(path,spec,command) is None,'A fixed job reappeared during protected exchange')
    absent()
    code,output=command(['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
    require(code==0 and cutover.processes_gone(storage.operational_tracking(journal),cutover.process_rows(output),journal.base['uid']),
            'Service processes reappeared during protected exchange')
    absent();journal.require_suspended(routing)


def configuration_and_exchange(journal,current,layout,direction,gate,*,resume=False):
    """Journal-bind the locked migration and actual protected filesystem effects.

    Installed entry point must verify sealed closures, factories, backup and
    fresh proof first. A stale proof is refused by the locked migration worker;
    callers may explicitly refresh it from another verified quiescent capture.
    No activation or public routing change occurs here.
    """
    import mod_host_upgrade_process as process
    base=journal.base
    require(direction in ('apply','rollback') and type(resume) is bool and layout.journal is journal
            and current['identity']==base['after'] and current['uid']==base['uid']
            and os.geteuid()==cutover.ROOT_UID and storage.operational_boot(journal)==boot_identity(),
            'Invalid journaled configuration/layout context')
    gate();require(layout.manifest is not None,'A complete prepared layout is required')
    fingerprint=digest(layout.manifest);state=journal.state()
    if state['layout_sha256'] is None:journal.record('layout-prepared',data=fingerprint)
    else:require(state['layout_sha256']==fingerprint,'Prepared layout differs from the root journal')
    layout.reopen(fingerprint);layout._locations()
    state=journal.state()
    require(resume or (state['config_pending'] is None and state['layout_pending'] is None),
            'Interrupted upgrade requires explicit recovery')
    def migrate(action):
        state=journal.state();target='after' if action=='apply' else 'before'
        require(state['config_pending'] in (None,action),'Another private migration requires explicit resolution')
        if state['config']==target and state['config_pending'] is None:return
        gate();layout._locations()
        if state['config_pending'] is None:journal.record('migration-intent',action,base['plan_sha256'])
        result=process.migration_step(current,base['uid'],base['before'],base['nonce'],base['plan_sha256'],rollback=action=='rollback')
        process.checked_migration_result(result,base['before'],base['after'],base['plan_sha256'],rollback=action=='rollback')
        gate();journal.record('migration-observed',action,result)
    if direction=='apply':migrate('apply')
    elif state['config_pending']=='apply':
        require(resume,'An interrupted private apply requires explicit recovery')
        migrate('apply')
    state=journal.state()
    require(state['layout_pending'] is None or state['layout_pending'].startswith(direction+'-'),
            'Resolve the pending protected effect in its original direction first')
    cancel=direction=='rollback' and state['config']=='before' and state['layout_complete']=='before' and state['layout_pending'] is None
    result=layout.exchange(direction,journal.layout_event,gate,resume=resume or cancel,pending=state['layout_pending'])
    target='after' if direction=='apply' else 'before'
    if journal.state()['layout_complete']!=target:journal.record('layout-complete',direction,result)
    if direction=='rollback':migrate('rollback')
    return dict(phase='configuration-and-layout-'+('applied' if direction=='apply' else 'rolled-back'),
                plan_sha256=base['plan_sha256'],layout_sha256=fingerprint,services_started=False)


def payloads(entries,release,uid):
    """Derive exactly nine plists and four markers; backup is deliberately absent."""
    cutover.validate_entries(entries)
    require(type(uid) is int and uid>=600 and release.parent==cutover.ROOT/'releases'
            and len(release.name)==64 and all(c in '0123456789abcdef' for c in release.name),
            'Invalid protected monitor generation')
    result={}
    for path,spec in entries:
        role=spec['Label'].split('.')[3]
        if role=='backup':continue
        require(spec['ProgramArguments'][0]==str(release/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12'),
                'Monitor factory runtime differs from its verified generation')
        relative=path.relative_to(cutover.ROOT/'monitor').as_posix()
        data=plistlib.dumps(spec);require(0<len(data)<=32768,'Protected job exceeds budget')
        result[relative]=data
        if spec['Label'].endswith('.monitor'):
            marker=dict(schema=1,producer='tsfp-system-scan-monitor-v1' if role=='scan' else 'root-system-host-v1',
                        uid=uid,release=str(release),service=spec['Label'].removesuffix('.monitor'))
            if role!='scan':marker['role']=role
            result[str(Path(relative).with_name('monitor.json')).replace('\\','/')]=wire(marker)
    require(len(result)==13,'Protected monitor inventory differs')
    return result


def snapshot(path):
    """Bounded identity-only inventory; never disclose archived metadata contents.

    Root health/deployment history stays in the original directory. Unknown
    root-owned regular records are preserved as evidence, never executed/copied.
    Links, devices, writable objects and excessive inventories are refused.
    """
    result={};total=0
    def walk(directory,relative,depth):
        nonlocal total
        require(depth<=4,'Monitor history nesting exceeds budget')
        fd=cutover.protected_directory(directory)
        try:
            info=os.fstat(fd)
            require(stat.S_IMODE(info.st_mode) in (0o700,0o755),'Unsafe monitor directory mode')
            # Renaming the root changes ctime; identity/owner/mode remain bound.
            result[relative]=[info.st_dev,info.st_ino,info.st_uid,stat.S_IMODE(info.st_mode)]
            with os.scandir(fd) as stream:names=sorted(entry.name for entry in islice(stream,129))
            require(len(names)<=128,'Monitor history inventory exceeds budget')
            for name in names:
                require(len(result)<128,'Monitor history inventory exceeds budget')
                child=directory/name;key=(relative+'/'+name).lstrip('/')
                item=os.stat(name,dir_fd=fd,follow_symlinks=False)
                require(item.st_uid==cutover.ROOT_UID and not item.st_mode&0o022,'Unsafe monitor history owner/mode')
                if stat.S_ISDIR(item.st_mode):walk(child,key,depth+1)
                else:
                    require(stat.S_ISREG(item.st_mode) and item.st_nlink==1 and stat.S_IMODE(item.st_mode) in (0o600,0o644)
                            and item.st_size<=65536,'Unsafe monitor history file')
                    total+=item.st_size;require(total<=1024*1024,'Monitor history bytes exceed budget')
                    result[key]=list(cutover.stamp(item))
            visible=cutover.protected_directory(directory)
            try:require(os.fstat(visible).st_ino==info.st_ino and os.fstat(visible).st_dev==info.st_dev,
                        'Monitor history directory was replaced')
            finally:os.close(visible)
        finally:os.close(fd)
    walk(path,'',0)
    return result


def _new(directory,name,data,mode):
    fd=cutover.protected_directory(directory)
    try:
        child=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,mode,dir_fd=fd)
        with os.fdopen(child,'wb') as stream:
            stream.write(data);stream.flush();os.fchmod(stream.fileno(),mode);os.fsync(stream.fileno())
        os.fsync(fd)
    finally:os.close(fd)


def _check_payloads(directory,expected,*,allow_proof=False,history=None):
    observed=snapshot(directory)
    directories={'','api','publication','gateway'}
    extra={'upgrade-quiescence.json'} if allow_proof and 'upgrade-quiescence.json' in observed else set()
    require(set(observed)==(set(history) if history is not None else directories|set(expected))|extra,'Staged monitor inventory differs')
    require(all(observed[name][-1]==0o755 for name in directories),'Staged monitor directory mode differs')
    for name,data in expected.items():
        found,_=cutover.protected_bytes(directory/name,0o600,32768)
        require(found==data,'Staged monitor bytes differ from immutable policy')
    return observed


class Layout:
    """Fixed eleven-effect exchange with explicit interrupted-effect recovery.

    Caller holds the cutover journal lease, checks migration completion, and
    supplies a durable record callback and live quiescence gate. Effects never
    bootstrap jobs. Partial/interrupted files are preserved rather than repaired.
    """
    def __init__(self,journal,before,after,old_entries,new_entries,*,observation=False):
        self.journal=journal;self.old=old_entries;self.new=new_entries
        base=journal.base;uid=base['uid']
        require(type(observation) is bool,'Choose explicit protected-layout observation')
        current=boot_identity()
        reboot=(journal.state().get('reboot') or {}) if observation else {}
        observing=(observation and getattr(journal,'observation_boot',None)==current and current!=base['boot_id']
            and reboot.get('boot_id')==current and reboot.get('quiescent') is True)
        require(os.geteuid()==cutover.ROOT_UID
                and (observing or (not observation and storage.operational_boot(journal)==current))
                and before==cutover.ROOT/'releases'/base['before'] and after==cutover.ROOT/'releases'/base['after'],
                'Invalid protected layout context')
        self.observation_only=observation
        self.before=payloads(old_entries,before,uid);self.after=payloads(new_entries,after,uid)
        self.boot={}
        for entries,side in ((old_entries,'before'),(new_entries,'after')):
            for path,spec in entries:
                label=spec['Label']
                if label.endswith('.backup'):continue
                self.boot.setdefault(label,{})[side]=plistlib.dumps(spec)
        backup=lambda entries:next(item for item in entries if item[1]['Label'].endswith('.backup'))
        require(backup(old_entries)==backup(new_entries),'Preserved backup binding changed')
        path,spec=backup(old_entries)
        self.backup_paths=((path,0o444),(cutover.LAUNCHD/(spec['Label']+'.plist'),0o644))
        self.backup_bytes=plistlib.dumps(spec)
        self.folder=cutover.ROOT/'upgrade-layouts'/base['nonce']
        self.staged=self.folder/'monitor-after';self.saved=self.folder/'monitor-before'
        self.manifest=None

    def _backup(self):
        result={}
        for path,mode in self.backup_paths:
            data,stamp=cutover.protected_bytes(path,mode,32768)
            require(data==self.backup_bytes,'Preserved backup job changed')
            result[str(path)]=dict(sha256=digest(data),stamp=list(stamp))
        return result

    def prepare(self):
        """New-only protected staging, without changing any active or boot path."""
        require(not self.observation_only,'Observation layout cannot prepare protected files')
        require(self.journal.state()['quiescent'],'Protected layout requires observed shutdown')
        cutover.job_files(self.old)
        for name,data in self.before.items():
            observed,_=cutover.protected_bytes(cutover.ROOT/'monitor'/name,0o600,32768)
            if name.endswith('.json'):require(read_json(observed)==read_json(data),'Old monitor generation differs')
            else:require(observed==data,'Old protected job differs')
        inventory=snapshot(cutover.ROOT/'monitor')
        root=cutover.protected_directory(cutover.ROOT)
        try:
            try:os.mkdir('upgrade-layouts',0o700,dir_fd=root);os.fsync(root)
            except FileExistsError:pass
        finally:os.close(root)
        parent=cutover.protected_directory(self.folder.parent)
        try:
            require(stat.S_IMODE(os.fstat(parent).st_mode)==0o700,'Unsafe protected layout store')
            with os.scandir(parent) as stream:names=[entry.name for entry in islice(stream,33)]
            require(len(names)<32 and all(len(name)==64 and all(c in '0123456789abcdef' for c in name) for name in names),
                    'Preserving unknown or full protected layout storage')
            os.mkdir(self.folder.name,0o700,dir_fd=parent);os.fsync(parent)
        finally:os.close(parent)
        self.staged.mkdir(mode=0o755);self.staged.chmod(0o755)
        for role in ('api','publication','gateway'):
            (self.staged/role).mkdir(mode=0o755);(self.staged/role).chmod(0o755)
        for name,data in self.after.items():_new(self.staged/Path(name).parent,Path(name).name,data,0o600)
        staged_inventory=_check_payloads(self.staged,self.after)
        require(snapshot(cutover.ROOT/'monitor')==inventory,'Old monitor changed during staging')
        self.manifest=wire(dict(schema=1,producer=PRODUCER,nonce=self.journal.base['nonce'],
            before=self.journal.base['before'],after=self.journal.base['after'],uid=self.journal.base['uid'],
            boot_id=self.journal.base['boot_id'],monitor=inventory,after_monitor=staged_inventory,backup=self._backup(),
            payloads={name:digest(data) for name,data in self.after.items()},
            boot={label:{side:digest(data) for side,data in versions.items()} for label,versions in self.boot.items()}))
        _new(self.folder,'manifest.json',self.manifest,0o600)
        return digest(self.manifest)

    def reopen(self,expected):
        """Read the prepared manifest bound to the original journal and boot."""
        require(isinstance(expected,str) and len(expected)==64,'An exact prepared layout digest is required')
        data,_=cutover.protected_bytes(self.folder/'manifest.json',0o600,65536)
        value=read_json(data);base=self.journal.base
        require(digest(data)==expected and data==wire(value) and set(value)=={'schema','producer','nonce','before','after','uid',
                'boot_id','monitor','after_monitor','backup','payloads','boot'} and type(value['schema']) is int and value['schema']==1
                and value['producer']==PRODUCER and all(type(value[k]) is type(base[k]) and value[k]==base[k]
                    for k in ('nonce','before','after','uid','boot_id'))
                and value['payloads']=={name:digest(payload) for name,payload in self.after.items()}
                and value['boot']=={label:{side:digest(payload) for side,payload in versions.items()} for label,versions in self.boot.items()},
                'Preserving an unknown protected layout manifest')
        self.manifest=data

    def _locations(self):
        require(self.manifest is not None,'Prepare or explicitly reopen a protected layout first')
        data,_=cutover.protected_bytes(self.folder/'manifest.json',0o600,65536)
        require(data==self.manifest,'Protected layout manifest changed')
        manifest=read_json(data);original=manifest['monitor'];found={}
        candidates=dict(before=original,after=manifest['after_monitor'])
        reboot=self.journal.state().get('reboot')
        if reboot is not None and reboot['admitted'] is not None:
            from mod_host_reboot_checkpoint import load
            checkpoint=load(self.journal,reboot['layout_checkpoint']['checkpoint_sha256'])
            for name,generation in checkpoint['locations'].items():
                if generation is not None:candidates[generation]=checkpoint['inventories'][name]
        checkpoints=self.journal.state().get('monitor_checkpoints',{})
        if checkpoints:
            from mod_host_upgrade_shutdown import read_checkpoint
            for generation,sha in checkpoints.items():candidates[generation]=read_checkpoint(self,generation,sha)
        require(manifest['backup']==self._backup(),'Preserved backup job identities changed')
        for name,path in (('active',cutover.ROOT/'monitor'),('saved',self.saved),('staged',self.staged)):
            try:observed=snapshot(path)
            except FileNotFoundError:found[name]=None;continue
            stable=lambda value:{key:item for key,item in value.items() if key!='upgrade-quiescence.json'}
            if 'upgrade-quiescence.json' in observed:
                from mod_host_upgrade_admission import checked_existing
                checked_existing(self.journal,cutover.protected_bytes(path/'upgrade-quiescence.json',0o644,8192)[0])
            matches=[generation for generation,value in candidates.items() if stable(observed)==stable(value)]
            require(len(matches)==1,'Preserving changed protected monitor evidence')
            generation=matches[0]
            missing_original=generation=='before' and 'upgrade-quiescence.json' in original and 'upgrade-quiescence.json' not in observed
            if missing_original:
                from mod_host_upgrade_activation import retired_original_proof
                require(retired_original_proof(self.journal,self.folder,'before'),'Preserving a missing original migration proof')
            if generation=='after':_check_payloads(path,self.after,allow_proof=True,history=candidates['after'])
            found[name]=generation
        require(sorted(v for v in found.values() if v is not None)==['after','before']
                and found['saved'] in (None,'before') and found['staged'] in (None,'after'),
                'Preserving unknown or duplicate protected monitor generations')
        return found

    def exchange(self,direction,record,gate,*,resume=False,pending=None):
        """Durably announce each fixed effect, then inspect outcome before return.

        `record(kind, operation, digest)` must commit to the coordinator journal
        before returning. `gate()` must re-observe suspended ingress, all ten
        absent jobs and descendant/UID quiescence before every effect.
        """
        require(not self.observation_only,'Observation layout cannot exchange protected files')
        require(direction in ('apply','rollback') and type(resume) is bool,'Invalid protected exchange direction')
        operations={direction+'-archive',direction+'-install',*(direction+'-'+label for label in self.boot)}
        require(pending is None or (resume and pending in operations),'Unknown protected exchange intent')
        target='after' if direction=='apply' else 'before';source='before' if direction=='apply' else 'after'
        locations=self._locations();already=locations['active']==target
        require(resume or (locations['active']==source and locations['saved' if direction=='apply' else 'staged'] is None),
                'Interrupted protected exchange requires explicit recovery')
        for label,versions in self.boot.items():
            data,_=cutover.protected_bytes(cutover.LAUNCHD/(label+'.plist'),0o644,32768)
            require(data in versions.values(),'Preserving a foreign protected boot file')
            if not resume:require(data==versions[source],'Mixed protected boot files require explicit recovery')
        if pending is not None:
            if pending==direction+'-archive':done=locations['saved' if direction=='apply' else 'staged']==source
            elif pending==direction+'-install':done=locations['active']==target
            else:
                label=pending.removeprefix(direction+'-')
                done=cutover.protected_bytes(cutover.LAUNCHD/(label+'.plist'),0o644,32768)[0]==self.boot[label][target]
            if done:
                gate();record('observed',pending,digest(self.manifest));pending=None
        def announce(kind,operation,fingerprint):
            if kind=='intent' and operation==pending:return
            record(kind,operation,fingerprint)
        if not already:
            if locations['active'] is not None:
                destination=self.saved if direction=='apply' else self.staged
                self._move(cutover.ROOT/'monitor',destination,direction+'-archive',announce,gate)
            origin=self.staged if direction=='apply' else self.saved
            self._move(origin,cutover.ROOT/'monitor',direction+'-install',announce,gate)
        for label,versions in sorted(self.boot.items()):
            path=cutover.LAUNCHD/(label+'.plist');data,identity=cutover.protected_bytes(path,0o644,32768)
            if data==versions[target]:continue
            gate();announce('intent',direction+'-'+label,digest(self.manifest))
            current,stamp=cutover.protected_bytes(path,0o644,32768)
            require(current==data and stamp==identity,'Protected boot file changed before exchange')
            temporary='.'+label+'.upgrade-'+self.journal.base['nonce']
            _new(cutover.LAUNCHD,temporary,versions[target],0o644)
            self._locations()
            current,stamp=cutover.protected_bytes(path,0o644,32768)
            require(current==data and stamp==identity,'Protected boot file changed during staging')
            fd=cutover.protected_directory(cutover.LAUNCHD)
            try:
                os.replace(temporary,path.name,src_dir_fd=fd,dst_dir_fd=fd);os.fsync(fd)
            finally:os.close(fd)
            require(cutover.protected_bytes(path,0o644,32768)[0]==versions[target],'Protected boot exchange differs')
            record('observed',direction+'-'+label,digest(self.manifest))
        gate();require(self._locations()['active']==target,'Protected monitor exchange differs')
        return dict(phase='protected-layout-'+('applied' if direction=='apply' else 'rolled-back'),
                    layout_sha256=digest(self.manifest),boot_files=9,backup_changed=False,services_started=False)

    def _move(self,source,destination,operation,record,gate):
        require(not self.observation_only,'Observation layout cannot move protected files')
        gate();record('intent',operation,digest(self.manifest));self._locations()
        first=cutover.protected_directory(source.parent);second=cutover.protected_directory(destination.parent)
        try:
            try:os.stat(destination.name,dir_fd=second,follow_symlinks=False)
            except FileNotFoundError:pass
            else:raise ValueError('Preserving an existing monitor exchange destination')
            os.rename(source.name,destination.name,src_dir_fd=first,dst_dir_fd=second)
            os.fsync(first);os.fsync(second)
        finally:os.close(first);os.close(second)
        self._locations();record('observed',operation,digest(self.manifest))
