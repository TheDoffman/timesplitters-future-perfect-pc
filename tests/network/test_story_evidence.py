"""Story health acceptance distinguishes death retirement from missing live NPCs."""
import pytest
from story_pair_game import npc_health_agrees

@pytest.mark.parametrize('host,peer,expected',[
    (10.,10.,True),(10.,9.96,True),(10.,9.,False),(10.,None,False),
    (0.,None,True),(None,0.,True),(0.,-1.,True),(None,None,True),
    (0.,1.,False),(None,1.,False),(float('nan'),0.,False),(0.,float('inf'),False),
])
def test_npc_health_agrees(host,peer,expected):
    assert npc_health_agrees(host,peer) is expected
