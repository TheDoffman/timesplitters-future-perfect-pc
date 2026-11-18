"""Admit one current stopped boot after exact layout and verified backup checks."""
import os

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_reboot_backup as backups
import mod_host_reboot_checkpoint as checkpoints
import mod_host_reboot_layout as layouts
import mod_host_upgrade_context as contexts
from mod_package import require


def admit(journal,fresh,*,resume=False):
    require(type(resume) is bool and os.geteuid()==cutover.ROOT_UID,'Choose explicit Root reboot admission')
    reboot=journal.state()['reboot']
    require(reboot is not None and reboot['admitted'] is None,'This boot has no pending reboot admission')
    checkpoints.seal(journal,fresh,resume=resume)
    report=backups.capture(journal,fresh,resume=resume)
    require(storage.wire(checkpoints.load(journal,report['checkpoint_sha256']))==storage.wire(layouts.read(journal,fresh)),
            'Fresh stopped reboot layout differs before continuation admission')
    metadata=contexts.read_reboot_context(journal,fresh)
    require(metadata['context_sha256']==journal.state()['reboot']['context_sha256'],'Original context changed before reboot admission')
    journal.record_reboot('reboot-admitted',data=dict(boot_id=report['boot_id'],checkpoint_sha256=report['checkpoint_sha256'],
        backup_sha256=report['receipt']['verified_manifest_sha256'],private_baseline=metadata['private_baseline']))
    storage.operational_boot(journal)
    return dict(schema=1,phase='reboot-admitted',boot_id=report['boot_id'],operational_admission=True,
                services_started=False,public_endpoint_started=False)
