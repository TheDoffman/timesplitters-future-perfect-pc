"""Bounded real child transport and typed installed upgrade protocols."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest

SOURCE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SOURCE/'tools/mods'))
import mod_host_upgrade_process as process
import mod_host_migration as migration
from mod_package import ModError


def migration_report(profile):
    import hashlib
    manifest=dict(schema=1,producer=migration.PRODUCER,before='a'*64,after='b'*64,origin=profile['origin'],
        guards={name:'9'*64 for name in migration.GUARDS},
        files={name:dict(before='1'*64,after='2'*64) for name in migration.FIELDS})
    return dict(manifest=manifest,sha256=hashlib.sha256(migration.wire(manifest)).hexdigest(),files=6,services_changed=False)


@pytest.mark.parametrize('damage',[None,'uid','release','origin','root','floor','rollback','reused-version',
    'count','boolean','revocations','transport','health','public','unknown','checkpoint-extra','version-limit'])
def test_public_acceptance_protocol_binds_fixed_child_and_both_catalog_checkpoints(verified,monkeypatch,damage):
    _,current=verified;profile=dict(origin='https://workshop.example.test:8443',trusted_root_sha256='9'*64)
    checkpoint=dict(catalog_version=42,metadata_sha256='1'*64,revocations_sha256='2'*64,revocation_count=3)
    report=dict(schema=1,phase='public-services-verified',release=current['identity'],uid=600,**profile,
        loopback=dict(checkpoint),public=dict(checkpoint),release_count=0,revocations_preserved=True,
        role_health_verified=True,transport='public-https',public_endpoint_verified=True)
    if damage=='uid':report['uid']=True
    elif damage=='release':report['release']='0'*64
    elif damage=='origin':report['origin']='https://foreign.example'
    elif damage=='root':report['trusted_root_sha256']='0'*64
    elif damage=='floor':report['loopback']['catalog_version']=39
    elif damage=='rollback':report['public']['catalog_version']=41
    elif damage=='reused-version':report['public']['metadata_sha256']='3'*64
    elif damage=='count':report['public']['revocation_count']=2
    elif damage=='boolean':report['public']['catalog_version']=True
    elif damage=='revocations':report['revocations_preserved']=False
    elif damage=='transport':report['transport']='private-loopback-http'
    elif damage=='health':report['role_health_verified']=False
    elif damage=='public':report['public_endpoint_verified']=False
    elif damage=='unknown':report['private']='canary'
    elif damage=='checkpoint-extra':report['public']['private']='canary'
    elif damage=='version-limit':report['public']['catalog_version']=2**53
    def capture(arguments,options,*,timeout,limit):
        assert arguments==[str(current['python']),'-I','-B',str(current['root']/'source/tools/mods/hosting/accept-public-host-services-mac.py')]
        assert options['user']==options['group']==600 and options['extra_groups']==[] and timeout==110 and limit==8192
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.public_readiness(current,600,profile,floor=40)
    else:assert process.public_readiness(current,600,profile,floor=40)==report


@pytest.mark.parametrize('damage',[None,'uid','release','root','origin','rollback','boolean','public','health','unknown'])
def test_private_readiness_protocol_binds_floor_origin_root_and_exact_loopback_evidence(verified,monkeypatch,damage):
    _,current=verified;profile=dict(origin='https://workshop.example.test:8443',trusted_root_sha256='9'*64)
    report=dict(schema=1,phase='private-services-verified',release=current['identity'],uid=600,**profile,
        catalog_version=42,release_count=3,role_health_verified=True,transport='private-loopback-http',public_endpoint_verified=False)
    if damage=='uid':report['uid']=True
    elif damage=='release':report['release']='0'*64
    elif damage=='root':report['trusted_root_sha256']='0'*64
    elif damage=='origin':report['origin']='https://foreign.example'
    elif damage=='rollback':report['catalog_version']=39
    elif damage=='boolean':report['catalog_version']=True
    elif damage=='public':report['public_endpoint_verified']=True
    elif damage=='health':report['role_health_verified']=False
    elif damage=='unknown':report['private']='canary'
    def capture(arguments,options,*,timeout,limit):
        assert arguments==[str(current['python']),'-I','-B',str(current['root']/'source/tools/mods/hosting/accept-private-host-services-mac.py')]
        assert options['user']==options['group']==600 and options['extra_groups']==[] and timeout==110 and limit==8192
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.private_readiness(current,600,profile,floor=40)
    else:assert process.private_readiness(current,600,profile,floor=40)==report


@pytest.mark.parametrize('damage',[None,'uid','release','clean','eicar','missing-engine','temporary','network','unknown'])
def test_scanner_control_protocol_requires_exact_isolated_release_and_fail_closed_results(verified,monkeypatch,damage):
    _,current=verified
    report=dict(schema=1,phase='scanner-admitted',uid=600,release=current['identity'],version='ClamAV fixture',
        results=dict(clean='clean',blocked='blocked',empty_database='error',missing_engine='error'),
        temporary_state_removed=True,services_started=False,network_started=False)
    if damage=='uid':report['uid']=True
    elif damage=='release':report['release']='0'*64
    elif damage=='clean':report['results']['clean']='error'
    elif damage=='eicar':report['results']['blocked']='clean'
    elif damage=='missing-engine':report['results']['missing_engine']='clean'
    elif damage=='temporary':report['temporary_state_removed']=False
    elif damage=='network':report['network_started']=True
    elif damage=='unknown':report['private']='canary'
    def capture(arguments,options,*,timeout,limit):
        assert arguments==[str(current['python']),'-I','-B',str(current['root']/'source/tools/mods/hosting/accept-installed-scanner-mac.py')]
        assert options['user']==options['group']==600 and options['extra_groups']==[] and timeout==900 and limit==8192
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.scanner_check(current,600)
    else:assert process.scanner_check(current,600)==report


@pytest.mark.parametrize('damage',[None,'origin','root','extra-file','unchanged-file','hash','boolean','payload'])
def test_private_plan_protocol_binds_origin_root_and_exact_six_outputs(verified,monkeypatch,damage):
    _,current=verified;profile=dict(origin='https://workshop.example.test:8443',trusted_root_sha256='9'*64)
    report=migration_report(profile)
    if damage=='origin':report['manifest']['origin']='https://foreign.example'
    elif damage=='root':report['manifest']['guards']['host/trusted-root.json']='8'*64
    elif damage=='extra-file':report['manifest']['files']['host/key.pem']=dict(before='1'*64,after='2'*64)
    elif damage=='unchanged-file':report['manifest']['files'][migration.FIELDS[0]]['after']='1'*64
    elif damage=='hash':report['sha256']='0'*64
    elif damage=='boolean':report['files']=True
    elif damage=='payload':report['private_config']='secret-canary'
    def capture(arguments,options,*,timeout,limit):
        assert arguments[:3]==[str(current['python']),'-I','-B'] and '-S' not in arguments
        assert arguments[-3:]==['--inspect','--from-release','a'*64]
        assert options['user']==options['group']==600 and options['extra_groups']==[]
        assert timeout==30 and limit==16384
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.migration_plan(current,600,'a'*64,profile)
    else:assert process.migration_plan(current,600,'a'*64,profile)==report


@pytest.mark.parametrize('rollback',[False,True])
@pytest.mark.parametrize('damage',[None,'phase','plan','keys','repository','services','unknown','files'])
def test_private_mutation_protocol_checks_exact_result_without_secret_payloads(verified,monkeypatch,rollback,damage):
    _,current=verified
    result=dict(schema=1,phase='rolled-back' if rollback else 'applied',plan_sha256='d'*64,before='a'*64,after='b'*64,
                files=6,services_changed=False,repository_modified=False,keys_modified=False)
    if damage=='phase':result['phase']='inspected'
    elif damage=='plan':result['plan_sha256']='e'*64
    elif damage in ('keys','repository','services'):result[damage+'_modified' if damage!='services' else 'services_changed']=True
    elif damage=='unknown':result['payload']='secret-canary'
    elif damage=='files':result['files']=True
    def capture(arguments,options,*,timeout,limit):
        assert arguments[-5:]==['--rollback' if rollback else '--apply','--from-release','a'*64,'--nonce','c'*64]
        assert options['user']==600 and timeout==30 and limit==8192
        return json.dumps(result).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.migration_step(current,600,'a'*64,'c'*64,'d'*64,rollback=rollback)
    else:assert process.migration_step(current,600,'a'*64,'c'*64,'d'*64,rollback=rollback)==result


@pytest.mark.parametrize('case', ['success','stderr','nonzero','stdout-overflow','stderr-overflow','timeout','selector-failed'])
def test_real_owned_child_is_reaped_after_transport_success_and_failures(private_posix_path,monkeypatch,case):
    programs=dict(success="print('ok')",stderr="import sys;sys.stderr.write('private-canary')",
                  nonzero='raise SystemExit(3)',**{'stdout-overflow':"print('x'*100000)",
                  'stderr-overflow':"import sys;sys.stderr.write('x'*100000)",
                  'timeout':'import time;time.sleep(30)','selector-failed':'import time;time.sleep(30)'})
    real=process.subprocess.Popen;children=[]
    def spawn(*args,**kwargs):
        child=real(*args,**kwargs);children.append(child);return child
    monkeypatch.setattr(process.subprocess,'Popen',spawn)
    if case=='selector-failed':
        monkeypatch.setattr(process.selectors,'DefaultSelector',lambda:(_ for _ in ()).throw(OSError('selector unavailable')))
    arguments=[sys.executable,'-I','-S','-B','-c',programs[case]]
    if case=='success':assert process._capture(arguments,{},timeout=3,limit=64)==b'ok\n'
    else:
        with pytest.raises((ModError,OSError)) as error:process._capture(arguments,{},timeout=.15 if case=='timeout' else 3,limit=64)
        assert 'private-canary' not in str(error.value)
    assert len(children)==1 and children[0].poll() is not None
    assert children[0].stdout.closed and children[0].stderr.closed


@pytest.fixture
def verified(monkeypatch):
    monkeypatch.setattr(process.sys,'platform','darwin')
    monkeypatch.setattr(process.os,'geteuid',lambda:process.ROOT_UID,raising=False)
    def entry(identity):
        root=process.ROOT/'releases'/identity
        return dict(root=root,identity=identity,uid=600,python=root/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12',
                    descriptor=dict(source_files={name:{} for name in ('tools/mods/hosting/upgrade-job-specs-mac.py',
                                                                        'tools/mods/hosting/force-upgrade-backup-mac.py',
                                                                        'tools/mods/hosting/migrate-private-generation-mac.py',
                                                                        'tools/mods/hosting/accept-installed-scanner-mac.py',
                                                                        'tools/mods/hosting/accept-private-host-services-mac.py',
                                                                        'tools/mods/hosting/accept-public-host-services-mac.py')}))
    return entry('a'*64),entry('b'*64)


def job_report(runtime):
    spec=importlib.util.spec_from_file_location('upgrade_process_factory_test',SOURCE/'tools/mods/hosting/register-installed-host-mac.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    entries=module.jobs(runtime['root'],dict(uid=600,gid=600),runtime['python'])
    return dict(schema=1,producer='tsfp-upgrade-job-specs-v1',kind='host',identity=runtime['identity'],
                base_release=runtime['identity'],uid=600,gid=600,kernel_groups=[],
                entries=[dict(generated=str(path),specification=value) for path,value in entries])


@pytest.mark.parametrize('damage', [None,'schema','uid','groups','identity','base','kind','duplicate','argv','unknown'])
def test_job_protocol_checks_binding_labels_credentials_and_fixed_launch_arguments(verified,monkeypatch,damage):
    runtime,current=verified;report=job_report(runtime)
    if damage=='schema':report['schema']=True
    elif damage=='uid':report['uid']=True
    elif damage=='groups':report['kernel_groups']=[0]
    elif damage=='identity':report['identity']='c'*64
    elif damage=='base':report['base_release']='c'*64
    elif damage=='kind':report['kind']='backup'
    elif damage=='duplicate':report['entries'][-1]=report['entries'][0]
    elif damage=='argv':report['entries'][0]['specification']['ProgramArguments'][0]='/foreign/python'
    elif damage=='unknown':report['other']=False
    def capture(arguments,options,*,timeout,limit):
        assert arguments[:5]==[str(runtime['python']),'-I','-S','-B',str(current['root']/'source/tools/mods/hosting/upgrade-job-specs-mac.py')]
        assert arguments[5:]==['--host-release',runtime['identity']]
        assert options['user']==options['group']==600 and options['extra_groups']==[]
        assert options['env']==dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=str(process.ROOT/'data'),TMPDIR=str(process.ROOT/'data'),LC_ALL='C',TZ='UTC')
        assert timeout==15 and limit==65536
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.job_specs(runtime,current,600)
    else:assert len(process.job_specs(runtime,current,600))==9


@pytest.mark.parametrize('damage', [None,'not-due','unverified','before-quiescence','future','uid','base','hash','name','unknown'])
def test_forced_backup_protocol_requires_new_verified_capture_after_quiescence(verified,monkeypatch,damage):
    runtime,current=verified;now=int(time.time());identity='c'*64
    report=dict(schema=1,producer='tsfp-upgrade-backup-v1',identity=identity,base_release=runtime['identity'],uid=600,
                status='captured',backup='snapshot-'+('d'*32)+'-'+str(now)+'-00000001',
                verified_manifest_sha256='e'*64,created=now-1,started=now-2,finished=now,bytes=100,forced=True,verified=True)
    if damage=='not-due':report['status']='not-due'
    elif damage=='unverified':report['verified']=False
    elif damage=='before-quiescence':report.update(started=now-10,created=now-9)
    elif damage=='future':report['finished']=now+3600
    elif damage=='uid':report['uid']=601
    elif damage=='base':report['base_release']='f'*64
    elif damage=='hash':report['verified_manifest_sha256']='E'*64
    elif damage=='name':report['backup']='snapshot-../private'
    elif damage=='unknown':report['other']=True
    def capture(arguments,options,*,timeout,limit):
        assert arguments[-2:]==['--backup-release',identity] and '--capture' in arguments
        assert timeout==1800 and limit==8192 and options['user']==600
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    if damage:
        with pytest.raises(ModError):process.forced_backup(runtime,current,600,identity,quiesced_at=now-3)
    else:assert process.forced_backup(runtime,current,600,identity,quiesced_at=now-3)['forced']


def test_child_context_refuses_unprivileged_invocation_before_spawn(verified,monkeypatch):
    runtime,current=verified
    monkeypatch.setattr(process.os,'geteuid',lambda:501,raising=False)
    monkeypatch.setattr(process,'_capture',lambda *args,**kwargs:pytest.fail('Spawned before root admission'))
    with pytest.raises(ModError):process.job_specs(runtime,current,600)


def test_backup_factory_uses_its_independent_retained_runtime(verified,monkeypatch):
    import mod_backup_release as backup
    runtime,current=verified;identity='c'*64;folder=process.ROOT/'backup-releases'/identity
    binding=dict(identity=identity,python=str(runtime['python']))
    report=dict(schema=1,producer='tsfp-upgrade-job-specs-v1',kind='backup',identity=identity,
                base_release=runtime['identity'],uid=600,gid=600,kernel_groups=[600],
                entries=[dict(generated=str(process.ROOT/'backup-jobs'/identity/'worker.plist'),
                              specification=backup.job(folder,binding))])
    def capture(arguments,options,*,timeout,limit):
        assert arguments[0]==str(runtime['python']) and arguments[-2:]==['--backup-release',identity]
        return json.dumps(report).encode()
    monkeypatch.setattr(process,'_capture',capture)
    assert process.job_specs(runtime,current,600,backup_identity=identity)[0][1]['Label']=='org.timesplitters.workshop.backup'


@pytest.mark.parametrize('retain_context',[False,True,'inputs'])
def test_preflight_verifies_independent_backup_binding_and_combines_only_attested_job_metadata(monkeypatch,retain_context):
    from types import SimpleNamespace
    spec=importlib.util.spec_from_file_location('upgrade_preflight_composition',SOURCE/'tools/mods/hosting/upgrade-installed-host-mac.py')
    upgrade=importlib.util.module_from_spec(spec);spec.loader.exec_module(upgrade)
    before,after,retained,backup_id='a'*64,'b'*64,'c'*64,'d'*64
    def entry(identity):return dict(identity=identity,root=process.ROOT/'releases'/identity,uid=600,
        python=process.ROOT/'releases'/identity/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12')
    old,new,backup_runtime=entry(before),entry(after),entry(retained)
    new['descriptor']=dict(source_files={key:{} for key in ('tools/mods/mod_host_upgrade_process.py',
        'tools/mods/mod_host_upgrade_context.py',
        'tools/mods/mod_host_cutover_journal.py','tools/mods/mod_host_upgrade_coordinator.py',
        'tools/mods/mod_host_upgrade_admission.py','tools/mods/mod_host_upgrade_activation.py',
        'tools/mods/mod_host_upgrade_layout.py','tools/mods/mod_host_upgrade_shutdown.py',
        'tools/mods/mod_host_migration.py','tools/mods/hosting/force-upgrade-backup-mac.py',
        'tools/mods/hosting/accept-installed-scanner-mac.py','tools/mods/hosting/accept-private-host-services-mac.py',
        'tools/mods/mod_host_cutover.py','tools/mods/mod_backup_release.py','tools/mods/mod_host_routing.py',
        'tools/mods/hosting/upgrade-job-specs-mac.py','tools/mods/hosting/migrate-private-generation-mac.py')})
    profile=dict(origin='https://workshop.example.test:8443',trusted_root_sha256='f'*64)
    monkeypatch.setattr(upgrade,'verified_pair',lambda *args:(old,new,profile))
    monkeypatch.setattr(upgrade.release,'installed',lambda identity,uid:backup_runtime if identity==retained and uid==600 else pytest.fail('Unexpected retained runtime'))
    monkeypatch.setattr(upgrade.release,'INSTALLED_ROOT',process.ROOT)
    monkeypatch.setattr(sys,'path',list(sys.path))
    calls=[]
    old_entries=[(Path('/old-generated.plist'),dict(Label='old'))]
    backup_entries=[(Path('/backup-generated.plist'),dict(Label='backup'))]
    candidate_entries=[(Path('/new-generated.plist'),dict(Label='new'))]
    def job_specs(runtime,current,uid,**kwargs):
        assert current is new and uid==600
        calls.append((runtime['identity'],kwargs))
        return old_entries if runtime is old else candidate_entries if runtime is new else backup_entries
    observation=dict(files={'only':'attested'},observation=dict(tracked=dict(pids=[123],groups=[123])))
    def observed(entries,uid,command):
        if retain_context=='inputs':pytest.fail('Live observation in pure input verification')
        assert entries==old_entries+backup_entries and uid==600
        return observation
    def validate(entries):assert entries==candidate_entries+backup_entries
    private=dict(catalog_version=42)
    plan=dict(sha256='9'*64,manifest={'retained':'plan'})
    def initial_plan(*args):
        if retain_context=='inputs':pytest.fail('Private migration inspection in pure input verification')
        return plan
    def initial_readiness(*args):
        if retain_context=='inputs':pytest.fail('Private readiness in pure input verification')
        return private
    monkeypatch.setitem(sys.modules,'mod_host_upgrade_process',SimpleNamespace(job_specs=job_specs,
        migration_plan=initial_plan,private_readiness=initial_readiness))
    monkeypatch.setitem(sys.modules,'mod_host_cutover',SimpleNamespace(preflight=observed,validate_entries=validate))
    binding=dict(uid=600,gid=600,base_release=retained,python=str(backup_runtime['python']))
    monkeypatch.setitem(sys.modules,'mod_backup_release',SimpleNamespace(installed=lambda folder:({},binding)))
    monkeypatch.setitem(sys.modules,'mod_host_watch',SimpleNamespace(run_command=lambda _:pytest.fail('Unexpected real command')))
    route=dict(schema=1,origin=profile['origin'],phase='public',config_sha256='e'*64,unrelated_sha256='f'*64)
    def invoke(action):
        if retain_context=='inputs':pytest.fail('Routing observation in pure input verification')
        return b'{}'
    monkeypatch.setitem(sys.modules,'mod_host_routing',SimpleNamespace(operator=lambda *_:invoke,snapshot=lambda *_:route))
    if retain_context=='inputs':
        fresh=upgrade.verified_inputs(before,after,600,backup_id)
        assert fresh['old'] is old and fresh['new'] is new and fresh['backup_runtime'] is backup_runtime
        assert fresh['operator'] is invoke and fresh['candidate_entries']==candidate_entries+backup_entries
        assert not {'observed','routing','plan','private_baseline','report'} & fresh.keys()
        assert calls==[(before,{}),(retained,dict(backup_identity=backup_id)),(after,{})]
        return
    if retain_context:
        context=upgrade.prepare_context(before,after,600,backup_id)
        assert context['old'] is old and context['new'] is new and context['profile'] is profile
        assert context['backup_runtime'] is backup_runtime and context['backup_binding'] is binding
        assert context['operator'] is invoke and context['observed'] is observation
        assert context['entries']==old_entries+backup_entries
        assert context['candidate_entries']==candidate_entries+backup_entries
        assert context['plan'] is plan and context['private_baseline'] is private and context['routing'] is route
        report=context['report']
    else:report=upgrade.preflight(before,after,600,backup_id)
    assert calls==[(before,{}),(retained,dict(backup_identity=backup_id)),(after,{})]
    assert report['jobs']==10 and report['protected_job_files']==20 and report['backup_base_release']==retained
    assert len(report['preflight_sha256'])==64 and not report['data_written'] and not report['services_changed']
    assert report['public_route_verified'] and report['unrelated_routing_sha256']=='f'*64
    assert report['private_baseline']==private
    assert not {'old','new','operator','command','observed','plan','entries','candidate_entries'} & report.keys()
    json.dumps(report)  # Public CLI output remains metadata-only JSON.
    # A changed target factory must alter admission even if running jobs,
    # routing, private readiness and the migration plan are unchanged.
    candidate_entries[0][1]['Label']='changed-new'
    assert upgrade.preflight(before,after,600,backup_id)['preflight_sha256']!=report['preflight_sha256']


def test_job_reader_default_plan_and_ordinary_active_refusal(monkeypatch):
    path=SOURCE/'tools/mods/hosting/upgrade-job-specs-mac.py'
    result=subprocess.run([sys.executable,'-I','-S','-B',str(path)],capture_output=True,check=True,timeout=10)
    assert json.loads(result.stdout)==dict(schema=1,phase='plan-only',services_changed=False,data_written=False)
    spec=importlib.util.spec_from_file_location('job_reader_guard_test',path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    monkeypatch.setattr(module.sys,'platform','darwin');monkeypatch.setattr(module.os,'geteuid',lambda:501,raising=False)
    monkeypatch.setattr(module,'protected_program',lambda _:pytest.fail('Read protected code before identity admission'))
    with pytest.raises(ValueError,match='isolated locked'):module.active('host','a'*64)
