"""Fresh fixed-job observations after a reboot, never pre-boot PID authority.

The immutable caller must verify both release closures and hold the original
Root recovery lease. This reader neither opens a recovery journal nor authorizes
stops, routing, migration, startup or settlement. Known mixed boot generations
are reported individually; protected monitor/layout recovery remains separate.
"""
import hashlib
import os
import plistlib
import re

import mod_host_cutover as cutover
from mod_host_migration import boot_identity
from mod_package import ModError,require


def policies(before,after):
    cutover.validate_entries(before);cutover.validate_entries(after)
    old={spec['Label']:(path,spec) for path,spec in before}
    new={spec['Label']:(path,spec) for path,spec in after}
    require(all(old[label][0]==new[label][0] for label in old),'Reboot recovery changes fixed job paths')
    backup=next(label for label in old if label.endswith('.backup'))
    require(old[backup]==new[backup],'Reboot recovery changes the preserved backup binding')
    result={}
    for label in old:
        choices=[]
        for side,entries in (('before',old),('after',new)):
            path,spec=entries[label];wire=plistlib.dumps(spec)
            require(0<len(wire)<=32768,'Reboot job policy exceeds budget')
            choices.append(dict(side=side,path=path,spec=spec,wire=wire))
        result[label]=choices
    return result


def files(choices):
    """Read all ten boot files and the unchanged backup's generated file.

    The monitor tree may be between its known archive/install operations. Its
    complete inventory is deliberately not inferred from these eleven files.
    """
    result={}
    for label,versions in choices.items():
        targets=[(cutover.LAUNCHD/(label+'.plist'),0o644)]
        if label.endswith('.backup'):targets.append((versions[0]['path'],0o444))
        for path,mode in targets:
            wire,stamp=cutover.protected_bytes(path,mode,32768)
            matches=[item['side'] for item in versions if item['wire']==wire]
            require(matches,'Reboot boot file differs from both verified generations')
            result[str(path)]=dict(generations=matches,sha256=hashlib.sha256(wire).hexdigest(),stamp=stamp)
    require(len(result)==11,'Reboot boot/backup file inventory differs')
    return result


def loaded(choices,boot_files,command):
    result={};generated_files={}
    for label,versions in choices.items():
        arguments=['/bin/launchctl','print','system/'+label]
        reply=command(arguments)
        cached=lambda argv:reply if argv==arguments else (_ for _ in ()).throw(ModError('Unexpected observation command'))
        matches=[]
        for item in versions:
            try:observed=cutover.label_state(item['path'],item['spec'],cached)
            except ModError:continue
            if observed is None:result[label]=None;break
            matches.append((item,observed))
        else:
            require(matches,'Loaded reboot job differs from both verified policies')
            # The current protected origin bytes must match the loaded command,
            # not just another known generation's executable name or label.
            accepted=[]
            for item,observed in matches:
                path=observed['path']
                if path==str(cutover.LAUNCHD/(label+'.plist')):
                    if item['side'] not in boot_files[path]['generations']:continue
                else:
                    mode=0o444 if label.endswith('.backup') else 0o600
                    wire,stamp=cutover.protected_bytes(item['path'],mode,32768)
                    if wire!=item['wire']:continue
                    generated_files[path]=dict(sha256=hashlib.sha256(wire).hexdigest(),stamp=stamp)
                accepted.append((item,observed))
            require(accepted,'Loaded reboot job differs from its current protected origin')
            require(all(value==accepted[0][1] for _,value in accepted),'Ambiguous reboot job identity')
            result[label]=dict(accepted[0][1],generations=[item['side'] for item,_ in accepted])
    return dict(jobs=result,generated_files=generated_files)


def observe(before,after,uid,prior_boot,command):
    """Attest fixed origins twice around a fresh numeric process snapshot.

    Only the prior boot identity is accepted. Saved PIDs/groups cannot enter this
    API. An unrelated process reusing an old PID is consequently never adopted
    as a service root. Escaped current dedicated-UID processes are refused.
    """
    current=boot_identity()
    require(os.geteuid()==cutover.ROOT_UID and type(uid) is int and uid>=600
            and isinstance(prior_boot,str) and re.fullmatch('[0-9a-f]{64}',prior_boot)
            and isinstance(current,str) and re.fullmatch('[0-9a-f]{64}',current) and current!=prior_boot,
            'Use explicit Root observation of a different boot')
    choices=policies(before,after);protected=files(choices)
    first=loaded(choices,protected,command)
    running=[item['pid'] for item in first['jobs'].values() if item is not None and item['pid'] is not None]
    require(len(running)==len(set(running)),'Reboot jobs share a process identity')
    code,output=command(['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
    require(code==0,'Cannot observe fresh reboot processes')
    rows=cutover.process_rows(output);tracked=cutover.tracked_processes(set(running),rows,uid)
    require(all(owner!=uid or pid in tracked['pids'] for pid,(_,_,owner) in rows.items()),
            'Unrecorded dedicated-identity processes after reboot')
    require(loaded(choices,protected,command)==first and files(choices)==protected and boot_identity()==current,
            'Reboot jobs, protected origins or boot changed during observation')
    return dict(schema=1,phase='reboot-jobs-observed',prior_boot=prior_boot,boot_id=current,uid=uid,
        files=protected,observation=first,tracked=tracked,services_changed=False)
