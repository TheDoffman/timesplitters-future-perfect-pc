"""A joined/frozen network scene must never pass a movement comparison."""


import importlib.util
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location('movement', Path(__file__).resolve().parents[2] / 'tools/analysis/network_movement_stats.py')
movement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(movement)


def snapshot(x, scene=3):
    return dict(scene=scene, match_state=7,
                network=dict(state=15, actors=[dict(slot=0, position=[x, 0, 0])]))


def test_moving_peer_endpoint():
    result = movement.compare(snapshot(0), snapshot(5), snapshot(5.001), 0, 3)
    assert result['displacement'] == 5
    assert result['endpoint_error'] == pytest.approx(.001)


def test_matching_frozen_peers_are_rejected():
    with pytest.raises(ValueError, match='stationary endpoints'):
        movement.compare(snapshot(5), snapshot(5), snapshot(5), 0, 3)


@pytest.mark.parametrize('remote', [snapshot(6), snapshot(5, 25), snapshot(float('nan'))])
def test_stale_other_scene_or_invalid_peer_is_rejected(remote):
    with pytest.raises(ValueError):
        movement.compare(snapshot(0), snapshot(5), remote, 0, 3)
