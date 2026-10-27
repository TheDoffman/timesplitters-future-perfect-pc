

import sys
from pathlib import Path
import pytest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from network_pose_stats import measure


def poses(fps=144):
    return [dict(seconds=i/fps, frame=i, scene=48, game_delta=60/fps,
                 entity0=0x80001000, x0=i/fps, y0=0, z0=0) for i in range(fps+1)]


@pytest.mark.parametrize('fps', [60, 120, 144])
def test_fresh_updates_and_game_speed(fps):
    result = measure(poses(fps), 0, 0, 1, 48)
    assert result['continuous_motion'] and result['update_hz'] == fps
    assert result['game_time_ratio'] == pytest.approx(1)


def test_stationary_tail_is_not_a_continuous_motion_pass():
    rows = poses()
    for row in rows[100:]: row['x0'] = rows[99]['x0']
    result = measure(rows, 0, 0, 1, 48)
    assert not result['continuous_motion'] and result['unchanged_updates'] == 45
    assert result['longest_unchanged_ms'] == pytest.approx(312.5)


@pytest.mark.parametrize('change', ['missing', 'duplicate', 'scene', 'actor', 'nan', 'coverage'])
def test_invalid_update_evidence(change):
    rows = poses()
    if change == 'missing': del rows[20]
    if change == 'duplicate': rows[20]['frame'] = 19
    if change == 'scene': rows[20]['scene'] = 25
    if change == 'actor': rows[20]['entity0'] += 4
    if change == 'nan': rows[20]['x0'] = float('nan')
    if change == 'coverage': rows = rows[:50]
    with pytest.raises(ValueError): measure(rows, 0, 0, 1, 48)
