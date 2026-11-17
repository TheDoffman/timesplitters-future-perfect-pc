"""Protected metadata persistence; simulated authority and sealed factory inputs."""
import json
import importlib.util
import os
import subprocess
from pathlib import Path
import sys

import pytest
from test_host_cutover import protected_jobs
from test_upgrade_admission import plan_report

SOURCE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(SOURCE/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_migration as migration
import mod_host_upgrade_context as contexts
from mod_package import ModError


@pytest.fixture
def saved_inputs(protected_jobs,workshop_routing,monkeypatch):
    entries=protected_jobs
    monkeypatch.setattr(storage,'boot_identity',lambda:'f'*64)
    monkeypatch.setattr(migration,'boot_identity',lambda:'f'*64)
    candidate=[(path,dict(spec,KeepAlive=True,ProgramArguments=['/candidate/python',*spec['ProgramArguments'][1:]]))
        if not spec['Label'].endswith('.backup') else (path,spec) for path,spec in entries]
    profile=dict(origin=workshop_routing['baseline']['origin'],trusted_root_sha256='9'*64)
    plan=plan_report(profile['origin'])
    private=dict(schema=1,phase='private-services-verified',release='a'*64,uid=600,**profile,catalog_version=42,
        release_count=3,role_health_verified=True,transport='private-loopback-http',public_endpoint_verified=False)
    observed=dict(files=cutover.job_files(entries),observation=dict(tracked=dict(pids=[],groups=[]),
        jobs={spec['Label']:dict(path=str(path),pid=None) for path,spec in entries}))
    route=workshop_routing['baseline']
    arguments=dict(before='a'*64,after='b'*64,uid=600,nonce='c'*64,plan_sha256=plan['sha256'],
        preflight_sha256=contexts.preflight_digest(observed,candidate,route,plan,private),
        backup_identity='a'*64,backup_base_release='7'*64,tracked=dict(pids=[],groups=[]),routing=route,clock=lambda:1000000000)
    def release(identity):return dict(identity=identity,root=cutover.ROOT/'releases'/identity,uid=600,python=cutover.ROOT/identity/'python')
    runtime=release('7'*64)
    fresh=dict(old=release('a'*64),new=release('b'*64),profile=profile,backup_runtime=runtime,
        backup_binding=dict(uid=600,gid=600,base_release='7'*64,python=str(runtime['python'])),
        entries=entries,candidate_entries=candidate,operator=workshop_routing['invoke'],command=lambda _:pytest.fail('Executed command during metadata reconstruction'))
    context=dict(fresh,plan=plan,private_baseline=private,observed=observed,routing=route,
        report={key:value for key,value in arguments.items() if key not in ('clock','nonce','tracked','routing')})
    path=cutover.ROOT/'upgrade-contexts'/('c'*64+'.json')
    return dict(arguments=arguments,context=context,fresh=fresh,path=path)


def test_exact_context_survives_lease_reopen_without_private_inspection_or_stored_commands(saved_inputs):
    item=saved_inputs
    with storage.transaction(**item['arguments']) as journal:
        sha=contexts.seal(journal,item['context'])
        before=item['path'].read_bytes()
        assert len(sha)==64 and item['path'].stat().st_mode&0o777==0o600
        assert item['path'].parent.stat().st_mode&0o777==0o700
        with pytest.raises(ModError):contexts.seal(journal,item['context'])
        assert contexts.seal(journal,item['context'],resume=True)==sha
        journal.record('baseline-private-verified',data=item['context']['private_baseline'])
    # New caller-provided transports/factories replace the old in-memory objects.
    invoke=lambda _:pytest.fail('Historical metadata must not execute a transport')
    fresh=dict(item['fresh'],operator=invoke)
    with storage.transaction(**item['arguments']) as journal:
        restored=contexts.reconstruct(journal,fresh)
        assert restored['operator'] is invoke and restored['entries'] is fresh['entries']
        assert restored['plan']==item['context']['plan'] and restored['private_baseline']['catalog_version']==42
        assert restored['observed']['observation']['tracked']==journal.base['tracked']
        assert restored['report']['phase']=='historical-upgrade-context-reconstructed'
        assert item['path'].read_bytes()==before


def test_separate_process_reconstructs_metadata_using_new_factory_objects(saved_inputs):
    item=saved_inputs
    with storage.transaction(**item['arguments']) as journal:contexts.seal(journal,item['context'])
    packet=dict(root=str(cutover.ROOT),boot=str(cutover.LAUNCHD),
        arguments={k:v for k,v in item['arguments'].items() if k!='clock'},
        fresh={k:v for k,v in item['fresh'].items() if k not in ('entries','candidate_entries','operator','command')},
        jobs=contexts.metadata(item['fresh']['entries']),candidate_jobs=contexts.metadata(item['fresh']['candidate_entries']))
    fixture=cutover.ROOT/'fresh-caller.json';fixture.write_text(json.dumps(packet,default=str));fixture.chmod(0o600)
    code='''
import json,os,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import mod_host_cutover as cutover
import mod_host_cutover_journal as journal
import mod_host_migration as migration
import mod_host_upgrade_context as contexts
packet=json.loads(Path(sys.argv[2]).read_bytes())
cutover.ROOT=migration.ROOT=Path(packet['root']);cutover.LAUNCHD=Path(packet['boot']);cutover.ROOT_UID=os.geteuid()
journal.boot_identity=migration.boot_identity=lambda:'f'*64
fresh=packet['fresh']
for name in ('old','new','backup_runtime'):
    for key in ('root','python'):fresh[name][key]=Path(fresh[name][key])
fresh.update(entries=[(Path(e['generated']),e['specification']) for e in packet['jobs']],
    candidate_entries=[(Path(e['generated']),e['specification']) for e in packet['candidate_jobs']],
    operator=lambda _:None,command=lambda _:None)
with journal.transaction(**packet['arguments'],clock=lambda:1000000000) as state:
    result=contexts.reconstruct(state,fresh)
    print(json.dumps(dict(version=result['private_baseline']['catalog_version'],plan=result['plan']['sha256'],phase=result['report']['phase'])))
'''
    result=subprocess.run([sys.executable,'-I','-B','-c',code,str(SOURCE/'tools/mods'),str(fixture)],
        capture_output=True,text=True,timeout=15)
    assert result.returncode==0,result.stderr
    assert json.loads(result.stdout)==dict(version=42,plan=item['context']['plan']['sha256'],phase='historical-upgrade-context-reconstructed')


def test_installed_recovery_composition_reverifies_inputs_without_initial_private_preflight(saved_inputs,monkeypatch):
    import mod_host_upgrade_process as process
    item=saved_inputs;calls=[]
    spec=importlib.util.spec_from_file_location('recovery_context_entry',SOURCE/'tools/mods/hosting/upgrade-installed-host-mac.py')
    entry=importlib.util.module_from_spec(spec);spec.loader.exec_module(entry)
    def verified(*args):calls.append(args);return dict(item['fresh'])
    def forbidden(*args,**kwargs):pytest.fail('Initial private/live preflight was invoked during recovery')
    monkeypatch.setattr(entry,'verified_inputs',verified)
    monkeypatch.setattr(process,'migration_plan',forbidden)
    monkeypatch.setattr(process,'private_readiness',forbidden)
    monkeypatch.setattr(cutover,'preflight',forbidden)
    with storage.transaction(**item['arguments']) as journal:
        contexts.seal(journal,item['context'])
        before=journal.current
        with pytest.raises(ValueError):entry.prepare_recovery_context('0'*64,'b'*64,600,'a'*64,journal)
        assert not calls
        restored=entry.prepare_recovery_context('a'*64,'b'*64,600,'a'*64,journal)
        assert calls==[('a'*64,'b'*64,600,'a'*64)] and restored['plan']==item['context']['plan']
        assert journal.current==before


@pytest.mark.parametrize('damage',['mode','link','hardlink','fifo','partial','nonce','extra','plan','observed','jobs','private','noncanonical','bool-alias'])
def test_unknown_or_damaged_context_is_preserved_and_never_reconstructed(saved_inputs,damage):
    item=saved_inputs
    with storage.transaction(**item['arguments']) as journal:
        contexts.seal(journal,item['context']);path=item['path']
        if damage=='mode':path.chmod(0o644)
        elif damage in ('link','hardlink','fifo'):
            retained=cutover.ROOT/'preserved-context.json';path.rename(retained)
            if damage=='link':path.symlink_to(retained)
            elif damage=='hardlink':os.link(retained,path)
            else:os.mkfifo(path,0o600)
        elif damage=='partial':path.write_bytes(b'{')
        else:
            value=json.loads(path.read_bytes())
            if damage=='nonce':value['binding']['nonce']='0'*64
            elif damage=='extra':value['secret']='not a supported metadata field'
            elif damage=='plan':value['plan']['manifest']['origin']='https://foreign.invalid'
            elif damage=='observed':value['observed']['files'].pop(next(iter(value['observed']['files'])))
            elif damage=='jobs':value['jobs'][0]['specification']['ProgramArguments'][0]='/foreign/python'
            elif damage=='bool-alias':value['candidate_jobs'][0]['specification']['KeepAlive']=1
            elif damage=='private':value['private']['catalog_version']=True
            path.write_bytes(json.dumps(value).encode() if damage=='noncanonical' else storage.wire(value))
        before=os.lstat(path)
        with pytest.raises((ModError,OSError,ValueError)):contexts.reconstruct(journal,item['fresh'])
        assert os.lstat(path)==before and not json.loads(journal.current)['events']


def test_context_cannot_be_recreated_after_events_or_rebased_across_boot(saved_inputs,monkeypatch):
    item=saved_inputs
    with storage.transaction(**item['arguments']) as journal:
        journal.record('baseline-private-verified',data=item['context']['private_baseline'])
        with pytest.raises(ModError):contexts.seal(journal,item['context'],resume=True)
        assert not item['path'].exists()
        monkeypatch.setattr(migration,'boot_identity',lambda:'0'*64)
        with pytest.raises(ModError):contexts.reconstruct(journal,item['fresh'])


@pytest.mark.parametrize('damage',['unknown-name','capacity','directory-mode','changed-candidate'])
def test_context_namespace_and_fresh_factory_guards_refuse_before_writing(saved_inputs,damage):
    item=saved_inputs;folder=item['path'].parent;folder.mkdir(mode=0o700)
    if damage=='unknown-name':(folder/'foreign').write_bytes(b'preserve')
    elif damage=='capacity':
        for i in range(contexts.MAX_CONTEXTS):(folder/(format(i,'064x')+'.json')).write_bytes(b'preserve')
    elif damage=='directory-mode':folder.chmod(0o755)
    else:item['context']['candidate_entries'][0][1]['ProgramArguments'][0]='/changed/python'
    names=sorted(p.name for p in folder.iterdir())
    with storage.transaction(**item['arguments']) as journal:
        with pytest.raises(ModError):contexts.seal(journal,item['context'])
        assert sorted(p.name for p in folder.iterdir())==names and not item['path'].exists()
