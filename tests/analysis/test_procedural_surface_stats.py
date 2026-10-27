

import sys
from pathlib import Path
import pytest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from procedural_surface_stats import IMAGE_BYTES,image_hash,measure,measure_segments,validate_switch_cycle


def capture():
    data=[bytes([v])*IMAGE_BYTES for v in (10,10,20,20)]
    rows=[dict(sample=i,frame=100+i,scene=4,level_ticks=300+i//2,whole_ticks=i%2,
               delta=.5,native=1,resource=0x80001000,executed=int(i in (0,2)),format=6,
               hash=image_hash(image)) for i,image in enumerate(data)]
    return rows,b''.join(data)


def test_freshness_counts_retained_images():
    rows,pixels=capture();result=measure(rows,pixels)
    assert result['changed_images']==1 and result['unchanged_images']==2
    assert result['executed_changed']==1 and result['skipped_unchanged']==2
    assert result['game_ticks']==1.5


def test_executed_but_frozen_is_not_fresh():
    rows,pixels=capture();rows[1]['executed']=1
    assert measure(rows,pixels)['executed_unchanged']==1


def segmented_capture():
    rows,pixels=capture()
    for i,row in enumerate(rows):
        row.update(segment=i//2,rate=144 if i<2 else 60,native=int(i<2))
        if i>=2:row['frame']+=1000
    return rows,pixels


@pytest.mark.parametrize('rate', [120,144,180,240])
def test_segments_do_not_count_the_rate_switch_as_a_fresh_frame(rate):
    rows,pixels=segmented_capture()
    for row in rows[:2]:row['rate']=rate
    result=measure_segments(rows,pixels)
    assert [r['rate'] for r in result['segments']]==[rate,60]
    assert all(r['comparisons']==1 and r['changed_images']==0 for r in result['segments'])


@pytest.mark.parametrize('mutation',['none','partial','order','segment','original-path'])
@pytest.mark.parametrize('rates', [(240,180,60,120),(144,120,60,144)])
def test_complete_switch_cycle_requires_all_presets_and_original_60(mutation,rates):
    rows=[dict(segment=i//90,rate=rates[i//90],native=int(i//90!=2))
          for i in range(360)]
    if mutation=='partial':rows=rows[:180]
    if mutation=='order':rows[180]['rate']=120
    if mutation=='segment':rows[270]['segment']=1
    if mutation=='original-path':rows[180]['native']=1
    if mutation=='none':validate_switch_cycle(rows,rates)
    else:
        with pytest.raises(ValueError):validate_switch_cycle(rows,rates)


@pytest.mark.parametrize('mutation',['sample','segment','mixed-rate','rate','time','path','gap'])
def test_invalid_segmented_evidence(mutation):
    rows,pixels=segmented_capture()
    if mutation=='sample':rows[2]['sample']=0
    if mutation=='segment':rows[2]['segment']=2
    if mutation=='mixed-rate':rows[1]['rate']=120
    if mutation=='rate':rows[0]['rate']=90
    if mutation=='time':rows[2]['delta']=0
    if mutation=='path':rows[0]['native']=0
    if mutation=='gap':rows[3]['frame']+=1
    with pytest.raises(ValueError):measure_segments(rows,pixels)


def test_current_cycle_does_not_accept_legacy_presets_by_default():
    rows=[dict(segment=i//90,rate=(240,180,60,120)[i//90],native=int(i//90!=2))
          for i in range(360)]
    validate_switch_cycle(rows)
    for i,row in enumerate(rows):row['rate']=(144,120,60,144)[i//90]
    with pytest.raises(ValueError):validate_switch_cycle(rows)


@pytest.mark.parametrize('mutation',['short','hash','gap','resource','nan','scene'])
def test_invalid_evidence(mutation):
    rows,pixels=capture()
    if mutation=='short':pixels=pixels[:-1]
    if mutation=='hash':rows[2]['hash']='0'*16
    if mutation=='gap':rows[2]['frame']+=1
    if mutation=='resource':rows[2]['resource']+=4
    if mutation=='nan':rows[2]['delta']=float('nan')
    if mutation=='scene':rows[2]['scene']=30
    with pytest.raises(ValueError):measure(rows,pixels)


def test_channel_statistics_are_exact_for_flat_and_varying_bytes():
    rows,pixels=capture()
    result=measure(rows,pixels)
    assert result['channel_statistics']==[
        dict(mean=15,std=5,minimum=10,maximum=20,distinct=2)]*4
    pixel=bytes((255,128,128,255))
    frame=pixel*(IMAGE_BYTES//4)
    for row in rows:
        row['hash']=image_hash(frame)
        row['executed']=1
    result=measure(rows,frame*len(rows))
    assert result['changed_images']==0 and result['executed_unchanged']==3
    assert [v['mean'] for v in result['channel_statistics']]==[255,128,128,255]
    assert all(v['std']==0 and v['distinct']==1 for v in result['channel_statistics'])


def test_compressed_capture_preserves_bytes_and_checks_expanded_bound(tmp_path):
    import csv,gzip
    from procedural_surface_stats import load_capture,LIMIT
    rows,pixels=capture();path=tmp_path/'surface.csv'
    with path.open('w',newline='') as out:
        writer=csv.DictWriter(out,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    compressed=Path(str(path)+'.rgba.gz')
    compressed.write_bytes(gzip.compress(pixels))
    loaded,data=load_capture(path)
    assert data==pixels and measure(loaded,data)==measure(rows,pixels)
    compressed.write_bytes(gzip.compress(bytes(LIMIT*IMAGE_BYTES+1)))
    with pytest.raises(ValueError,match='Expanded'):load_capture(path)
