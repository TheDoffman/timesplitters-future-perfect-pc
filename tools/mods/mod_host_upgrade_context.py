"""Root-only metadata for reconstructing the original same-boot upgrade context.

Saved job specifications are compared with newly verified factory outputs, never
executed. Sealed closures, backup binding and transports must be reconstructed by
the immutable caller before reading this artifact. No service effect or CLI here.
"""
import hashlib
from itertools import islice
import json
import os
import plistlib
import re
import stat

from mod_package import read_json,require

MAX_BYTES=65536
MAX_CONTEXTS=32
BINDING=('before','after','uid','nonce','plan_sha256','preflight_sha256','boot_id','backup_identity','backup_base_release')
PRODUCER='tsfp-root-upgrade-context-v1'


def metadata(entries):return [dict(generated=str(path),specification=spec) for path,spec in entries]


def preflight_digest(observed,entries,routing,plan,private):
    value=dict(jobs=observed,candidate_jobs=metadata(entries),routing=routing,plan_sha256=plan['sha256'],private=private)
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def _binding(journal):return {key:journal.base[key] for key in BINDING}


def _checked(value,journal,fresh):
    import mod_host_cutover as cutover
    import mod_host_migration as migration
    import mod_host_upgrade_process as process
    from mod_host_cutover_journal import wire
    base=journal.base
    require(os.geteuid()==cutover.ROOT_UID and base['boot_id']==migration.boot_identity(),
            'Recovery context requires the original Root boot authority')
    journal.state()
    require(isinstance(value,dict) and set(value)=={'schema','producer','binding','plan','private','routing','observed','jobs','candidate_jobs'}
            and type(value['schema']) is int and value['schema']==1 and value['producer']==PRODUCER
            and value['binding']==_binding(journal) and value['routing']==base['routing']
            and wire(value['jobs'])==wire(metadata(fresh['entries']))
            and wire(value['candidate_jobs'])==wire(metadata(fresh['candidate_entries'])),
            'Preserving a foreign upgrade recovery context')
    cutover.validate_entries(fresh['entries']);cutover.validate_entries(fresh['candidate_entries'])
    process.checked_migration_plan(value['plan'],base['before'],base['after'],fresh['profile'])
    require(value['plan']['sha256']==base['plan_sha256'],'Recovery migration plan changed')
    process.checked_private_result(value['private'],base['before'],base['uid'],fresh['profile'])
    observed=value['observed']
    require(isinstance(observed,dict) and set(observed)=={'files','observation'}
            and isinstance(observed['files'],dict) and len(observed['files'])==20
            and isinstance(observed['observation'],dict) and set(observed['observation'])=={'jobs','tracked'}
            and observed['observation']['tracked']==base['tracked'],'Recovery observation differs from the transaction')
    cutover.checked_tracking(observed['observation']['tracked'])
    expected={}
    for path,spec in fresh['entries']:
        sha=hashlib.sha256(plistlib.dumps(spec)).hexdigest()
        expected[str(path)]=(sha,0o444 if spec['Label'].endswith('.backup') else 0o600)
        expected[str(cutover.LAUNCHD/(spec['Label']+'.plist'))]=(sha,0o644)
    require(set(observed['files'])==set(expected),'Recovery context changes the fixed file set')
    for path,(sha,mode) in expected.items():
        record=observed['files'][path]
        require(isinstance(record,dict) and set(record)=={'sha256','stamp'} and record['sha256']==sha
                and isinstance(record['stamp'],list) and len(record['stamp'])==9
                and all(type(v) is int and v>=0 for v in record['stamp'])
                and record['stamp'][5]==cutover.ROOT_UID and record['stamp'][7:]==[mode,1],
                'Recovery context contains unrecognized protected-file metadata')
    jobs=observed['observation']['jobs'];pids=[]
    require(isinstance(jobs,dict) and set(jobs)=={spec['Label'] for _,spec in fresh['entries']},
            'Recovery context changes the observed fixed labels')
    for path,spec in fresh['entries']:
        record=jobs[spec['Label']]
        require(isinstance(record,dict) and set(record)=={'path','pid'}
                and record['path'] in (str(path),str(cutover.LAUNCHD/(spec['Label']+'.plist')))
                and (record['pid'] is None or (type(record['pid']) is int and 1<record['pid']<2**31)),
                'Recovery context contains unrecognized loaded-job metadata')
        if record['pid'] is not None:pids.append(record['pid'])
    require(len(pids)==len(set(pids)) and set(pids)<=set(base['tracked']['pids'])
            and preflight_digest(observed,fresh['candidate_entries'],value['routing'],value['plan'],value['private'])==base['preflight_sha256'],
            'Recovery context does not match the original preflight digest')
    return value


