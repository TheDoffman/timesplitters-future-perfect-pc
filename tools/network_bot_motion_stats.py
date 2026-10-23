"""Compare actor update cadence from isolated Online bot observations.

These asynchronous reads do not measure rendered frames or end-to-end latency.
Stationary AI and combat can lower change rates, so report every bot and the
host control alongside the guest rather than treating every hold as a defect.
"""
import argparse
import json
import math
from pathlib import Path
import statistics


def summarize(folder):
    folder=Path(folder)
    rows=json.loads((folder/'observations.json').read_text())
    if not rows or {row['peer'] for row in rows}!={0,1}:
        raise ValueError('Both peers must reach actual bot gameplay')
    result=dict(provenance=json.loads((folder/'provenance.json').read_text()),peers={})
    for peer in (0,1):
        observed=[row for row in rows if row['peer']==peer]
        assert all(row['humans']==2 and row['bots']>0 for row in observed)
        bots=observed[-1]['bots']
        actors={}
        for slot in range(2+bots):
            previous=None;previous_change=None;gaps=[];changes=0;distance=0.;alive=0.;spawns=set();health=set()
            for row in observed:
                actor=row['actors'][slot]
                if actor:
                    spawns.add(actor['spawn']);health.add(round(actor['health'],3))
                continuous=(actor and previous and actor['entity']==previous[1]['entity'] and
                    actor['spawn']==previous[1]['spawn'] and actor['health']>0 and previous[1]['health']>0)
                if continuous:
                    elapsed=row['seconds']-previous[0]
                    alive+=elapsed
                    delta=math.dist(actor['position'],previous[1]['position'])
                    if delta>1e-6:
                        changes+=1;distance+=delta
                        if previous_change is not None:gaps.append(row['seconds']-previous_change)
                        previous_change=row['seconds']
                else:
                    previous_change=None
                previous=(row['seconds'],actor) if actor else None
            actors[slot]=dict(kind='human' if slot<2 else 'bot',alive_seconds=alive,
                observed_changes=changes,changes_per_alive_second=changes/alive if alive else 0,
                changed_pose_gap_p50_ms=1000*statistics.median(gaps) if gaps else None,
                distance=distance,spawns=len(spawns),health_values=len(health))
        observation_gaps=[b['seconds']-a['seconds'] for a,b in zip(observed,observed[1:])]
        result['peers']['host' if peer==0 else 'guest']=dict(samples=len(observed),actors=actors,
            observation_gap_p50_ms=1000*statistics.median(observation_gaps),
            observation_gap_max_ms=1000*max(observation_gaps))
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',type=Path)
    parser.add_argument('--baseline',type=Path,help='Require a discriminating baseline/fixed cadence comparison')
    parser.add_argument('--output',type=Path,help='Write UTF-8 JSON without shell redirection')
    args=parser.parse_args()
    result=summarize(args.folder)
    if args.baseline:
        before=summarize(args.baseline)
        assert before['provenance'].get('completed') and result['provenance'].get('completed'), 'A runtime run did not complete'
        assert before['provenance']['executable_sha256']!=result['provenance']['executable_sha256'], 'Same executable'
        for name,run in (('baseline',before),('fixed',result)):
            for peer in ('host','guest'):
                actors=run['peers'][peer]['actors']
                assert all(actors[slot]['observed_changes']>20 for slot in (0,1)), (name,peer,'human movement missing')
                assert all(a['spawns']>=2 and a['health_values']>=2 for a in actors.values() if a['kind']=='bot'), (name,peer,'lifecycle missing')
        slots=[slot for slot,a in result['peers']['guest']['actors'].items() if a['kind']=='bot']
        assert len(slots)==6, 'Expected six configured bots'
        for slot in slots:
            old=before['peers']['guest']['actors'][slot]['changed_pose_gap_p50_ms']
            new=result['peers']['guest']['actors'][slot]['changed_pose_gap_p50_ms']
            control=result['peers']['host']['actors'][slot]['changed_pose_gap_p50_ms']
            assert old>40 and new<25 and control<25, (slot,old,new,control)
        result['comparison']='PASS: all six guest bots change at frame cadence, baseline holds reproduced; host/human motion and bot lifecycles observed'
    encoded=json.dumps(result,indent=2)+'\n'
    if args.output:
        args.output.write_text(encoded,encoding='utf-8')
    else:
        print(encoded,end='')
