"""Fresh backup-to-migration admission, after journaled routing/job quiescence.

The installed coordinator verifies immutable code/runtime/bindings first. Root
opens only metadata here; the supervised locked child reads repository/backups.
This module does not activate services or change private configuration.
"""
import os

import mod_host_cutover as cutover
import mod_host_migration as migration
import mod_host_upgrade_process as process
from mod_package import read_json,require

NAME='upgrade-quiescence.json'
PRODUCER='tsfp-upgrade-quiescence-v1'


def _existing(journal):
    """Only this transaction's previously recorded backup proofs may refresh."""
    path=cutover.ROOT/'monitor'/NAME
    try:data,identity=cutover.protected_bytes(path,0o644,8192)
    except FileNotFoundError:return None,None
    checked_existing(journal,data)
    return data,identity


def checked_existing(journal,data):
    """Validate active or archived proof metadata against this root history."""
    value=read_json(data);base=journal.base
    fields={'schema','producer','before','after','uid','nonce','plan_sha256','backup_sha256','time',
            'stopped_jobs','quiesced_at','backup_verified_at','boot_id'}
    history=read_json(journal.current)['events']
    captures=[entry['data'] for entry in history if entry['kind']=='backup-verified']
    quiesced={entry['time'] for entry in history if entry['kind']=='quiescent'}
    require(isinstance(value,dict) and set(value)==fields and type(value['schema']) is int and value['schema']==1
            and value['producer']==PRODUCER and all(type(value[k]) is type(base[k]) and value[k]==base[k]
                for k in ('before','after','uid','nonce','plan_sha256','boot_id'))
            and value['stopped_jobs']==migration.jobs()
            and all(type(value[k]) is int for k in ('time','quiesced_at','backup_verified_at'))
            and 0<=value['quiesced_at']<=value['backup_verified_at']<=value['time']<=journal.clock()
            and value['quiesced_at'] in quiesced and any(record['verified_manifest_sha256']==value['backup_sha256']
                and record['finished']==value['backup_verified_at'] for record in captures),
            'Preserving an unknown or foreign migration admission')
    return value


def _publish(journal,value,previous,identity):
    directory=cutover.ROOT/'monitor';fd=cutover.protected_directory(directory)
    temporary='.upgrade-quiescence-'+journal.base['nonce']+'.pending'
    try:
        # New only. An interrupted/unrecognized proof write requires explicit
        # recovery, never deletion or replacement merely to make admission pass.
        child=os.open(temporary,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
        with os.fdopen(child,'wb') as output:
            output.write(migration.wire(value));output.flush();os.fchmod(output.fileno(),0o644);os.fsync(output.fileno())
        current,stamp=_existing(journal)
        require(current==previous and stamp==identity,'Migration admission changed before commit')
        visible=cutover.protected_directory(directory)
        try:
            require((os.fstat(visible).st_dev,os.fstat(visible).st_ino)==(os.fstat(fd).st_dev,os.fstat(fd).st_ino),
                    'Migration admission directory was replaced')
        finally:os.close(visible)
        os.replace(temporary,NAME,src_dir_fd=fd,dst_dir_fd=fd);os.fsync(fd)
    finally:os.close(fd)


def capture_and_admit(journal,entries,runtime,current,profile,plan,routing,command):
    """Force a fresh locked capture, recheck quiescence, then issue short authority.

    The caller holds the root journal lease and supplies already sealed closure
    objects. No public CLI exposes this phase until activation/rollback completes.
    """
    base=journal.base;uid=base['uid']
    require(os.geteuid()==cutover.ROOT_UID and base['boot_id']==migration.boot_identity()
            and runtime['identity']==base['backup_base_release'] and current['identity']==base['after']
            and runtime['uid']==current['uid']==uid and profile['origin']==base['routing']['origin'],
            'Invalid root backup/admission context')
    process.checked_migration_plan(plan,base['before'],base['after'],profile)
    require(plan['sha256']==base['plan_sha256'],'Migration plan differs from the root transaction')
    previous,identity=_existing(journal)
    journal.require_suspended(routing)
    stopped=cutover.quiesce(entries,uid,journal,command,routing=routing)
    require(stopped['quiescent'] and type(stopped['quiesced_at']) is int,'No observed quiescence for recovery capture')
    report=process.forced_backup(runtime,current,uid,base['backup_identity'],quiesced_at=stopped['quiesced_at'])
    process.checked_backup_receipt(report,base['backup_base_release'],uid,base['backup_identity'],
                                   stopped['quiesced_at'],clock=journal.clock)
    journal.record('backup-verified',data=report)
    # The capture child itself used UID600. It must be reaped, and no worker or
    # escaped descendant may have reappeared while the repository was captured.
    state=cutover.quiesce(entries,uid,journal,command,routing=routing)
    journal.require_suspended(routing)
    require(state['backup']==report and state['quiescent'] and state['checked_at']>=report['finished']
            and 0<=journal.clock()-report['finished']<=300,'Fresh post-capture quiescence is required')
    value=dict(schema=1,producer=PRODUCER,before=base['before'],after=base['after'],uid=uid,nonce=base['nonce'],
               plan_sha256=base['plan_sha256'],backup_sha256=report['verified_manifest_sha256'],time=int(journal.clock()),
               stopped_jobs=migration.jobs(),quiesced_at=state['quiesced_at'],backup_verified_at=report['finished'],boot_id=base['boot_id'])
    _publish(journal,value,previous,identity)
    payload,_=cutover.protected_bytes(cutover.ROOT/'monitor'/NAME,0o644,8192)
    require(payload==migration.wire(value),'Migration admission changed after commit')
    return value
