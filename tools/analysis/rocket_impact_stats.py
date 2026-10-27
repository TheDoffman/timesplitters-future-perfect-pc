"""Acceptance for the no-bot Temple floor-shot route, not general damage parity.

The tracked lifetime belongs to the type-5 effect observed after the blast.
Observers are asynchronous. Effect counts/ages do not prove rendered opacity,
audio, scanout or input latency. Death saturates health at zero, so this route
can compare lethality but cannot measure equal unsaturated damage.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import json
import math
from pathlib import Path


def analyze(report):
    names=('spawn','aim-floor','impact','settled','respawn','walk')
    checkpoints={}
    for name in names:
        matches=[row for row in report if row.get('command',{}).get('label')==name]
        assert len(matches)==1, f'Missing/duplicate {name} checkpoint'
        checkpoints[name]=matches[0]
    assert all(checkpoints[a]['seconds']<checkpoints[b]['seconds']
               for a,b in zip(names,names[1:])), 'Checkpoints out of order'
    for name,row in checkpoints.items():
        assert (row['scene'],row['match_state'],row['roster'],row['paused'])==(49,7,257,0), name
        assert row.get('player') is not None, f'Unreadable player at {name}'
        actors=row['actors']
        assert actors['pool']==actors['pool_after'] and actors['bot_slots']==0, name
        assert actors['human_slots']==1 and [a['slot'] for a in actors['actors']]==[0], name
        assert row['effects']['complete'], f'Incomplete effects at {name}'
        window=row.get('window',{})
        assert window.get('foreground') and window.get('visible') and not window.get('minimized',True), f'Not foreground gameplay at {name}'

    before=checkpoints['aim-floor'];impact=checkpoints['impact'];settled=checkpoints['settled']
    assert before['player']['look'][1]==-75, 'Floor aim was not clamped'
    assert before['player']['health']>0 and before['player']['equipped'][1]==16
    assert not before['effects']['effects'], 'Unexpected pre-shot effects'
    ammunition=[a for a in before['player']['ammo'] if a['weapon']==16 and a['type']==19]
    assert len(ammunition)==1 and ammunition[0]['loaded']==6 and ammunition[0]['reserve']==6
    assert impact['command']['keys']==[1] and len(impact['key_events'])==1, 'Expected one fire press'
    samples=impact.get('hold_samples',[])+impact.get('wait_samples',[])+settled.get('wait_samples',[])
    assert all(row.get('window',{}).get('foreground') and row['window'].get('visible') and
               not row['window'].get('minimized',True) for row in samples), 'Not foreground effect observations'
    assert len(settled.get('wait_samples',[]))>=80, 'Insufficient effect-cleanup observations'
    missing_player_samples=sum(row.get('player') is None for row in samples)
    totals=[a['loaded']+a['reserve'] for row in samples if row.get('player') is not None for a in row['player']['ammo']
            if a['weapon']==16 and a['type']==19]
    assert totals and min(totals)==11 and max(totals)<=12, 'Did not observe exactly one rocket consumed'
    assert impact['player']['health']==settled['player']['health']==0, 'Blast did not kill the player'
    assert not settled['effects']['effects'], 'Shot effects remained after settling'
    for name in ('respawn','walk'):
        assert checkpoints[name]['player']['health']==before['player']['health'], name
        assert checkpoints[name]['player']['equipped'][1]==16, name
    displacement=math.dist(checkpoints['respawn']['player']['position'],checkpoints['walk']['player']['position'])
    assert displacement>1, 'Walking did not resume'
    wall=settled['seconds']-impact['seconds']
    game=(settled['level_ticks']-impact['level_ticks'])/60
    assert wall>=6 and abs(game-wall)<.1, 'Level clock did not track wall time'

    complete=[row for row in samples if row['effects']['complete']]
    explosion=[(row,e) for row in complete for e in row['effects']['effects'] if e['kind']==5]
    assert len(explosion)>=3, 'Explosion lifetime was not observed'
    assert len({e['address'] for _,e in explosion})==1, 'Multiple/recycled explosions; cannot compare one lifetime'
    assert all(abs(e['lifetime']-.7)<1e-5 for _,e in explosion), 'Unexpected authored explosion duration'
    first,first_effect=explosion[0];last,last_effect=explosion[-1]
    age_delta=last_effect['age']-first_effect['age']
    observed_delta=last['seconds']-first['seconds']
    assert age_delta>.3 and abs(age_delta-observed_delta)<.08, 'Explosion age did not track elapsed time'
    absent=next((row for row in complete if row['seconds']>last['seconds']
                 and not any(e['kind']==5 for e in row['effects']['effects'])),None)
    assert absent, 'Explosion removal was not observed'
    removal_upper=last_effect['age']+absent['seconds']-last['seconds']
    assert last_effect['age']<=.72 and .65<=removal_upper<=.85, 'Explosion removal outside observed lifetime bounds'
    return dict(initial_position=before['player']['position'],initial_health=before['player']['health'],
                rockets_consumed=1,lethal=True,respawn_walk_distance=displacement,
                settle_wall_seconds=wall,settle_game_seconds=game,
                effect_samples=len(samples),incomplete_effect_samples=len(samples)-len(complete),
                missing_player_samples=missing_player_samples,
                explosion_age_error_seconds=age_delta-observed_delta,
                explosion_removal_age_bounds=[last_effect['age'],removal_upper],
                effects_empty_after_settle=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report',type=Path)
    args=parser.parse_args()
    print(json.dumps(analyze(json.loads(args.report.read_text())),indent=2))


if __name__=='__main__':main()
