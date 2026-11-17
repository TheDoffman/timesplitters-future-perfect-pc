"""Actual protected filesystem exchange; simulated authority and effect journal."""
import os
from pathlib import Path
import plistlib
import sys
from types import SimpleNamespace

import pytest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_upgrade_layout as layout
from mod_package import ModError


@pytest.fixture
def prepared(private_posix_path,monkeypatch):
    root=private_posix_path/'workshop';root.mkdir(mode=0o755)
    boot=private_posix_path/'boot';boot.mkdir(mode=0o755)
    (root/'upgrades').mkdir(mode=0o700)
    monkeypatch.setattr(cutover,'ROOT',root);monkeypatch.setattr(cutover,'LAUNCHD',boot)
    monkeypatch.setattr(cutover,'ROOT_UID',os.geteuid());monkeypatch.setattr(layout,'boot_identity',lambda:'f'*64)
    before=root/'releases'/('a'*64);after=root/'releases'/('b'*64)
    def entries(release):
        result=[]
        for label in cutover.jobs():
            role=label.split('.')[3];monitor=label.endswith('.monitor')
            if role=='backup':path=root/'backup-jobs'/('8'*64)/'worker.plist'
            elif role=='scan':path=root/'monitor'/('monitor.plist' if monitor else 'scan.plist')
            elif role=='definitions':path=root/'monitor/definitions.plist'
            else:path=root/'monitor'/role/('monitor.plist' if monitor else 'worker.plist')
            generation=before if role=='backup' else release
            result.append((path,dict(Label=label,ProgramArguments=[str(generation/'runtime/Frameworks/Python.framework/Versions/3.12/bin/python3.12'),
                '-I','-S','-B',str(generation/'source/bootstrap.py'),'--role',role])))
        return result
    old=entries(before);new=entries(after)
    (root/'monitor').mkdir(mode=0o755)
    for role in ('api','publication','gateway'):(root/'monitor'/role).mkdir(mode=0o755)
    def write(path,data,mode):
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);path.chmod(mode)
    for name,data in layout.payloads(old,before,600).items():write(root/'monitor'/name,data,0o600)
    for path,spec in old:
        wire=plistlib.dumps(spec)
        if '.backup' in spec['Label']:write(path,wire,0o444)
        write(boot/(spec['Label']+'.plist'),wire,0o644)
    # Unrecognized root-owned historical records must survive whole-directory archival.
    (root/'monitor/deployments').mkdir(mode=0o700)
    write(root/'monitor/deployments/old-receipt.json',b'{"historical":true}',0o600)
    write(root/'monitor/api/health.json',b'{"old-health":true}',0o644)
    base=dict(before=before.name,after=after.name,uid=600,nonce='c'*64,boot_id='f'*64)
    journal=SimpleNamespace(base=base,state=lambda:dict(quiescent=True))
    exchange=layout.Layout(journal,before,after,old,new);fingerprint=exchange.prepare()
    original=layout.snapshot(root/'monitor');events=[];pending=[None];gates=[]
    def record(kind,operation,identity):
        assert identity==fingerprint
        if kind=='intent':assert pending[0] is None;pending[0]=operation
        else:assert pending[0]==operation;pending[0]=None
        events.append((kind,operation))
    def gate():gates.append(True)
    return SimpleNamespace(exchange=exchange,root=root,boot=boot,fingerprint=fingerprint,original=original,
        events=events,pending=pending,record=record,gate=gate,gates=gates,journal=journal,old=old,new=new)


def test_exchange_and_rollback_preserve_history_backup_and_exact_original_directory(prepared):
    case=prepared;item=case.exchange
    result=item.exchange('apply',case.record,case.gate)
    assert result['boot_files']==9 and result['backup_changed'] is False and result['services_started'] is False
    assert layout.snapshot(item.saved)==case.original
    assert not (case.root/'monitor/api/health.json').exists()
    assert len(case.events)==22 and len(case.gates)==12
    for path,spec in case.new:
        assert (case.boot/(spec['Label']+'.plist')).read_bytes()==plistlib.dumps(spec)
    result=item.exchange('rollback',case.record,case.gate)
    assert result['phase']=='protected-layout-rolled-back'
    assert layout.snapshot(case.root/'monitor')==case.original
    assert (case.root/'monitor/deployments/old-receipt.json').read_bytes()==b'{"historical":true}'
    assert len(case.events)==44 and case.pending[0] is None


@pytest.mark.parametrize('operation',['archive','install','boot'])
@pytest.mark.parametrize('direction',['apply','rollback'])
def test_interrupted_observation_requires_explicit_resume_and_records_known_outcome(prepared,operation,direction):
    case=prepared;item=case.exchange
    if direction=='rollback':item.exchange('apply',case.record,case.gate)
    def record(kind,name,identity):
        if kind=='observed' and ((operation=='boot' and '.workshop.' in name) or name==direction+'-'+operation):
            raise OSError('interrupted after effect before observation')
        case.record(kind,name,identity)
    with pytest.raises(OSError):item.exchange(direction,record,case.gate)
    pending=case.pending[0];assert pending
    with pytest.raises(ModError):item.exchange(direction,case.record,case.gate)
    reopened=layout.Layout(case.journal,case.root/'releases'/('a'*64),case.root/'releases'/('b'*64),case.old,case.new)
    reopened.reopen(case.fingerprint)
    reopened.exchange(direction,case.record,case.gate,resume=True,pending=pending)
    assert case.pending[0] is None
    assert reopened._locations()['active']==('after' if direction=='apply' else 'before')


