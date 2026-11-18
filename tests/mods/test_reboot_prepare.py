"""Real Root history/files, simulated restart, routing, launchd and processes."""
import copy
import json
from pathlib import Path
import plistlib
import sys
from types import SimpleNamespace

import pytest
from test_host_cutover import protected_jobs
from test_upgrade_context import saved_inputs

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_migration as migration
import mod_host_reboot_observation as observation
import mod_host_reboot_prepare as prepare
import mod_host_upgrade_context as contexts
from mod_package import ModError


@pytest.fixture
def restarted(saved_inputs,workshop_routing,monkeypatch):
    item=saved_inputs;arguments=item['arguments'];context=item['context'];fresh=dict(item['fresh'])
    tracked=dict(pids=[123],groups=[123]);arguments['tracked']=tracked
    context['observed']['observation']['tracked']=tracked
    first=next(iter(context['observed']['observation']['jobs']))
    context['observed']['observation']['jobs'][first]['pid']=123
    fingerprint=contexts.preflight_digest(context['observed'],fresh['candidate_entries'],context['routing'],context['plan'],context['private_baseline'])
    arguments['preflight_sha256']=context['report']['preflight_sha256']=fingerprint
    with storage.transaction(**arguments) as journal:
        contexts.seal(journal,context);journal.record('baseline-private-verified',data=context['private_baseline'])
        original=journal.current
    current=['e'*64];loaded={};stops=[];actions=[];flags={};ticks=[0]
    for module in (storage,migration,observation,prepare):monkeypatch.setattr(module,'boot_identity',lambda:current[0])
    def populate(start=500):
        loaded.clear()
        for index,((path,old),(_,new)) in enumerate(zip(fresh['entries'],fresh['candidate_entries'])):
            selected=new if index%2 else old;label=selected['Label']
            target=cutover.LAUNCHD/(label+'.plist');target.write_bytes(plistlib.dumps(selected));target.chmod(0o644)
            loaded[label]=(path,selected,start+index)
    populate()
    def command(argv):
        if argv[0]=='/bin/ps':
            assert argv==['/bin/ps','-axo','pid=,ppid=,pgid=,uid=']
            rows='1 0 1 0\n123 1 123 501\n'  # A reused pre-boot PID is unrelated.
            for label,(_,_,pid) in loaded.items():rows+=f'{pid} 1 {pid} {0 if label.endswith(".monitor") else 600}\n'
            if any(pid==500 for _,_,pid in loaded.values()):rows+='800 500 500 600\n'
            elif flags.get('orphan'):rows+='800 1 800 600\n'
            return 0,rows
        assert argv[0]=='/bin/launchctl' and argv[1] in ('print','bootout')
        label=argv[-1].removeprefix('system/')
        if argv[1]=='bootout':
            stops.append(label)
            if not flags.get('stuck'):loaded.pop(label,None)
            return 5 if flags.get('stuck') else 0,''
        if label not in loaded:return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
        _,spec,pid=loaded[label];args=spec['ProgramArguments']
        return 0,('system/'+label+' = {\n\tpath = '+str(cutover.LAUNCHD/(label+'.plist'))
            +'\n\ttype = LaunchDaemon\n\tprogram = '+args[0]+'\n\targuments = {\n'
            +''.join('\t\t'+value+'\n' for value in args)+'\t}\n\tpid = '+str(pid)+'\n}\n')
    def routing(action):
        if action!='status':actions.append(action)
        result=workshop_routing['invoke'](action)
        if action=='suspend' and flags.get('replace-file'):
            path=cutover.LAUNCHD/(fresh['entries'][0][1]['Label']+'.plist');wire=path.read_bytes()
            path.rename(path.with_suffix('.preserved'));path.write_bytes(wire);path.chmod(0o644)
        return result
    fresh.update(command=command,operator=routing)
    def lease():return storage.reboot_observation_lease('c'*64,'a'*64,'b'*64,600,'a'*64,'f'*64,clock=lambda:1000000000)
    def run(journal,*,resume=False):
        return prepare.prepare(journal,fresh,resume=resume,clock=lambda:ticks[0],sleep=lambda seconds:ticks.__setitem__(0,ticks[0]+seconds),timeout=1)
    return SimpleNamespace(item=item,fresh=fresh,original=original,current=current,loaded=loaded,stops=stops,actions=actions,
        flags=flags,lease=lease,run=run,populate=populate,route=workshop_routing)


@pytest.mark.parametrize('already_off',[False,True])
def test_fresh_reboot_preparation_suspends_and_stops_only_current_fixed_jobs(restarted,already_off):
    case=restarted
    if already_off:case.route['invoke']('suspend')
    with case.lease() as journal:
        result=case.run(journal)
        assert result['phase']=='reboot-services-quiescent' and result['operational_admission'] is False
        state=journal.state();assert state['reboot']['quiescent'] and state['reboot']['observation']['tracked']['pids']==list(range(500,510))+[800]
        assert journal.base['tracked']==dict(pids=[123],groups=[123]) and journal.base['boot_id']=='f'*64
        assert state['config']=='before' and state['quiescent'] is False
        assert case.stops==storage.stop_order() and case.actions==([] if already_off else ['suspend']) and not case.loaded
        with pytest.raises(ModError):journal.record('settled')
        with pytest.raises(ModError):journal.record_reboot('settled')
        assert json.loads(journal.current)['events'][:1]==json.loads(case.original)['events']


