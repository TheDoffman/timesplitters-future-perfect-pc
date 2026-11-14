"""Real OS digest providers, private copies and fail-closed helper image pins."""
import hashlib
import json
from pathlib import Path
import os
import subprocess
import sys

import pytest
from test_script_process import process_tools


def call(tools, image, cache, worker, guardian='-', mode='stage'):
    driver=tools['worker'].with_name('tsfp-mod-image-driver'+tools['worker'].suffix)
    result=subprocess.run([str(driver),mode,str(image),str(cache),worker,guardian],capture_output=True,timeout=10)
    assert result.returncode==0,(result.returncode,result.stdout,result.stderr)
    assert result.stderr==b''
    return json.loads(result.stdout)


@pytest.fixture
def images(tmp_path):
    source=tmp_path/'source';source.mkdir()
    image=source/'payload';image.write_bytes(bytes(range(256))*400)
    guardian=source/'tsfp-mod-guardian';guardian.write_bytes(b'bounded guardian fixture')
    cache=tmp_path/'cache';cache.mkdir()
    pins=(hashlib.sha256(image.read_bytes()).hexdigest(),hashlib.sha256(guardian.read_bytes()).hexdigest() if sys.platform=='darwin' else '-')
    return image,guardian,cache,pins


def test_os_hash_matches_streamed_bytes_and_private_copy_cleanup(process_tools,images):
    image,_,cache,pins=images
    assert call(process_tools,image,cache,*pins)==dict(accepted=True,snapshot_unchanged=False,readonly=True)
    assert not list(cache.iterdir())


def test_source_replacement_after_staging_does_not_change_snapshot(process_tools,images):
    image,_,cache,pins=images
    assert call(process_tools,image,cache,*pins,mode='snapshot')['snapshot_unchanged']
    assert hashlib.sha256(image.read_bytes()).hexdigest()!=pins[0] and not list(cache.iterdir())


@pytest.mark.parametrize('kind',['digest','malformed','uppercase','empty','oversize','cancel','missing','directory'])
def test_helper_refusal_leaves_no_created_cache_files(process_tools,images,kind):
    image,_,cache,pins=images;worker,guardian=pins
    if kind=='digest':worker='0'*64
    if kind=='malformed':worker='g'*64
    if kind=='uppercase':worker=worker.upper()
    if kind=='empty':image.write_bytes(b'');worker=hashlib.sha256(b'').hexdigest()
    if kind=='oversize':
        with image.open('wb') as output:output.truncate(32*1024*1024+1)
    if kind=='missing':image.unlink()
    if kind=='directory':image=image.parent
    result=call(process_tools,image,cache,worker,guardian,mode='cancel' if kind=='cancel' else 'stage')
    assert not result['accepted'] and not list(cache.iterdir())


def test_unrecognized_cache_content_is_preserved(process_tools,images):
    image,_,cache,pins=images
    assert call(process_tools,image,cache,*pins,mode='unknown')['accepted']
    directories=list(cache.iterdir());assert len(directories)==1
    assert [p.name for p in directories[0].iterdir()]==['keep.txt']
    assert (directories[0]/'keep.txt').read_text()=='preserve unknown content'


@pytest.mark.parametrize('where',['image','ancestor','cache'])
def test_linked_helper_paths_are_refused(process_tools,images,where,tmp_path):
    image,_,cache,pins=images
    link=tmp_path/'link'
    target=cache if where=='cache' else image.parent if where=='ancestor' else image
    try:link.symlink_to(target,target_is_directory=where!='image')
    except OSError:pytest.skip('Host does not permit this symlink test')
    result=call(process_tools,link/image.name if where=='ancestor' else link if where=='image' else image,
                link if where=='cache' else cache,*pins)
    assert not result['accepted'] and not list(cache.iterdir())


@pytest.mark.skipif(sys.platform!='darwin',reason='Darwin lifetime guardian required')
@pytest.mark.parametrize('kind',['missing','digest','linked'])
def test_guardian_verification_is_required_before_launch(process_tools,images,kind,tmp_path):
    image,guardian,cache,pins=images
    if kind=='missing':guardian.unlink()
    elif kind=='digest':guardian.write_bytes(b'changed')
    else:
        original=tmp_path/'original';guardian.rename(original);guardian.symlink_to(original)
    assert not call(process_tools,image,cache,*pins)['accepted'] and not list(cache.iterdir())


@pytest.mark.skipif(os.name=='nt',reason='POSIX cache ownership policy')
def test_writable_by_other_users_cache_is_refused(process_tools,images):
    image,_,cache,pins=images
    cache.chmod(0o777)
    assert not call(process_tools,image,cache,*pins)['accepted'] and not list(cache.iterdir())
