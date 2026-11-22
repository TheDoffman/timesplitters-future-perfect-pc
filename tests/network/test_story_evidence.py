"""Story health acceptance distinguishes death retirement from missing live NPCs."""
import pytest
from story_pair_game import npc_health_agrees, owned_player_slot

@pytest.mark.parametrize('host,peer,expected',[
    (10.,10.,True),(10.,9.96,True),(10.,9.,False),(10.,None,False),
    (0.,None,True),(None,0.,True),(0.,-1.,True),(None,None,True),
    (0.,1.,False),(None,1.,False),(float('nan'),0.,False),(0.,float('inf'),False),
])
def test_npc_health_agrees(host,peer,expected):
    assert npc_health_agrees(host,peer) is expected


@pytest.mark.parametrize('slot',range(4))
def test_owned_slot_follows_admission_even_when_actor_rows_are_reordered(slot):
    state={'actors':[dict(slot=i,local_port=0 if i==slot else 0xffffffff)
                     for i in (2,0,3,1)]}
    assert owned_player_slot(state)==slot


@pytest.mark.parametrize('ports',[(0xffffffff,0xffffffff),(0,0)])
def test_owned_slot_rejects_missing_or_ambiguous_input_owner(ports):
    with pytest.raises(AssertionError,match='exactly one owned'):
        owned_player_slot({'actors':[dict(slot=i,local_port=port) for i,port in enumerate(ports)]})
