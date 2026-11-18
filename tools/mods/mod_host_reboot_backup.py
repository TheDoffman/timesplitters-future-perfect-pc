"""Force a current verified backup after reboot layout/quiescence admission.

Only the locked child reads repository/backups. Ordinary migration, startup and
settlement remain unavailable through the reboot observation lease.
"""
import os

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_reboot_checkpoint as checkpoints
import mod_host_reboot_layout as layouts
import mod_host_upgrade_process as process
from mod_package import require


def capture(journal,fresh,*,resume=False):
    require(type(resume) is bool and os.geteuid()==cutover.ROOT_UID,'Choose explicit Root reboot backup recovery')
    binding=checkpoints.selection(journal);state=journal.state()['reboot'];base=journal.base
    require(state['layout_checkpoint'] is not None,'Verify the reboot layout checkpoint before capturing')
    runtime,current=fresh['backup_runtime'],fresh['new'];backup=fresh['backup_binding']
    require(runtime['identity']==base['backup_base_release'] and current['identity']==base['after']
            and runtime['uid']==current['uid']==base['uid'] and backup['uid']==backup['gid']==base['uid']
            and backup['base_release']==runtime['identity'] and backup['python']==str(runtime['python']),
            'Preserve the independently verified backup runtime and binding')
    require(resume or (state['backup_pending'] is None and state['backup'] is None),
            'An existing reboot backup requires explicit recovery')
    sha=state['layout_checkpoint']['checkpoint_sha256']
    original=checkpoints.load(journal,sha)
    def guard():
        require(checkpoints.selection(journal)==binding and checkpoints.load(journal,sha)==original
                and storage.wire(layouts.read(journal,fresh))==storage.wire(original),
                'Reboot checkpoint/layout/quiescence changed around backup capture')
    guard();state=journal.state()['reboot']
    if state['backup_pending'] is None and state['backup'] is not None and 0<=journal.clock()-state['backup']['finished']<=300:
        report=process.checked_backup_receipt(state['backup'],base['backup_base_release'],base['uid'],base['backup_identity'],
            state['quiesced_at'],clock=journal.clock)
        return dict(schema=1,phase='reboot-backup-verified',boot_id=binding['boot_id'],checkpoint_sha256=sha,
                    receipt=report,reused=True,operational_admission=False)
    if state['backup_pending'] is None:journal.record_reboot('reboot-backup-intent',data=dict(checkpoint_sha256=sha))
    # On explicit recovery of an uncertain capture, a new independently verified
    # snapshot is requested only after fresh absence/quiescence checks. The fixed
    # child transport reaps its owned child before returning or raising; an old
    # still-running UID600 capture prevents guard() from admitting another one.
    attempted=int(journal.clock())
    report=process.forced_backup(runtime,current,base['uid'],base['backup_identity'],quiesced_at=state['quiesced_at'])
    process.checked_backup_receipt(report,base['backup_base_release'],base['uid'],base['backup_identity'],attempted,clock=journal.clock)
    require(0<=journal.clock()-report['finished']<=300,'Fresh reboot backup verification is stale')
    guard()
    journal.record_reboot('reboot-backup-verified',data=dict(checkpoint_sha256=sha,receipt=report))
    return dict(schema=1,phase='reboot-backup-verified',boot_id=binding['boot_id'],checkpoint_sha256=sha,
                receipt=report,reused=False,operational_admission=False)
