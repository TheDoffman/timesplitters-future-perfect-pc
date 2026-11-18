"""Suspend public routing and quiesce fresh fixed jobs under reboot history.

This phase does not admit migration, start services or settle an upgrade. The
immutable caller verifies closures and owns the explicit observation lease.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import time

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_reboot_observation as observation
import mod_host_routing as routes
import mod_host_upgrade_context as contexts
from mod_host_migration import boot_identity
from mod_package import require


def prepare(journal,fresh,*,resume=False,clock=time.monotonic,sleep=time.sleep,timeout=60):
    require(type(resume) is bool and type(timeout) is int and 1<=timeout<=120 and os.geteuid()==cutover.ROOT_UID
            and journal.observation_boot==boot_identity() and journal.observation_boot!=journal.base['boot_id'],
            'Use explicit current-boot Root preparation')
    base=journal.base;uid=base['uid'];old,new=fresh['old'],fresh['new']
    require(old['identity']==base['before'] and new['identity']==base['after'] and old['uid']==new['uid']==uid
            and old['root']==cutover.ROOT/'releases'/base['before'] and new['root']==cutover.ROOT/'releases'/base['after'],
            'Reboot preparation changes the verified installed pair')
    metadata=contexts.read_reboot_context(journal,fresh)
    command,routing=fresh['command'],fresh['operator']
    state=journal.state()['reboot']
    if state is None or state['boot_id']!=journal.observation_boot:
        require(state is None or resume,'A further restart requires explicit reboot recovery')
        prior=base['boot_id'] if state is None else state['boot_id']
        snapshot=observation.observe(fresh['entries'],fresh['candidate_entries'],uid,prior,command)
        journal.record_reboot('reboot-observed',data=dict(context_sha256=metadata['context_sha256'],observation=snapshot))
    else:
        require(resume and state['context_sha256']==metadata['context_sha256'],'Existing reboot preparation requires explicit recovery')
    snapshot=journal.state()['reboot']['observation']
    choices=observation.policies(fresh['entries'],fresh['candidate_entries'])
    def files():
        require(journal.observation_boot==boot_identity() and journal.state()['reboot']['boot_id']==journal.observation_boot,
                'Boot changed during reboot preparation')
        require(storage.wire(observation.files(choices))==storage.wire(snapshot['files']),
                'Protected reboot boot/backup files changed')
        for path,expected in snapshot['observation']['generated_files'].items():
            mode=0o444 if Path(path).parent.parent==cutover.ROOT/'backup-jobs' else 0o600
            data,stamp=cutover.protected_bytes(Path(path),mode,32768)
            require(hashlib.sha256(data).hexdigest()==expected['sha256'] and list(stamp)==expected['stamp'],
                    'Loaded reboot generated origin changed')
    def off():
        files();state=journal.state()['reboot']
        current=routes.snapshot(routing('status'),base['routing']['origin'])
        require(state['routing_suspended'] and state['routing_pending'] is None and current['phase']!='public'
                and current['unrelated_sha256']==base['routing']['unrelated_sha256'],
                'Reboot public suspension is not currently verified')
    def route_record(kind,data):
        state=journal.state()['reboot']
        if kind=='routing-intent' and state['routing_pending'] is False:return
        journal.record_reboot('reboot-'+kind,data=data)
    files();state=journal.state()['reboot']
    if not state['routing_suspended'] or state['routing_pending'] is not None:
        current=routes.snapshot(routing('status'),base['routing']['origin'])
        require(current['unrelated_sha256']==base['routing']['unrelated_sha256'],'Unrelated routing changed after reboot')
        if current['phase']!='public':journal.record_reboot('reboot-routing-observed',data=dict(current,expose=False))
        else:routes.transition(base['routing'],False,routing,route_record,resume=resume)
    off()
    def loaded(label):
        found=observation.loaded({label:choices[label]},snapshot['files'],command)['jobs'][label]
        expected=snapshot['observation']['jobs'][label]
        require(found is None or storage.wire(found)==storage.wire(expected),'Reboot job changed from its fresh observation')
        return found
    for label in storage.stop_order():
        off();state=journal.state()['reboot'];current=loaded(label)
        if label in state['stopped']:
            require(current is None,'A stopped reboot job reappeared');continue
        require(state['pending'] in (None,label),'Another reboot stop remains unresolved')
        new_stop=state['pending'] is None
        if new_stop:journal.record_reboot('reboot-stop-intent',label)
        if new_stop and current is not None:
            try:command(['/bin/launchctl','bootout','system/'+label])
            except (OSError,ValueError,subprocess.SubprocessError):pass
        deadline=clock()+timeout
        while loaded(label) is not None:
            off();require(clock()<deadline,'Reboot stop remains unconfirmed; preserving its intent');sleep(.1)
        off();journal.record_reboot('reboot-stop-observed',label)
    deadline=clock()+timeout
    while True:
        off();require(all(loaded(label) is None for label in storage.stop_order()),'A reboot job reappeared before cleanup')
        code,output=command(['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
        require(code==0,'Cannot observe fresh reboot descendant exit')
        if cutover.processes_gone(snapshot['tracked'],cutover.process_rows(output),uid):break
        require(clock()<deadline,'Fresh reboot descendants or dedicated-UID processes remain');sleep(.1)
    off();require(all(loaded(label) is None for label in storage.stop_order()),'A reboot label changed after cleanup')
    require(contexts.read_reboot_context(journal,fresh)['context_sha256']==metadata['context_sha256'],
            'Original recovery context changed during reboot preparation')
    if not journal.state()['reboot']['quiescent']:journal.record_reboot('reboot-quiescent')
    return dict(schema=1,phase='reboot-services-quiescent',boot_id=journal.observation_boot,original_boot=base['boot_id'],
        jobs=10,operational_admission=False,public_endpoint_started=False)
