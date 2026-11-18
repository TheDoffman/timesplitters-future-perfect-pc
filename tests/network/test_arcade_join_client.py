"""Player-side late-join progress, slot validation and catch-up fencing."""
import struct
from types import SimpleNamespace

import pytest

from online_arcade_client import NativeArcade, PROGRESS, SLOT_PROGRESS, STATE


def native():
    clock=[100.]
    bridge=SimpleNamespace(peer_id=3,clock=lambda:clock[0])
    value=NativeArcade(bridge)
    value.status=dict(epoch=2,host_peer=1,phase='active',joining=True,match_id='aa'*16)
    return value,clock


def test_assigned_actor_slot_is_independent_of_native_peer_id():
    value,_=native()
    value.from_native(SLOT_PROGRESS.pack(b'AMPG',1,1,9,8,7,248,2),('127.0.0.1',44000))
    value.ack=(5,'ab'*32,True)
    body=value.body('cd'*32)
    assert body['native_peer']==9 and body['native_slots']['local_slot']==2
    assert body['join_ready']=='ab'*32 and 'ready' not in body
    assert value.body('cd'*32,recover=True)['epoch']==-1
    value.from_native(PROGRESS.pack(b'AMPG',2,1,9),('127.0.0.1',44000))
    assert 'native_slots' not in value.body('cd'*32)


@pytest.mark.parametrize('fields',[(17,7,248,2),(8,7,252,2),(8,7,248,7),(4,7,248,2)])
def test_invalid_slot_reports_cannot_advance_native_progress(fields):
    value,_=native()
    value.from_native(SLOT_PROGRESS.pack(b'AMPG',1,1,2,*fields),('127.0.0.1',44000))
    assert value.counter==-1 and value.endpoint is None and value.native_slots is None


def test_native_progress_replay_cannot_change_slot_assignment():
    value,_=native()
    value.from_native(SLOT_PROGRESS.pack(b'AMPG',10,1,2,8,7,248,2),('127.0.0.1',44000))
    value.from_native(SLOT_PROGRESS.pack(b'AMPG',9,1,2,8,15,240,3),('127.0.0.1',44000))
    assert value.counter==10 and value.native_slots['local_slot']==2


def test_only_loading_member_gets_catchup_phase_and_migration_takes_precedence():
    value,clock=native();packets=[];value.endpoint=('127.0.0.1',44000)
    value.send=packets.append
    value.poll()
    assert STATE.unpack_from(packets[-1])[3]==5
    value.status['joining']=False;clock[0]+=.2;value.poll()
    assert STATE.unpack_from(packets[-1])[3]==1
    value.status.update(joining=True,phase='preparing');clock[0]+=.2;value.poll()
    assert STATE.unpack_from(packets[-1])[3]==2
