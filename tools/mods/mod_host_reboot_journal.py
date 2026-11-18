"""Typed reboot-preparation history, separate from pre-boot effect authority."""
import re

import mod_host_cutover as cutover
from mod_package import require

SHA=re.compile('[0-9a-f]{64}\\Z')
GENERATIONS=(['before'],['after'],['before','after'])


def generated(label,backup_identity):
    role=label.split('.')[3]
    if role=='backup':return cutover.ROOT/'backup-jobs'/backup_identity/'worker.plist'
    if role=='scan':return cutover.ROOT/'monitor'/('monitor.plist' if label.endswith('.monitor') else 'scan.plist')
    if role=='definitions':return cutover.ROOT/'monitor/definitions.plist'
    return cutover.ROOT/'monitor'/role/('monitor.plist' if label.endswith('.monitor') else 'worker.plist')


def checked_file(value,mode,*,generations=False):
    require(isinstance(value,dict) and set(value)==({'sha256','stamp','generations'} if generations else {'sha256','stamp'})
            and isinstance(value['sha256'],str) and SHA.fullmatch(value['sha256'])
            and isinstance(value['stamp'],list) and len(value['stamp'])==9
            and all(type(v) is int and v>=0 for v in value['stamp'])
            and 0<value['stamp'][2]<=32768 and value['stamp'][5]==cutover.ROOT_UID
            and value['stamp'][7:]==[mode,1], 'Invalid protected reboot file observation')
    if generations:require(value['generations'] in GENERATIONS,'Invalid reboot file generation')


def checked_observation(value,prior_boot,uid,backup_identity):
    require(isinstance(value,dict) and set(value)=={'schema','phase','prior_boot','boot_id','uid','files','observation','tracked','services_changed'}
            and type(value['schema']) is int and value['schema']==1 and value['phase']=='reboot-jobs-observed'
            and value['prior_boot']==prior_boot and isinstance(value['boot_id'],str) and SHA.fullmatch(value['boot_id'])
            and value['boot_id']!=prior_boot and type(value['uid']) is int and value['uid']==uid
            and value['services_changed'] is False,'Invalid fresh reboot observation binding')
    labels=cutover.jobs();backup=next(label for label in labels if label.endswith('.backup'))
    paths={str(cutover.LAUNCHD/(label+'.plist')):0o644 for label in labels}
    paths[str(generated(backup,backup_identity))]=0o444
    require(isinstance(value['files'],dict) and set(value['files'])==set(paths),'Reboot file set differs from fixed boot/backup policy')
    for path,mode in paths.items():checked_file(value['files'][path],mode,generations=True)
    for path in (str(cutover.LAUNCHD/(backup+'.plist')),str(generated(backup,backup_identity))):
        require(value['files'][path]['generations']==['before','after'],'Preserved backup generations differ')
    observed=value['observation']
    require(isinstance(observed,dict) and set(observed)=={'jobs','generated_files'} and isinstance(observed['jobs'],dict)
            and set(observed['jobs'])==set(labels) and isinstance(observed['generated_files'],dict),
            'Reboot observation changes fixed loaded jobs')
    expected={};running=[]
    for label,job in observed['jobs'].items():
        if job is None:continue
        require(isinstance(job,dict) and set(job)=={'path','pid','generations'} and job['generations'] in GENERATIONS
                and job['path'] in (str(cutover.LAUNCHD/(label+'.plist')),str(generated(label,backup_identity)))
                and (job['pid'] is None or (type(job['pid']) is int and 1<job['pid']<2**31)),
                'Invalid loaded reboot job binding')
        if job['path']==str(generated(label,backup_identity)):
            expected[job['path']]=0o444 if label==backup else 0o600
        else:require(job['generations']==value['files'][job['path']]['generations'],'Loaded reboot generation differs from boot policy')
        if job['pid'] is not None:running.append(job['pid'])
    require(set(observed['generated_files'])==set(expected),'Reboot generated-origin inventory differs')
    for path,mode in expected.items():checked_file(observed['generated_files'][path],mode)
    cutover.checked_tracking(value['tracked'])
    require(len(running)==len(set(running)) and set(running)<=set(value['tracked']['pids']),
            'Reboot tracking omits a fixed current process or shares roots')
    return value


def receipt(value,baseline):
    require(isinstance(value,dict) and set(value)=={*baseline,'expose'} and value['expose'] is False
            and type(value['schema']) is int and value['schema']==1 and value['origin']==baseline['origin']
            and value['phase'] in ('public','private','absent') and value['unrelated_sha256']==baseline['unrelated_sha256']
            and isinstance(value['config_sha256'],str) and SHA.fullmatch(value['config_sha256'])
            and (value['phase']!='public' or value['config_sha256']==baseline['config_sha256']),
            'Reboot routing observation changes the preserved authority')
    return value


