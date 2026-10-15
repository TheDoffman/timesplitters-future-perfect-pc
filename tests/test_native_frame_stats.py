"""Frame statistics must use elapsed time, not average instantaneous FPS."""
import importlib.util
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location('frame_stats', Path(__file__).resolve().parents[1] / 'tools/native_frame_stats.py')
stats = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stats)


def test_frame_window_and_stalls(tmp_path):
    path = tmp_path / 'frames.csv'
    path.write_text('frame,elapsed_ms,frame_ms,swap_ms\n'
                    '2,500,500,4\n3,1000,10,2\n4,1030,30,4\n5,1090,60,6\n6,1200,110,8\n')
    result = stats.summarize(path, 1, 1.1)
    assert result['samples'] == 3
    assert result['fps'] == pytest.approx(30)
    assert result['median_ms'] == 30
    assert result['p95_ms'] == result['p99_ms'] == result['worst_ms'] == 60
    assert result['frames_over_50_ms'] == 1
    assert result['frames_over_100_ms'] == 0
    assert result['mean_swap_ms'] == 4
    with pytest.raises(ValueError, match='No frame samples'):
        stats.summarize(path, 2)


def test_guest_frames_are_separate_from_extra_presents(tmp_path):
    path=tmp_path/'frames.csv'
    path.write_text('frame,elapsed_ms,frame_ms,swap_ms,kind,guest_frame\n'
                    '1,8,8,1,guest,1\n2,18,10,1,interpolated,1\n'
                    '3,26,8,1,guest,2\n4,40,14,1,interpolated,2\n')
    result=stats.summarize(path)
    assert result['fps']==100
    assert result['guest_presents']==result['interpolated_presents']==2
    assert result['guest_fps']==result['interpolated_fps']==50
