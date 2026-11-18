"""Protected monitor evidence after simulated reboot; observation never moves files."""
import json
from pathlib import Path
import sys
from types import SimpleNamespace

import pytest
from test_upgrade_layout import prepared

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_reboot_layout as reboot
import mod_host_reboot_observation as observations
import mod_host_upgrade_layout as layouts
from mod_package import ModError


@pytest.fixture
def stopped_layout(prepared,workshop_routing,monkeypatch,request):
    case=prepared;phase=getattr(request,'param','before')
    if phase in ('after','archive','install','boot'):
        def record(kind,name,identity):
            if kind=='observed' and ((phase=='archive' and name=='apply-archive')
                or (phase=='install' and name=='apply-install') or (phase=='boot' and name.startswith('apply-org.'))):
                raise OSError('lost pre-boot effect observation')
            case.record(kind,name,identity)
        if phase=='after':case.exchange.exchange('apply',case.record,case.gate)
        else:
            with pytest.raises(OSError):case.exchange.exchange('apply',record,case.gate)
    monkeypatch.setattr(layouts,'boot_identity',lambda:'e'*64)
    monkeypatch.setattr(reboot,'boot_identity',lambda:'e'*64)
    choices=observations.policies(case.old,case.new)
    files=json.loads(storage.wire(observations.files(choices)))
    reboot_state=dict(boot_id='e'*64,quiescent=True,context_sha256='9'*64,
                      observation=dict(files=files,tracked=dict(pids=[],groups=[])))
    state=dict(reboot=reboot_state,layout_sha256=case.fingerprint,monitor_checkpoints={})
    base=dict(case.journal.base,routing=workshop_routing['baseline'])
    journal=SimpleNamespace(base=base,state=lambda:state,observation_boot='e'*64,clock=lambda:1000000000)
    monkeypatch.setattr(reboot.contexts,'read_reboot_context',lambda *_:dict(context_sha256='9'*64))
    workshop_routing['invoke']('suspend');calls=[];extra=[]
    def command(argv):
        calls.append(argv)
        if argv[0]=='/bin/ps':return 0,'1 0 1 0\n'+''.join(extra)
        assert argv[:2]==['/bin/launchctl','print']
        label=argv[-1].removeprefix('system/')
        return 113,'Bad request.\nCould not find service "'+label+'" in domain for system\n'
    fresh=dict(entries=case.old,candidate_entries=case.new,command=command,operator=workshop_routing['invoke'])
    return SimpleNamespace(case=case,journal=journal,fresh=fresh,state=state,calls=calls,extra=extra,route=workshop_routing,phase=phase)


def write_health(item,generation,*,fault=None):
    case=item.case;entries=case.old if generation=='before' else case.new
    label=next(spec['Label'] for _,spec in entries if spec['Label'].split('.')[3]=='api' and not spec['Label'].endswith('.monitor'))
    value=dict(schema=1,uid=600,service=label,role='api',policy='root-system-host-v1',
               release=str(case.root/'releases'/item.journal.base[generation]),time=1000000000,status='terminated',pid=901)
    if fault=='uid':value['uid']=True
    elif fault=='generation':value['release']=str(case.root/'releases'/item.journal.base['after' if generation=='before' else 'before'])
    elif fault=='time':value['time']=1000000001
    elif fault=='unknown':value['private_config']='canary'
    path=case.root/'monitor/api/health.json';path.write_bytes(json.dumps(value).encode());path.chmod(0o644)


@pytest.mark.parametrize('stopped_layout',['before','after','archive','install','boot'],indirect=True)
def test_reboot_layout_observes_known_partial_positions_and_preserves_health_evidence(stopped_layout):
    item=stopped_layout;active=None if item.phase=='archive' else 'before' if item.phase=='before' else 'after'
    if active:
        write_health(item,active)
        partial=item.case.root/'monitor'/('.scan-'+'1'*32);partial.write_bytes(b'{');partial.chmod(0o600)
    manifest=(item.case.exchange.folder/'manifest.json').read_bytes()
    before={name:layouts.snapshot(path) for name,path in (('active',item.case.root/'monitor'),('saved',item.case.exchange.saved),('staged',item.case.exchange.staged)) if path.exists()}
    result=reboot.read(item.journal,item.fresh)
    assert result['locations']['active']==active and result['operational_admission'] is False and result['files_changed'] is False
    assert result['inventories']==before and (item.case.exchange.folder/'manifest.json').read_bytes()==manifest
    for name,path in (('active',item.case.root/'monitor'),('saved',item.case.exchange.saved),('staged',item.case.exchange.staged)):
        if path.exists():assert layouts.snapshot(path)==before[name]
    assert all(argv[0]=='/bin/ps' or argv[:2]==['/bin/launchctl','print'] for argv in item.calls)


@pytest.mark.parametrize('fault',['uid','generation','time','unknown','unknown-file','policy','backup','not-quiescent','orphan','public'])
def test_bad_reboot_monitor_or_process_evidence_is_preserved_without_authority(stopped_layout,fault):
    item=stopped_layout
    if fault in ('uid','generation','time','unknown'):write_health(item,'before',fault=fault)
    elif fault=='unknown-file':
        path=item.case.root/'monitor/unknown.json';path.write_bytes(b'preserve');path.chmod(0o600)
    elif fault=='policy':
        path=item.case.root/'monitor/api/monitor.json';path.write_bytes(b'{}');path.chmod(0o600)
    elif fault=='backup':
        path=next(path for path,spec in item.case.old if spec['Label'].endswith('.backup'))
        path.chmod(0o600);path.write_bytes(b'unknown');path.chmod(0o444)
    elif fault=='not-quiescent':item.state['reboot']['quiescent']=False
    elif fault=='orphan':item.extra.append('999 1 999 600\n')
    elif fault=='public':item.route['invoke']('restore')
    original=layouts.snapshot(item.case.root/'monitor')
    with pytest.raises((ModError,OSError)):reboot.read(item.journal,item.fresh)
    assert layouts.snapshot(item.case.root/'monitor')==original


def test_observation_layout_refuses_every_protected_write_entry(stopped_layout):
    item=stopped_layout
    with pytest.raises(ModError):layouts.Layout(item.journal,item.case.root/'releases'/('a'*64),item.case.root/'releases'/('b'*64),item.case.old,item.case.new)
    observed=layouts.Layout(item.journal,item.case.root/'releases'/('a'*64),item.case.root/'releases'/('b'*64),item.case.old,item.case.new,observation=True)
    observed.reopen(item.case.fingerprint);before=layouts.snapshot(item.case.root/'monitor')
    refused=lambda *a:pytest.fail('Reached effect callback in observation mode')
    with pytest.raises(ModError):observed.prepare()
    with pytest.raises(ModError):observed.exchange('apply',refused,refused)
    with pytest.raises(ModError):observed._move(item.case.root/'monitor',observed.saved,'apply-archive',refused,refused)
    assert layouts.snapshot(item.case.root/'monitor')==before