def _directory(journal,*,create=False):
    import mod_host_cutover as cutover
    from mod_host_migration import boot_identity
    require(os.geteuid()==cutover.ROOT_UID and journal.base['boot_id']==boot_identity(),
            'Upgrade context storage requires same-boot Root authority')
    journal.state();folder=cutover.ROOT/'upgrade-contexts'
    if create:
        parent=cutover.protected_directory(cutover.ROOT)
        try:
            try:os.mkdir(folder.name,0o700,dir_fd=parent);os.fsync(parent)
            except FileExistsError:pass
        finally:os.close(parent)
    fd=cutover.protected_directory(folder)
    try:
        require(stat.S_IMODE(os.fstat(fd).st_mode)==0o700,'Unsafe recovery context directory')
        with os.scandir(fd) as scan:names=[entry.name for entry in islice(scan,MAX_CONTEXTS+1)]
        require(len(names)<=MAX_CONTEXTS and all(re.fullmatch('[0-9a-f]{64}\\.json',name) for name in names),
                'Preserving unknown or excessive recovery context artifacts')
        return folder,fd,names
    except BaseException:os.close(fd);raise


def _visible(folder,fd):
    import mod_host_cutover as cutover
    current=cutover.protected_directory(folder)
    try:require((os.fstat(current).st_dev,os.fstat(current).st_ino)==(os.fstat(fd).st_dev,os.fstat(fd).st_ino),
                'Recovery context directory changed')
    finally:os.close(current)


def seal(journal,context,*,resume=False):
    """New-only bounded metadata, before any event; exact adoption is explicit.

    A partial or foreign file is preserved. An absent artifact after service
    effects cannot be recreated from a newly observed or guessed context.
    """
    import mod_host_cutover as cutover
    from mod_host_cutover_journal import wire
    from mod_host_upgrade_coordinator import checked_context
    require(type(resume) is bool,'Choose explicit context-file recovery')
    checked_context(journal,context)
    value=dict(schema=1,producer=PRODUCER,binding=_binding(journal),plan=context['plan'],
        private=context['private_baseline'],routing=context['routing'],observed=context['observed'],
        jobs=metadata(context['entries']),candidate_jobs=metadata(context['candidate_entries']))
    data=wire(value);require(0<len(data)<=MAX_BYTES,'Upgrade recovery context exceeds budget')
    _checked(read_json(data),journal,context)
    folder,fd,names=_directory(journal,create=True);name=journal.base['nonce']+'.json'
    try:
        if name in names:
            require(resume and cutover.protected_bytes(folder/name,0o600,MAX_BYTES)[0]==data,
                    'Preserving an existing recovery context')
        else:
            require(not read_json(journal.current)['events'] and len(names)<MAX_CONTEXTS,
                    'Cannot create recovery context after events or beyond capacity')
            _visible(folder,fd);journal.state()
            child=os.open(name,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
            with os.fdopen(child,'wb') as stream:
                stream.write(data);stream.flush();os.fchmod(stream.fileno(),0o600);os.fsync(stream.fileno())
            os.fsync(fd)
        _visible(folder,fd);journal.state()
        require(cutover.protected_bytes(folder/name,0o600,MAX_BYTES)[0]==data,'Recovery context changed before admission')
    finally:os.close(fd)
    return hashlib.sha256(data).hexdigest()


def reconstruct(journal,fresh):
    """Combine historical metadata with freshly verified closures/factories.

    This neither re-runs private preflight against changed configuration nor
    turns historical process observations into evidence of current OS state.
    """
    import mod_host_cutover as cutover
    from mod_host_cutover_journal import wire
    from mod_host_upgrade_coordinator import checked_context
    folder,fd,_=_directory(journal)
    try:
        data,identity=cutover.protected_bytes(folder/(journal.base['nonce']+'.json'),0o600,MAX_BYTES)
        value=read_json(data);require(data==wire(value),'Preserving noncanonical recovery context')
        _checked(value,journal,fresh);_visible(folder,fd);journal.state()
        need,stamp=cutover.protected_bytes(folder/(journal.base['nonce']+'.json'),0o600,MAX_BYTES)
        require(need==data and stamp==identity,'Recovery context changed while reconstructing')
    finally:os.close(fd)
    report={key:journal.base[key] for key in ('before','after','uid','plan_sha256','preflight_sha256','backup_identity','backup_base_release')}
    report['phase']='historical-upgrade-context-reconstructed'
    context=dict(fresh,plan=value['plan'],observed=value['observed'],routing=value['routing'],
        private_baseline=value['private'],report=report)
    checked_context(journal,context)
    return context
