"""Scheduling preserves fixtures and every independent regression case."""
from types import SimpleNamespace

import pytest
import parallel_groups as policy


def test_file_fixtures_share_a_worker():
    assert policy.group_for('tests/media/test_audio_timing.py::test_one[a]') == policy.group_for('tests/media/test_audio_timing.py::test_two[b]')
    assert policy.group_for('tests/media/test_audio_timing.py::test_one') != policy.group_for('tests/media/test_native_movie_file.py::test_one')


@pytest.mark.parametrize('path', ['tests/repository/test_patch_updates.py', 'repository/test_patch_updates.py', 'test_patch_updates.py',
                                  'tests/gameplay/test_native_timing.py', 'tests/gameplay/test_native_vehicle_timing.py'])
def test_independent_patch_scenarios_can_run_on_different_workers(path):
    assert policy.group_for(path+'::test_upgrade[false]') != policy.group_for(path+'::test_upgrade[true]')
    assert policy.group_for(path+'::test_upgrade') != policy.group_for(path+'::test_conflict')


def test_serial_runs_and_explicit_groups_are_preserved():
    marks = []
    item = SimpleNamespace(nodeid='tests/example.py::test_one', add_marker=marks.append,
                           get_closest_marker=lambda name: None)
    serial = SimpleNamespace(getoption=lambda name, default=None: {'dist': 'no'}.get(name, default))
    parallel = SimpleNamespace(getoption=lambda name, default=None: {'dist': 'loadgroup'}.get(name, default))
    policy.pytest_collection_modifyitems(serial, [item])
    assert not marks
    policy.pytest_collection_modifyitems(parallel, [item])
    assert len(marks) == 1 and marks[0].name == 'xdist_group'
    item.get_closest_marker = lambda name: object()
    policy.pytest_collection_modifyitems(parallel, [item])
    assert len(marks) == 1


def test_worker_configuration_still_groups_fixtures():
    marks = []
    item = SimpleNamespace(nodeid='tests/example.py::test_one', add_marker=marks.append,
                           get_closest_marker=lambda name: None)
    config = SimpleNamespace(getoption=lambda name, default=None: {'dist': 'no', 'loadgroup': True}.get(name, default))
    policy.pytest_collection_modifyitems(config, [item])
    assert len(marks) == 1
