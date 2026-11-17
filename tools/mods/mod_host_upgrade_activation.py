"""Private fixed-job startup after proof retirement; public admission stays separate."""
import hashlib
import os
import stat
import subprocess

import mod_host_cutover as cutover
import mod_host_cutover_journal as journal_code
import mod_host_upgrade_admission as admission
import mod_host_upgrade_process as process
from mod_host_migration import boot_identity
from mod_package import read_json, require

PROOF='upgrade-quiescence.json'


def fingerprint(data):return hashlib.sha256(data).hexdigest()


def archive_path(folder,generation,sha):
    require(generation in ('before','after') and isinstance(sha,str) and journal_code.SHA.fullmatch(sha),
            'Invalid retired proof identity')
    return folder/('retired-proof-'+generation+'-'+sha+'.json')


def _archive(journal,folder,generation,sha):
    path=archive_path(folder,generation,sha)
    try:data,identity=cutover.protected_bytes(path,0o600,8192)
    except ValueError:
        # A crash after rename but before chmod is safe inside the Root700
        # layout folder; only the exact pending retirement may tighten it.
        require(journal.state()['retire_pending']==dict(generation=generation,sha256=sha),
                'Preserving unknown retired proof permissions')
        data,identity=cutover.protected_bytes(path,0o644,8192)
    require(fingerprint(data)==sha,'Retired proof bytes differ from the durable intent')
    admission.checked_existing(journal,data)
    return data,identity


def retired_original_proof(journal,folder,generation):
    """Recognize only a recorded, retained proof archive; never infer deletion."""
    journal.state()
    for event in reversed(read_json(journal.current)['events']):
        if event['kind']=='proof-retire-intent' and event['label']==generation and event['data'] is not None:
            try:_archive(journal,folder,generation,event['data'])
            except FileNotFoundError:continue
            return True
    return False


def require_absent_proof():
    fd=cutover.protected_directory(cutover.ROOT/'monitor')
    try:
        try:os.stat(PROOF,dir_fd=fd,follow_symlinks=False)
        except FileNotFoundError:return
        raise ValueError('Migration proof must be retired before private service startup')
    finally:os.close(fd)


def retire_proof(journal,layout,gate,*,resume=False):
    """Archive the sole active worker authority under a durable Root intent."""
    state=journal.state();generation=state['config']
    require(type(resume) is bool and os.geteuid()==cutover.ROOT_UID and layout.journal is journal
            and journal.base['boot_id']==boot_identity() and state['layout_complete']==generation
            and not state['started'] and state['start_pending'] is None,'Invalid proof retirement context')
    gate();require(layout._locations()['active']==generation,'Active layout generation differs')
    pending=state['retire_pending']
    if pending is None and generation in state['retired_proofs']:
        require_absent_proof();return state['retired_proofs'][generation]
    require(pending is None or (resume and pending['generation']==generation),
            'Interrupted proof retirement requires explicit recovery')
    data,identity=admission._existing(journal)
    if pending is None:
        sha=fingerprint(data) if data is not None else None
        journal.record('proof-retire-intent',generation,sha)
    else:sha=pending['sha256']
    gate()
    if sha is None:require(data is None,'A proof appeared after the absence intent')
    else:
        path=archive_path(layout.folder,generation,sha)
        if data is not None:
            require(fingerprint(data)==sha,'Active proof changed after retirement intent')
            source=cutover.protected_directory(cutover.ROOT/'monitor');destination=cutover.protected_directory(layout.folder)
            try:
                try:os.stat(path.name,dir_fd=destination,follow_symlinks=False)
                except FileNotFoundError:pass
                else:raise ValueError('Preserving an existing retired proof destination')
                current,stamp=admission._existing(journal)
                require(current==data and stamp==identity,'Active proof changed before retirement')
                os.rename(PROOF,path.name,src_dir_fd=source,dst_dir_fd=destination)
                os.fsync(source);os.fsync(destination)
            finally:os.close(source);os.close(destination)
        # Retained bytes are verified before tightening permissions. An unknown
        # destination or replacement is never silently adopted or overwritten.
        archived,stamp=_archive(journal,layout.folder,generation,sha)
        fd=cutover.protected_directory(layout.folder)
        try:
            child=os.open(path.name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK,dir_fd=fd)
            with os.fdopen(child,'rb') as stream:
                require(cutover.stamp(os.fstat(stream.fileno()))==stamp and stream.read(8193)==archived,
                        'Retired proof changed before permission tightening')
                os.fchmod(stream.fileno(),0o600);os.fsync(stream.fileno())
            os.fsync(fd)
        finally:os.close(fd)
        _archive(journal,layout.folder,generation,sha)
    gate();require_absent_proof()
    journal.record('proof-retire-observed',generation,sha)
    return sha