def event(state,entry,*,original_boot,uid,backup_identity,backup_base_release,routing,order,nonce,layout_sha256):
    kind,label,data=entry['kind'],entry['label'],entry['data']
    if kind=='reboot-observed':
        require(label is None and isinstance(original_boot,str) and SHA.fullmatch(original_boot)
                and isinstance(data,dict) and set(data)=={'context_sha256','observation'}
                and isinstance(data['context_sha256'],str) and SHA.fullmatch(data['context_sha256']),
                'Reboot preparation requires bound original context')
        prior=original_boot if state is None else state['boot_id']
        observed=checked_observation(data['observation'],prior,uid,backup_identity)
        require(state is None or data['context_sha256']==state['context_sha256'],'Original reboot context changed between boots')
        seen=[] if state is None else state['prior_boots']+[state['boot_id']]
        require(observed['boot_id'] not in [original_boot,*seen],'Reboot preparation cannot reuse an earlier boot')
        return dict(boot_id=observed['boot_id'],prior_boots=seen,context_sha256=data['context_sha256'],observation=observed,
            routing_pending=None,routing_suspended=False,routing=None,stopped=[],pending=None,quiescent=False,quiesced_at=None,
            layout_checkpoint=None,backup_pending=None,backup=None,backup_time=None)
    if kind=='reboot-layout-verified':
        require(state is not None and state['quiescent'] and label is None and isinstance(data,dict)
                and set(data)=={'boot_id','original_boot','nonce','layout_sha256','context_sha256','checkpoint_sha256'}
                and data['boot_id']==state['boot_id'] and data['original_boot']==original_boot and data['nonce']==nonce
                and data['layout_sha256']==layout_sha256 and data['context_sha256']==state['context_sha256']
                and isinstance(data['checkpoint_sha256'],str) and SHA.fullmatch(data['checkpoint_sha256'])
                and state['layout_checkpoint'] is None,'Reboot layout checkpoint lacks exact stopped-generation binding')
        return dict(state,layout_checkpoint=data)
    if kind in ('reboot-backup-intent','reboot-backup-verified'):
        require(state is not None and state['quiescent'] and state['layout_checkpoint'] is not None and label is None,
                'Reboot backup lacks stopped verified-layout context')
        sha=state['layout_checkpoint']['checkpoint_sha256']
        if kind=='reboot-backup-intent':
            require(state['backup_pending'] is None and isinstance(data,dict) and set(data)=={'checkpoint_sha256'}
                    and data['checkpoint_sha256']==sha,'Another reboot backup intent remains unresolved')
            return dict(state,backup_pending=dict(checkpoint_sha256=sha,time=entry['time']))
        require(state['backup_pending'] is not None and isinstance(data,dict) and set(data)=={'checkpoint_sha256','receipt'}
                and data['checkpoint_sha256']==sha,'Reboot backup receipt lacks its exact intent')
        from mod_host_upgrade_process import checked_backup_receipt
        report=checked_backup_receipt(data['receipt'],backup_base_release,uid,backup_identity,
            max(state['quiesced_at'],state['backup_pending']['time']),clock=lambda:entry['time'])
        require(0<=entry['time']-report['finished']<=300,'Reboot backup verification expired before recording')
        return dict(state,backup_pending=None,backup=report,backup_time=entry['time'])
    require(state is not None and not state['quiescent'],'Reboot preparation lacks a current unfinished observation')
    state=dict(state)
    if kind in ('reboot-routing-intent','reboot-routing-observed'):
        require(label is None and not state['stopped'] and state['pending'] is None,'Reboot routing cannot cross service stops')
        route=receipt(data,routing)
        if kind=='reboot-routing-intent':
            require(state['routing_pending'] is None and not state['routing_suspended'] and route['phase']=='public',
                    'Invalid reboot suspension intent')
            state['routing_pending']=False
        else:
            require(route['phase']!='public' and not state['routing_suspended'],'Reboot public suspension is unconfirmed')
            state['routing_pending']=None;state['routing_suspended']=True;state['routing']=route
    elif kind in ('reboot-stop-intent','reboot-stop-observed'):
        require(data is None and state['routing_suspended'] and state['routing_pending'] is None,'Reboot stop lacks suspended routing')
        if kind=='reboot-stop-intent':
            require(state['pending'] is None and len(state['stopped'])<len(order) and label==order[len(state['stopped'])],
                    'Reboot stop intent is out of order')
            state['pending']=label
        else:
            require(label==state['pending'] and label is not None,'Reboot stop observation lacks its intent')
            state['stopped']=[*state['stopped'],label];state['pending']=None
    else:
        require(kind=='reboot-quiescent' and label is None and data is None and state['routing_suspended']
                and state['routing_pending'] is None and state['pending'] is None and state['stopped']==order,
                'Reboot quiescence lacks every fixed stop observation')
        state['quiescent']=True;state['quiesced_at']=entry['time']
    return state
