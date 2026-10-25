"""Reject incomplete/live-test counterexamples, without private game assets."""
import copy
from pathlib import Path
import sys

import pytest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from rocket_impact_stats import analyze


def row(name,seconds,health=1.75,loaded=6,effects=()):
    return dict(command=dict(label=name),seconds=seconds,level_ticks=round(seconds*60),
                scene=49,match_state=7,roster=257,paused=0,
                actors=dict(pool=123,pool_after=123,bot_slots=0,human_slots=1,actors=[dict(slot=0)]),
                player=dict(health=health,look=[335,-75],equipped=[-1,16],position=[0,0,0],
                            ammo=[dict(weapon=16,type=19,loaded=loaded,reserve=6)]),
                effects=dict(complete=True,effects=list(effects)))


def report():
    result=[row('spawn',10),row('aim-floor',11),row('impact',11.5,health=0),
            row('settled',17.6,health=0),row('respawn',23),row('walk',25)]
    result[-1]['player']['position']=[8,0,0]
    result[2]['command']['keys']=[1];result[2]['key_events']=[dict(key=1)]
    result[2]['hold_samples']=[row(None,11.05),row(None,11.1,loaded=5)]
    result[2]['wait_samples']=[row(None,11+age,health=0,loaded=5,effects=[
        dict(kind=5,address=1234,age=age,lifetime=.7)]) for age in (.2,.3,.4)]
    result[3]['wait_samples']=[row(None,11.5+i*.05,health=0,loaded=5,effects=(
        [dict(kind=5,address=1234,age=.5+i*.05,lifetime=.7)] if i<4 else [])) for i in range(122)]
    return result


def test_complete_route_and_bounded_lifetime():
    result=analyze(report())
    assert result['rockets_consumed']==1 and result['lethal']
    assert result['explosion_age_error_seconds']==pytest.approx(0)
    assert result['explosion_removal_age_bounds']==pytest.approx([.65,.7])


@pytest.mark.parametrize('fault', ['missing','duplicate','wrong-scene','bots','pool-race','pause',
    'no-shot','two-shots','survived','no-respawn','no-walk','clock','effects-stuck',
    'incomplete-checkpoint','no-samples','fast-effect','recycled-effect','no-removal',
    'unreadable-checkpoint','unreadable-players'])
def test_counterexamples_fail(fault):
    r=report()
    if fault=='missing':r.pop()
    elif fault=='duplicate':r.append(copy.deepcopy(r[0]))
    elif fault=='wrong-scene':r[1]['scene']=101
    elif fault=='bots':r[1]['actors']['bot_slots']=1
    elif fault=='pool-race':r[2]['actors']['pool_after']=124
    elif fault=='pause':r[3]['paused']=1
    elif fault=='no-shot':
        for s in r[2]['hold_samples']+r[2]['wait_samples']+r[3]['wait_samples']:s['player']['ammo'][0]['loaded']=6
    elif fault=='two-shots':r[2]['key_events'].append(dict(key=1))
    elif fault=='survived':r[2]['player']['health']=1
    elif fault=='no-respawn':r[4]['player']['health']=0
    elif fault=='no-walk':r[5]['player']['position']=[0,0,0]
    elif fault=='clock':r[3]['level_ticks']+=60
    elif fault=='effects-stuck':r[3]['effects']['effects']=[dict(kind=5)]
    elif fault=='incomplete-checkpoint':r[3]['effects']['complete']=False
    elif fault=='no-samples':r[3]['wait_samples']=[]
    elif fault=='fast-effect':
        for s in r[2]['wait_samples']+r[3]['wait_samples']:
            for e in s['effects']['effects']:e['age']*=2
    elif fault=='recycled-effect':r[3]['wait_samples'][0]['effects']['effects'][0]['address']=5678
    elif fault=='no-removal':
        for s in r[3]['wait_samples'][4:]:s['effects']['complete']=False
    elif fault=='unreadable-checkpoint':r[2]['player']=None
    elif fault=='unreadable-players':
        for s in r[2]['hold_samples']+r[2]['wait_samples']+r[3]['wait_samples']:s['player']=None
    with pytest.raises(AssertionError):analyze(r)


def test_incomplete_intermediate_effect_read_is_reported_not_counted():
    r=report();r[3]['wait_samples'][1]['effects']['complete']=False
    assert analyze(r)['incomplete_effect_samples']==1


def test_unreadable_intermediate_player_is_reported_not_counted_as_ammo():
    r=report();r[3]['wait_samples'][20]['player']=None
    assert analyze(r)['missing_player_samples']==1
