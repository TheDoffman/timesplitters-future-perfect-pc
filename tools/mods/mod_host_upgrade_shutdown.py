"""Stop recorded private-generation jobs and retain bounded monitor history."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import time

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_upgrade_activation as activation
from mod_host_migration import boot_identity,wire
from mod_package import read_json,require

PRODUCER='tsfp-monitor-health-checkpoint-v1'
HEALTH={'scan.json','api/health.json','publication/health.json','gateway/health.json'}


def stop_private_services(journal,layout,entries,command,routing,*,resume=False,clock=time.monotonic,sleep=time.sleep,timeout=60):
    """Unload fixed labels once per intent, then prove all descendants/UID gone."""
    state=journal.state();base=journal.base;generation=state['config']
    require(type(resume) is bool and type(timeout) is int and 1<=timeout<=120
            and os.geteuid()==cutover.ROOT_UID and storage.operational_boot(journal)==boot_identity() and layout.journal is journal
            and state['layout_complete']==generation==state['monitor'],'Invalid private shutdown context')
    cutover.validate_entries(entries);activation.fixed_monitor_files(layout,generation)
    files=cutover.job_files(entries);journal.require_suspended(routing)
    by_label={spec['Label']:(path,spec) for path,spec in entries}
    if state['shutdown'] is None:
        completed=set(state['started']);pending=state['start_pending']
        def observe():
            result={}
            for path,spec in entries:
                label=spec['Label'];value=cutover.label_state(path,spec,command)
                require(value is not None if label in completed else value is None or label==pending,
                        'Shutdown labels differ from recorded startup')
                if value is not None:result[label]=value
            return result
        first=observe();code,output=command(['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
        require(code==0,'Cannot bind private shutdown process trees')
        rows=cutover.process_rows(output);roots=[value['pid'] for value in first.values() if value['pid'] is not None]
        require(len(roots)==len(set(roots)) and observe()==first,'Private jobs changed during shutdown binding')
        tracked=[cutover.tracked_processes(set(roots),rows,base['uid'])]
        if state['restart_pending'] is not None:tracked.append(state['restart_pending']['tracked'])
        cutover.job_files(entries,files);journal.require_suspended(routing)
        journal.record('shutdown-intent',generation,dict(tracked=tracked,loaded=sorted(first)))
    else:require(resume,'Existing private shutdown requires explicit recovery')
    state=journal.state();context=state['shutdown']
    require(context['generation']==generation,'Private shutdown generation changed')
    pids={pid for tree in context['tracked'] for pid in tree['pids']}
    def guard():
        activation.fixed_monitor_files(layout,generation);cutover.job_files(entries,files);journal.require_suspended(routing)
        for label in journal.state()['shutdown_stopped']:
            path,spec=by_label[label]
            require(cutover.label_state(path,spec,command) is None,'A stopped private label reappeared')
    for label in storage.stop_order():
        state=journal.state();guard();path,spec=by_label[label];current=cutover.label_state(path,spec,command)
        if label in state['shutdown_stopped']:
            require(current is None,'A stopped private job reappeared');continue
        require(current is None or current['pid'] is None or current['pid'] in pids,
                'Private job changed after shutdown binding')
        require(label in context['loaded'] or current is None,'An unstarted job appeared during shutdown')
        fresh=state['shutdown_pending'] is None
        if fresh:journal.record('shutdown-stop-intent',label)
        else:require(state['shutdown_pending']==label,'Another private stop remains unresolved')
        if fresh and current is not None:
            try:command(['/bin/launchctl','bootout','system/'+label])
            except (OSError,ValueError,subprocess.SubprocessError):pass
        deadline=clock()+timeout
        while True:
            guard();remaining=cutover.label_state(path,spec,command)
            if remaining is None:break
            require(remaining['pid'] is None or remaining['pid'] in pids,'Private job restarted during stop')
            require(clock()<deadline,'Private stop remains unresolved; intent retained')
            sleep(.1)
        journal.record('shutdown-stop-observed',label)
    deadline=clock()+timeout
    while True:
        guard();require(all(cutover.label_state(path,spec,command) is None for path,spec in entries),'Private label reappeared')
        code,output=command(['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
        require(code==0,'Cannot prove private descendant exit');rows=cutover.process_rows(output)
        if all(cutover.processes_gone(tree,rows,base['uid']) for tree in context['tracked']):break
        require(clock()<deadline,'Private descendants or dedicated-UID processes remain')
        sleep(.1)
    guard();require(all(cutover.label_state(path,spec,command) is None for path,spec in entries),'Private label changed after cleanup')
    if not journal.state()['shutdown_complete']:journal.record('shutdown-complete',generation)
    return journal.state()


def checkpoint_path(layout,generation,sha):
    require(generation in ('before','after') and isinstance(sha,str) and storage.SHA.fullmatch(sha),'Invalid monitor checkpoint')
    return layout.folder/('monitor-checkpoint-'+generation+'-'+sha+'.json')


def read_checkpoint(layout,generation,sha):
    activation.private_layout_storage(layout.folder)
    data,_=cutover.protected_bytes(checkpoint_path(layout,generation,sha),0o600,65536);value=read_json(data)
    require(hashlib.sha256(data).hexdigest()==sha and data==wire(value) and isinstance(value,dict)
            and set(value)=={'schema','producer','generation','nonce','layout_sha256','inventory'}
            and type(value['schema']) is int and value['schema']==1 and value['producer']==PRODUCER
            and value['generation']==generation and value['nonce']==layout.journal.base['nonce']
            and value['layout_sha256']==hashlib.sha256(layout.manifest).hexdigest() and isinstance(value['inventory'],dict),
            'Preserving a foreign monitor checkpoint')
    return value['inventory']


def checked_monitor_history(journal,directory,baseline,entries,generation):
    """Read typed history and unchanged policy; grants no effect authority."""
    from mod_host_upgrade_layout import snapshot
    base=journal.base
    require(os.geteuid()==cutover.ROOT_UID and generation in ('before','after')
            and directory in (cutover.ROOT/'monitor',cutover.ROOT/'upgrade-layouts'/base['nonce']/'monitor-before',
                              cutover.ROOT/'upgrade-layouts'/base['nonce']/'monitor-after'),
            'Read only the exact protected monitor history')
    journal.state();cutover.validate_entries(entries)
    inventory=snapshot(directory)
    dynamic=HEALTH|{'upgrade-quiescence.json'}
    stable=lambda value:{key:record for key,record in value.items() if key not in dynamic}
    extra=set(inventory)-set(baseline)-HEALTH
    # These are the exact Root monitor atomic-write namespaces. Stopped, bounded
    # partial bytes remain opaque forensic evidence; no worker loads them.
    temporary={name for name in extra if re.fullmatch(r'\.scan-[0-9a-f]{32}|(?:api|publication|gateway)/\.health-[0-9a-f]{32}',name)
               and len(inventory[name])==9 and inventory[name][2]<=8192}
    require(extra==temporary and {name:value for name,value in stable(inventory).items() if name not in temporary}==stable(baseline)
            and all(name in inventory for name in HEALTH&baseline.keys()),'Preserving unknown or changed monitor history')
    by_role={spec['Label'].split('.')[3]:spec['Label'] for _,spec in entries if not spec['Label'].endswith('.monitor')}
    for name in HEALTH&inventory.keys():
        data,_=cutover.protected_bytes(directory/name,0o644,8192);value=read_json(data)
        role='scan' if name=='scan.json' else name.split('/')[0]
        headers={'schema','uid','service','policy','time','status'}|({'role','release'} if role!='scan' else set())
        require(isinstance(value,dict) and headers<=value.keys()<=headers|{'pid','rss_bytes','reason'}
                and type(value['schema']) is int and value['schema']==1 and type(value['uid']) is int and value['uid']==base['uid']
                and value['service']==by_role[role] and value['policy']==('root-system-bootstrap-v2' if role=='scan' else 'root-system-host-v1')
                and (role=='scan' or (value['role']==role and value['release']==str(cutover.ROOT/'releases'/base[generation])))
                and type(value['time']) is int and 0<=value['time']<=journal.clock()
                and value['status'] in ('idle','starting','watching','terminated','error')
                and ('pid' not in value or (type(value['pid']) is int and 1<value['pid']<2**31))
                and ('rss_bytes' not in value or (type(value['rss_bytes']) is int and 0<=value['rss_bytes']<2**63))
                and ('reason' not in value or (isinstance(value['reason'],str) and len(value['reason'])<=2048)),
                'Monitor health history differs from its fixed generation')
    require(snapshot(directory)==inventory,'Monitor history changed while validating')
    return inventory


def checkpoint_monitor(journal,layout,entries,gate,*,resume=False):
    """Pin stopped health/history without consuming or deleting temporary files."""
    from mod_host_upgrade_layout import snapshot,_new
    state=journal.state();generation=state['config'];base=journal.base
    require(type(resume) is bool and layout.journal is journal and state['shutdown_complete'] and state['quiescent']
            and os.geteuid()==cutover.ROOT_UID and storage.operational_boot(journal)==boot_identity(),'Complete private shutdown first')
    gate();activation.fixed_monitor_files(layout,generation)
    cutover.validate_entries(entries);manifest=read_json(layout.manifest)
    baseline=manifest['monitor' if generation=='before' else 'after_monitor']
    inventory=checked_monitor_history(journal,cutover.ROOT/'monitor',baseline,entries,generation)
    gate();require(snapshot(cutover.ROOT/'monitor')==inventory,'Monitor history changed while checkpointing')
    data=wire(dict(schema=1,producer=PRODUCER,generation=generation,nonce=base['nonce'],
        layout_sha256=hashlib.sha256(layout.manifest).hexdigest(),inventory=inventory))
    require(len(data)<=65536,'Monitor checkpoint exceeds budget');sha=hashlib.sha256(data).hexdigest();path=checkpoint_path(layout,generation,sha)
    try:_new(layout.folder,path.name,data,0o600)
    except FileExistsError:
        require(resume and cutover.protected_bytes(path,0o600,65536)[0]==data,'Preserving an existing monitor checkpoint')
    read_checkpoint(layout,generation,sha);gate()
    require(snapshot(cutover.ROOT/'monitor')==inventory,'Monitor history changed before checkpoint admission')
    if journal.state()['monitor_checkpoints'].get(generation)!=sha:journal.record('monitor-checkpoint',generation,sha)
    return sha