def fixed_monitor_files(layout,generation):
    """Attest immutable markers/plists while monitors update separate health files."""
    manifest=read_json(layout.manifest)
    data,_=cutover.protected_bytes(layout.folder/'manifest.json',0o600,65536)
    require(data==layout.manifest,'Prepared layout manifest changed')
    baseline=manifest['monitor' if generation=='before' else 'after_monitor']
    expected=layout.before if generation=='before' else layout.after
    for relative in ('','api','publication','gateway'):
        fd=cutover.protected_directory(cutover.ROOT/'monitor'/relative)
        try:
            info=os.fstat(fd)
            require([info.st_dev,info.st_ino,info.st_uid,stat.S_IMODE(info.st_mode)]==baseline[relative],
                    'Active monitor directory identity changed')
        finally:os.close(fd)
    for name,wanted in expected.items():
        found,stamp=cutover.protected_bytes(cutover.ROOT/'monitor'/name,0o600,32768)
        require(list(stamp)==baseline[name] and (read_json(found)==read_json(wanted) if name.endswith('.json') else found==wanted),
                'Active monitor policy differs from the admitted generation')
    require(manifest['backup']==layout._backup(),'Preserved backup job identities changed')
    require_absent_proof()


def _observe_known(journal,entries,command):
    """Prove recorded labels and reject unrecorded labels or escaped UID workers."""
    state=journal.state();completed=set(state['started']);pending=state['start_pending']
    def snapshot():
        result={}
        for path,spec in entries:
            label=spec['Label'];observed=cutover.label_state(path,spec,command)
            require(observed is not None if label in completed else observed is None or label==pending,
                    'Private job presence differs from durable startup history')
            if observed is not None:result[label]=observed
        return result
    first=snapshot();code,output=command(['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
    require(code==0,'Cannot observe private startup processes')
    rows=cutover.process_rows(output)
    pids=[value['pid'] for value in first.values() if value['pid'] is not None]
    require(len(pids)==len(set(pids)),'Private jobs share a process identity')
    tracked=cutover.tracked_processes(set(pids),rows,journal.base['uid'])
    require(all(owner!=journal.base['uid'] or pid in tracked['pids'] for pid,(_,_,owner) in rows.items())
            and snapshot()==first,'Unrecorded service processes or changing startup identities')
    return first


def start_private_services(journal,current,layout,entries,command,routing,*,resume=False):
    """Load only fixed admitted labels, leaving public acceptance disabled.

    A pending bootstrap is observed on explicit resume, never blindly retried.
    Health/scanner readiness and restart acceptance must complete separately.
    """
    state=journal.state();generation=state['config'];base=journal.base
    require(type(resume) is bool and os.geteuid()==cutover.ROOT_UID and base['boot_id']==boot_identity()
            and layout.journal is journal and current['uid']==base['uid']
            and current['identity']==base[generation] and state['layout_complete']==generation
            and state['monitor']==generation and all(value==generation for value in state['boots'].values())
            and state['config_pending'] is None and state['layout_pending'] is None,
            'Invalid private startup context')
    cutover.validate_entries(entries)
    require(resume or (not state['started'] and state['start_pending'] is None),
            'Existing startup requires explicit recovery')
    from mod_host_upgrade_layout import quiescence_gate
    gate=lambda:quiescence_gate(journal,entries,command,routing)
    if generation not in state['retired_proofs'] or state['retire_pending'] is not None:
        retire_proof(journal,layout,gate,resume=resume)
    fixed_monitor_files(layout,generation);files=cutover.job_files(entries)
    journal.require_suspended(routing);_observe_known(journal,entries,command)
    state=journal.state()
    if state['scanner'] is None or not 0<=journal.clock()-state['scanner_time']<=300:
        result=process.scanner_check(current,base['uid'])
        process.checked_scanner_result(result,current['identity'],base['uid'])
        # The supervised helper and its confined scanner must be gone. A leaked
        # descendant outside the known launchd trees prevents further startup.
        _observe_known(journal,entries,command);fixed_monitor_files(layout,generation)
        cutover.job_files(entries,files);journal.require_suspended(routing)
        journal.record('scanner-verified',generation,result)
    by_label={spec['Label']:(path,spec) for path,spec in entries}
    for label in journal_code.start_order():
        state=journal.state()
        journal.require_suspended(routing);fixed_monitor_files(layout,generation);cutover.job_files(entries,files)
        observed=_observe_known(journal,entries,command)
        if label in state['started']:continue
        path,spec=by_label[label]
        if state['start_pending'] is not None:
            require(resume and state['start_pending']==label and label in observed,
                    'Pending bootstrap is not verified loaded; preserve it for explicit recovery')
            journal.record('start-observed',label,observed[label]);continue
        journal.record('start-intent',label,generation)
        try:command(['/bin/launchctl','bootstrap','system',str(path)])
        except (OSError,ValueError,subprocess.SubprocessError):pass
        loaded=cutover.label_state(path,spec,command)
        require(loaded is not None,'Private bootstrap outcome is unresolved; durable intent retained')
        require_absent_proof();journal.require_suspended(routing)
        journal.record('start-observed',label,loaded)
    fixed_monitor_files(layout,generation);cutover.job_files(entries,files)
    journal.require_suspended(routing);_observe_known(journal,entries,command)
    return dict(phase='private-jobs-loaded',release=current['identity'],generation=generation,jobs=10,
                scanner_verified=True,readiness_verified=False,public_endpoint_started=False)
