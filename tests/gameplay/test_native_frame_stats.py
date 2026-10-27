"""Frame statistics must use elapsed time, not average instantaneous FPS."""


import importlib.util
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location('frame_stats', Path(__file__).resolve().parents[2] / 'tools/analysis/native_frame_stats.py')
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


def clock_capture(tmp_path):
    import csv
    origin=1234567890123456
    rows=[]
    for i,(elapsed,interval) in enumerate(((10,10),(20,10),(80,60),(90,10),(100,10))):
        rows.append(dict(frame=i+2,elapsed_ms=elapsed,frame_ms=interval,swap_ms=1,
                         kind='guest',guest_frame=i+2,clock_ns=origin+elapsed*1000000))
    path=tmp_path/'clock.csv'
    def save():
        with path.open('w',newline='') as out:
            writer=csv.DictWriter(out,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    save()
    return path,origin,rows,save


def test_clock_window_uses_actual_origin_and_preserves_stall(tmp_path):
    path,origin,rows,save=clock_capture(tmp_path)
    result=stats.summarize_clock_window(path,origin+15000000,origin+95000000)
    assert result['samples']==2 and result['worst_ms']==60
    assert result['guest_fps']==pytest.approx(2000/70)
    assert result['first_interval_start_ns']==origin+20000000
    assert result['last_interval_end_ns']==origin+90000000
    assert result['frames_over_50_ms']==1


@pytest.mark.parametrize('failure',['start','end','reverse','missing','gap','backward','duration','nan','no-complete-interval'])
def test_clock_window_rejects_partial_or_inconsistent_evidence(tmp_path,failure):
    path,origin,rows,save=clock_capture(tmp_path)
    start,end=origin+15000000,origin+95000000
    if failure=='start':start=origin
    if failure=='end':end=origin+200000000
    if failure=='reverse':start,end=end,start
    if failure=='missing':
        for row in rows:row.pop('clock_ns')
    if failure=='gap':rows[2]['frame']+=1
    if failure=='backward':rows[2]['clock_ns']=origin
    if failure=='duration':rows[2]['frame_ms']=59
    if failure=='nan':rows[2]['frame_ms']='nan'
    if failure=='no-complete-interval':start,end=origin+30000000,origin+40000000
    save()
    with pytest.raises(ValueError):stats.summarize_clock_window(path,start,end)
