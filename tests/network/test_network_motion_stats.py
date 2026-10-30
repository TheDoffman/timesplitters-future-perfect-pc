"""Keep actor sampling claims distinct from missing or undersampled movement."""


import sys
from pathlib import Path
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from network_motion_stats import measure


def observations(step=.002):
    return [dict(begin=i*step, end=i*step+.0001, scene=48, network=15, match=7,
                 x0=int(i*step*120)/120, y0=0, z0=0) for i in range(int(1/step))]


def test_dense_motion_observations():
    result = measure(observations(), 0, 0, 1, 48, 120)
    assert result['observed_changes_per_second'] == pytest.approx(120, abs=1)
    assert result['observation_gaps_exceed_frame'] == 0


def test_undersampling_is_explicit():
    result = measure(observations(.02), 0, 0, 1, 48, 120)
    assert result['observation_gaps_exceed_frame'] == 49
    assert result['observed_changes_per_second'] < 60


@pytest.mark.parametrize('failure', ['frozen', 'scene', 'nan', 'timestamps'])
def test_invalid_evidence_rejected(failure):
    rows = observations()
    if failure == 'frozen':
        for row in rows: row['x0'] = 0
    if failure == 'scene': rows[10]['scene'] = 25
    if failure == 'nan': rows[10]['x0'] = float('nan')
    if failure == 'timestamps': rows[10]['end'] = rows[10]['begin'] - .001
    with pytest.raises(ValueError):
        measure(rows, 0, 0, 1, 48, 120)
