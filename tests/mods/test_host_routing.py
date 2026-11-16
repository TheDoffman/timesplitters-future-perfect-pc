"""Selective routing state/intent tests; no actual Funnel mutation or root drop."""
import copy
import json
import sys
from pathlib import Path
import subprocess
from types import SimpleNamespace

import pytest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mods'))
import mod_host_routing as routing
from mod_package import ModError

ORIGIN='https://workshop.example.ts.net:8443'
HOST='workshop.example.ts.net'


def state():
    return dict(TCP={'443':dict(HTTPS=True),'8443':dict(HTTPS=True)},
        Web={HOST+':443':dict(Handlers={'/':dict(Proxy='http://127.0.0.1:18789')}),
             HOST+':8443':dict(Handlers={'/':dict(Proxy=routing.TARGET)})},AllowFunnel={HOST+':8443':True})


def wire(value):return json.dumps(value).encode()


def remove(value):
    for key,name in [('TCP','8443'),('Web',HOST+':8443'),('AllowFunnel',HOST+':8443')]:value[key].pop(name,None)


def test_snapshot_hashes_unrelated_private_configuration_and_normalizes_empty_known_maps():
    public=state();baseline=routing.snapshot(wire(public),ORIGIN)
    suspended=copy.deepcopy(public);remove(suspended)
    empty=routing.snapshot(wire(suspended),ORIGIN)
    del suspended['AllowFunnel']
    omitted=routing.snapshot(wire(suspended),ORIGIN)
    suspended['AllowFunnel']=None
    assert empty==omitted==routing.snapshot(wire(suspended),ORIGIN)
    assert empty['phase']=='absent' and baseline['phase']=='public'
    assert empty['unrelated_sha256']==baseline['unrelated_sha256'] and empty['config_sha256']!=baseline['config_sha256']
    private=state();private['AllowFunnel'][HOST+':8443']=False
    assert routing.snapshot(wire(private),ORIGIN)['phase']=='private'
    assert '18789' not in json.dumps(baseline)


@pytest.mark.parametrize('damage', ['false-map','other-authority','other-funnel','forwarder','extra-handler','partial',
    'public443','no443','ambiguous-port','foreground','boolean-integer','flag-integer'])
def test_unknown_or_ambiguous_routes_are_refused_before_actions(damage):
    value=state()
    if damage=='false-map':value['AllowFunnel']=False
    elif damage=='other-authority':value['Web']['other.example.ts.net:8443']=value['Web'][HOST+':8443']
    elif damage=='other-funnel':value['AllowFunnel']['other.example.ts.net:8443']=True
    elif damage=='forwarder':value['TCP']['8443']=dict(TCPForward='localhost:22')
    elif damage=='extra-handler':value['Web'][HOST+':8443']['Handlers']['/private']=dict(Proxy='http://localhost:1234')
    elif damage=='partial':value['TCP'].pop('8443')
    elif damage=='public443':value['AllowFunnel'][HOST+':443']=True
    elif damage=='no443':value['TCP'].pop('443')
    elif damage=='ambiguous-port':value['TCP']['08443']=dict(HTTPS=True)
    elif damage=='foreground':value['Foreground']={'session':{}}
    elif damage=='boolean-integer':value['TCP']['8443']['HTTPS']=1
    else:value['AllowFunnel'][HOST+':8443']=1
    with pytest.raises(ModError):routing.snapshot(wire(value),ORIGIN)


@pytest.mark.parametrize('origin', ['http://workshop.example.ts.net:8443','https://workshop.example.ts.net:443',
    ORIGIN+'/',ORIGIN+'?query',ORIGIN+'#fragment','https://user:password@workshop.example.ts.net:8443'])
def test_origin_never_defaults_to_or_selects_private443(origin):
    with pytest.raises(ModError):routing.snapshot(wire(state()),origin)


