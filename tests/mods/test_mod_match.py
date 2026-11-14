"""Real isolated script sessions behind the game-thread C bridge."""
import json
from pathlib import Path
import subprocess
import pytest
from test_script_process import process_tools

ROOT=Path(__file__).resolve().parents[2]


def run(tools, tmp_path, scenario, source='return {api=1}'):
    driver=tools['worker'].with_name('tsfp-mod-match-driver'+tools['worker'].suffix)
    p=subprocess.run([str(driver),str(tools['worker']),str(tmp_path),scenario],input=source.encode(),capture_output=True,timeout=8)
    assert p.returncode==0,(p.returncode,p.stdout[:2048],p.stderr[:2048])
    assert p.stderr==b'' and not list(tmp_path.iterdir())
    return json.loads(p.stdout)


def test_original_mode_through_game_bridge(process_tools,tmp_path):
    result=run(process_tools,tmp_path,'normal',(ROOT/'examples/mods/first-to-three/scripts/main.lua').read_text())
    assert result['score']==3 and result['winner']==1 and result['commits']==5


@pytest.mark.parametrize('scenario',['policy','initial-world','overflow','stale-epoch','stale-roster','stale-authority','stale-network','invalid-target'])
def test_game_boundary_faults_before_any_partial_commit(process_tools,tmp_path,scenario):
    result=run(process_tools,tmp_path,scenario,'return {api=1,on_start=function() game.set_score(1,99) end}')
    assert any(result.get(name) for name in ('refused','overflow','stale','invalid'))


def test_ticks_coalesce_without_dropping_or_reordering_deaths(process_tools,tmp_path):
    source='''local expect=1
return {api=1,on_tick=function(e)
 if expect==1 then assert(e.tick==100);expect=2 else assert(expect==3 and e.tick==200);expect=4 end
 game.message("tick") end,on_player_death=function(e) assert(expect==2 and e.tick==100);expect=3;game.message("death") end,
 on_stop=function() assert(expect==4) end}'''
    assert run(process_tools,tmp_path,'coalesce',source)['commits']==5


@pytest.mark.parametrize('scenario',['reordered','health'])
def test_player_identity_survives_roster_reordering_and_health_changes(process_tools,tmp_path,scenario):
    result=run(process_tools,tmp_path,scenario,'return {api=1,on_player_death=function() game.set_score(1,9) end}')
    assert result['score']==9 and not result['ended']


@pytest.mark.parametrize('scenario,winner',[('winner',2),('draw',0)])
def test_explicit_winner_or_draw_has_consistent_derived_ranking(process_tools,tmp_path,scenario,winner):
    result=run(process_tools,tmp_path,scenario,f'return {{api=1,on_player_death=function() game.set_score(1,8);game.end_match({winner}) end}}')
    assert result['winner']==winner and result['score']==8


@pytest.mark.parametrize('scenario',['guards','stale','respawn','game'])
def test_real_title_adapter_with_isolated_worker_and_controlled_world(process_tools,tmp_path,scenario):
    driver=process_tools['worker'].with_name('tsfp-mod-guest-driver'+process_tools['worker'].suffix)
    source=(ROOT/'examples/mods/first-to-three/scripts/main.lua').read_bytes()
    p=subprocess.run([str(driver),str(process_tools['worker']),str(tmp_path),scenario],input=source,capture_output=True,timeout=15)
    assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
    assert b'PASS' in p.stdout and p.stderr==b'' and not list(tmp_path.iterdir())