@pytest.mark.parametrize('damage',['staged-file','staged-link','staged-clone','foreign-boot','backup-identical-replacement','manifest','gate'])
def test_unknown_or_changed_files_and_failed_quiescence_preserve_active_generation(prepared,damage):
    case=prepared;item=case.exchange;gate=case.gate
    if damage=='staged-file':(item.staged/'monitor.json').write_bytes(b'foreign')
    elif damage=='staged-link':(item.staged/'extra').symlink_to(case.root/'monitor')
    elif damage=='staged-clone':
        path=item.staged/'monitor.json';data=path.read_bytes();path.rename(case.root/'preserved-marker.json');path.write_bytes(data);path.chmod(0o600)
    elif damage in ('foreign-boot','backup-identical-replacement'):
        label=next(spec['Label'] for _,spec in case.old if spec['Label'].endswith('.backup')==(damage=='backup-identical-replacement'))
        path=case.boot/(label+'.plist')
        data=path.read_bytes() if damage=='backup-identical-replacement' else b'foreign'
        path.rename(path.with_suffix('.saved'));path.write_bytes(data);path.chmod(0o644)
    elif damage=='manifest':(item.folder/'manifest.json').write_bytes(b'{}')
    else:
        def gate():raise ModError('public routing or a worker reappeared')
    with pytest.raises((ModError,OSError)):item.exchange('apply',case.record,gate)
    assert layout.snapshot(case.root/'monitor')==case.original
    assert not item.saved.exists() and case.events==[]


def test_intent_callback_failure_precedes_every_filesystem_effect(prepared):
    case=prepared
    def failed(*args):raise OSError('journal unavailable')
    with pytest.raises(OSError):case.exchange.exchange('apply',failed,case.gate)
    assert layout.snapshot(case.root/'monitor')==case.original and not case.exchange.saved.exists()


@pytest.mark.parametrize('operation',['archive','install','boot'])
@pytest.mark.parametrize('direction',['apply','rollback'])
def test_durable_intent_before_interrupted_effect_resumes_without_duplicate_intent(prepared,operation,direction):
    case=prepared
    if direction=='rollback':case.exchange.exchange('apply',case.record,case.gate)
    def record(kind,name,identity):
        case.record(kind,name,identity)
        if kind=='intent' and ((operation=='boot' and '.workshop.' in name) or name==direction+'-'+operation):
            raise OSError('interrupted after durable intent before effect')
    with pytest.raises(OSError):case.exchange.exchange(direction,record,case.gate)
    pending=case.pending[0];assert pending
    count=case.events.count(('intent',pending))
    case.exchange.exchange(direction,case.record,case.gate,resume=True,pending=pending)
    assert case.events.count(('intent',pending))==count==1 and case.pending[0] is None


@pytest.mark.parametrize('damage',['digest','boot','foreign-backup'])
def test_reopen_refuses_unbound_or_foreign_layout_context(prepared,damage):
    case=prepared;fingerprint=case.fingerprint
    if damage=='digest':fingerprint='0'*64
    if damage=='boot':case.journal.base['boot_id']='0'*64
    if damage=='foreign-backup':
        index=next(i for i,(_,spec) in enumerate(case.new) if spec['Label'].endswith('.backup'))
        case.new[index]=(case.new[index][0],dict(case.new[index][1],ProgramArguments=['/foreign/python']))
    with pytest.raises(ModError):
        reopened=layout.Layout(case.journal,case.root/'releases'/('a'*64),case.root/'releases'/('b'*64),case.old,case.new)
        reopened.reopen(fingerprint)
    assert layout.snapshot(case.root/'monitor')==case.original


def test_pending_boot_write_is_preserved_and_never_implicitly_adopted(prepared,monkeypatch):
    case=prepared
    def failed(*args,**kwargs):raise OSError('boot rename interrupted')
    with monkeypatch.context() as patch:
        patch.setattr(layout.os,'replace',failed)
        with pytest.raises(OSError):case.exchange.exchange('apply',case.record,case.gate)
    pending=case.pending[0];temporaries=list(case.boot.glob('.*.upgrade-*'))
    assert len(temporaries)==1;data=temporaries[0].read_bytes()
    with pytest.raises(FileExistsError):case.exchange.exchange('apply',case.record,case.gate,resume=True,pending=pending)
    assert temporaries[0].read_bytes()==data and layout.snapshot(case.exchange.saved)==case.original


@pytest.mark.parametrize('damage',[None,'uncertain-label','service-process','old-group','routing','boot'])
def test_live_gate_checks_all_fixed_labels_process_groups_uid_and_routing(prepared,damage):
    case=prepared;calls=[];route_checks=[]
    case.journal.base['tracked']=dict(pids=[12],groups=[12])
    def suspended(routing):
        route_checks.append(True)
        if damage=='routing':raise ModError('route changed')
    case.journal.require_suspended=suspended
    if damage=='boot':case.journal.base['boot_id']='0'*64
    def command(arguments):
        calls.append(arguments)
        if arguments[0]=='/bin/ps':
            rows='1 0 1 0\n'
            if damage=='service-process':rows+='99 1 99 600\n'
            if damage=='old-group':rows+='99 1 12 501\n'
            return 0,rows
        label=arguments[-1].removeprefix('system/')
        if damage=='uncertain-label':return 1,'unrecognized error'
        return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
    if damage:
        with pytest.raises(ModError):layout.quiescence_gate(case.journal,case.old,command,None)
    else:
        layout.quiescence_gate(case.journal,case.old,command,None)
        assert len(calls)==21 and len(route_checks)==2