@pytest.mark.parametrize('result', ['success','timeout-after-change','error-after-change','no-change','unrelated-change'])
def test_suspend_records_intent_then_observes_actual_outcome_without_retry(result):
    value=state();baseline=routing.snapshot(wire(value),ORIGIN);events=[];calls=[]
    def invoke(action):
        calls.append(action)
        if action=='status':return wire(value)
        assert action=='suspend' and events[0][0]=='routing-intent'
        if result!='no-change':remove(value)
        if result=='unrelated-change':value['Web'][HOST+':443']['Handlers']['/']['Proxy']='private-canary'
        if result=='timeout-after-change':raise subprocess.TimeoutExpired(['fixed'],15)
        if result=='error-after-change':raise ValueError('utility exit nonzero')
        return b''
    if result in ('no-change','unrelated-change'):
        with pytest.raises(ModError) as error:routing.transition(baseline,False,invoke,lambda *args:events.append(args))
        assert 'private-canary' not in str(error.value) and len(events)==1
    else:
        assert routing.transition(baseline,False,invoke,lambda *args:events.append(args))['phase']=='absent'
        assert [kind for kind,_ in events]==['routing-intent','routing-observed']
    assert calls==['status','suspend','status']


def test_restore_requires_saved_authority_and_explicit_resume_for_already_changed_state():
    value=state();public=copy.deepcopy(value);baseline=routing.snapshot(wire(value),ORIGIN);remove(value)
    events=[];calls=[]
    def invoke(action):
        calls.append(action)
        if action=='restore':value.update(copy.deepcopy(public));return b''
        assert action=='status';return wire(value)
    assert routing.transition(baseline,True,invoke,lambda *args:events.append(args))['phase']=='public'
    with pytest.raises(ModError):routing.transition(baseline,True,invoke,lambda *args:events.append(args))
    count=calls.count('restore')
    assert routing.transition(baseline,True,invoke,lambda *args:events.append(args),resume=True)==baseline
    assert calls.count('restore')==count


def test_failed_durable_intent_does_not_change_routing_and_foreign_state_is_preserved():
    value=state();baseline=routing.snapshot(wire(value),ORIGIN);calls=[]
    def invoke(action):calls.append(action);return wire(value)
    def failed(*args):raise OSError('journal unavailable')
    with pytest.raises(OSError):routing.transition(baseline,False,invoke,failed)
    assert calls==['status']
    value['Web'][HOST+':443']['Handlers']['/']['Proxy']='private-canary'
    with pytest.raises(ModError):routing.transition(baseline,False,invoke,failed)
    assert calls==['status','status']


def test_operator_app_always_executes_with_established_nonroot_uid_and_actual_primary_gid(monkeypatch):
    import mod_host_upgrade_process as process
    home=str(Path.home())
    monkeypatch.setattr(routing.sys,'platform','darwin')
    monkeypatch.setattr(routing.os,'geteuid',lambda:0,raising=False)
    monkeypatch.setitem(sys.modules,'pwd',SimpleNamespace(getpwuid=lambda uid:SimpleNamespace(pw_uid=uid,pw_gid=20,pw_dir=home)))
    calls=[]
    def capture(arguments,options,*,timeout,limit):
        calls.append(arguments)
        assert options['user']==501 and options['group']==20 and options['extra_groups']==[]
        assert options['env']==dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=home,LC_ALL='C',TZ='UTC')
        assert timeout==15 and limit==65536
        return b'{}'
    monkeypatch.setattr(process,'_capture',capture)
    before=dict(uid=600,descriptor=dict(owner_uid=501));after=copy.deepcopy(before)
    invoke=routing.operator(before,after)
    for action in ('status','suspend','restore'):invoke(action)
    assert calls==[[str(routing.BINARY),'serve','status','--json'],
                  [str(routing.BINARY),'funnel','--bg','--https=8443','off'],
                  [str(routing.BINARY),'funnel','--bg','--https=8443','--yes',routing.TARGET]]
    with pytest.raises(ModError):invoke('reset')
    after['descriptor']['owner_uid']=502
    with pytest.raises(ModError):routing.operator(before,after)