@pytest.mark.parametrize('lost',['reboot-observed','reboot-routing-observed','reboot-stop-observed','reboot-quiescent'])
def test_lost_reboot_preparation_observation_recovers_under_a_new_lease_without_duplicate_effects(restarted,monkeypatch,lost):
    case=restarted;failed=[]
    with case.lease() as journal:
        record=journal.record_reboot
        def interrupt(kind,*args,**kwargs):
            if kind==lost and not failed:failed.append(True);raise OSError('lost observation')
            return record(kind,*args,**kwargs)
        monkeypatch.setattr(journal,'record_reboot',interrupt)
        with pytest.raises(OSError):case.run(journal)
    with case.lease() as journal:
        if lost!='reboot-observed':
            prior=list(case.stops)
            with pytest.raises(ModError):case.run(journal)
            assert case.stops==prior
        case.run(journal,resume=True)
        assert journal.state()['reboot']['quiescent']
    assert failed and case.stops==storage.stop_order() and case.actions==['suspend']


def test_uncertain_reboot_stop_is_observed_on_resume_without_another_bootout(restarted):
    case=restarted;case.flags['stuck']=True
    with case.lease() as journal:
        with pytest.raises(ModError,match='unconfirmed'):case.run(journal)
        pending=journal.state()['reboot']['pending'];assert case.stops==[pending]
    case.flags['stuck']=False
    with case.lease() as journal:
        with pytest.raises(ModError,match='unconfirmed'):case.run(journal,resume=True)
        assert case.stops==[pending]
    case.loaded.pop(pending)  # Authoritative simulated exit, not an inferred timeout outcome.
    with case.lease() as journal:case.run(journal,resume=True)
    assert case.stops==storage.stop_order()


@pytest.mark.parametrize('fault',['replace-file','orphan'])
def test_changed_files_or_escaped_current_workers_prevent_reboot_quiescence(restarted,fault):
    case=restarted;case.flags[fault]=True
    with case.lease() as journal:
        with pytest.raises(ModError):case.run(journal)
        assert not journal.state()['reboot']['quiescent']
        if fault=='replace-file':assert not case.stops
    if fault=='orphan':
        case.flags[fault]=False
        with case.lease() as journal:case.run(journal,resume=True)
        assert case.stops==storage.stop_order()


def test_another_restart_discards_preparation_pid_authority_but_keeps_original_history(restarted):
    case=restarted
    with case.lease() as journal:case.run(journal)
    case.current[0]='d'*64;case.populate(900)
    with case.lease() as journal:
        with pytest.raises(ModError):case.run(journal)
        case.run(journal,resume=True);state=journal.state()['reboot']
        assert state['boot_id']=='d'*64 and state['prior_boots']==['e'*64]
        assert state['observation']['tracked']['pids']==list(range(900,910))
        assert state['quiescent'] and journal.base['boot_id']=='f'*64
    assert case.stops==storage.stop_order()*2 and case.actions==['suspend']


def test_reboot_history_rejects_forged_observations_routing_order_and_ordinary_settlement(restarted):
    case=restarted
    with case.lease() as journal:
        case.run(journal);value=json.loads(journal.current)
        observed=next(i for i,e in enumerate(value['events']) if e['kind']=='reboot-observed')
        route=next(i for i,e in enumerate(value['events']) if e['kind']=='reboot-routing-observed')
        stop=next(i for i,e in enumerate(value['events']) if e['kind']=='reboot-stop-intent')
        for fault in ('uid','boot','file-set','file-owner','file-stamp','job-pid','tracking','context',
                      'exposure','unrelated-route','still-public','stop-order','missing-stop','settled'):
            changed=copy.deepcopy(value);data=changed['events'][observed]['data'];snapshot=data['observation']
            first_file=next(iter(snapshot['files']));first_job=next(iter(snapshot['observation']['jobs']))
            if fault=='uid':snapshot['uid']=True
            elif fault=='boot':snapshot['boot_id']=snapshot['prior_boot']
            elif fault=='file-set':snapshot['files']['/foreign.plist']=snapshot['files'].pop(first_file)
            elif fault=='file-owner':snapshot['files'][first_file]['stamp'][5]=600
            elif fault=='file-stamp':snapshot['files'][first_file]['stamp'][0]=True
            elif fault=='job-pid':snapshot['observation']['jobs'][first_job]['pid']=True
            elif fault=='tracking':snapshot['tracked']=dict(pids=[],groups=[])
            elif fault=='context':data['context_sha256']='z'*64
            elif fault=='exposure':changed['events'][route]['data']['expose']=True
            elif fault=='unrelated-route':changed['events'][route]['data']['unrelated_sha256']='0'*64
            elif fault=='still-public':
                changed['events'][route]['data'].update(phase='public',config_sha256=journal.base['routing']['config_sha256'])
            elif fault=='stop-order':changed['events'][stop]['label']=storage.stop_order()[1]
            elif fault=='missing-stop':del changed['events'][stop+1]
            elif fault=='settled':changed['events'].append(dict(kind='settled',label=None,time=1000000000,data=None))
            with pytest.raises(ModError):storage.checked(storage.wire(changed),journal.clock)
        different=copy.deepcopy(value['events'][observed]['data']);different['observation']['boot_id']='d'*64
        before=journal.current
        with pytest.raises(ModError):journal.record_reboot('reboot-observed',data=different)
        assert journal.current==before
