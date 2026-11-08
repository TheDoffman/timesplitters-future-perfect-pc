"""Every case runs exactly once; shared compiled fixtures stay together."""
import os
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

import pytest
import runtime_shards as shards


@pytest.mark.parametrize('value', ['', '0/3', '1/0', '4/3', '-1/3', '1', '1/2/3', '1.0/3'])
def test_invalid_shards_fail_closed(value):
    with pytest.raises(ValueError):
        shards.parse_shard(value)


def test_assignment_is_stable_and_preserves_fixture_groups():
    assert shards.parse_shard('2/3') == (2, 3)
    one = 'tests/graphics/test_shared.py::test_one[a]'
    two = 'tests/graphics/test_shared.py::test_two[b]'
    assert shards.shard_for(one, 3) == shards.shard_for(two, 3)
    assert shards.shard_for(one.replace('/', '\\'), 3) == shards.shard_for(one, 3)
    assert shards.shard_for(one, 3, ['group']) == shards.shard_for(two, 3, ['group'])
    assert shards.shard_for(one, 3, ['a', 'b']) == shards.shard_for(two, 3, ['b', 'a', 'a'])
    independent = [f'tests/repository/test_patch_updates.py::test_case[{i}]' for i in range(100)]
    assert {shards.shard_for(name, 3) for name in independent} == {1, 2, 3}


@pytest.mark.parametrize('workers', [1, 2])
def test_real_pytest_shards_cover_exactly_the_serial_suite(tmp_path, workers):
    support = Path(__file__).parent
    env = dict(os.environ, PYTHONPATH=str(support))
    (tmp_path / 'conftest.py').write_text(
        "pytest_plugins = ['runtime_shards', 'parallel_groups']\n")
    fixture_source = '''import os
from pathlib import Path
import pytest
@pytest.fixture(scope='module')
def compiled():
    path = Path(os.environ['FIXTURE_LOG'])
    with path.open('a') as stream:
        stream.write(__name__ + '\\n')
@pytest.mark.parametrize('i', range(15))
def test_shared(compiled, i):
    assert i >= 0
'''
    (tmp_path / 'test_shared.py').write_text(fixture_source)
    (tmp_path / 'test_second.py').write_text(
        "import pytest\npytestmark = pytest.mark.xdist_group('explicit')\n" + fixture_source)
    (tmp_path / 'test_third.py').write_text(
        "import pytest\npytestmark = pytest.mark.xdist_group('explicit')\n" + fixture_source)
    (tmp_path / 'test_patch_updates.py').write_text(
        "import pytest\n@pytest.mark.parametrize('i', range(30))\n"
        "def test_independent(i):\n    assert i >= 0\n")

    def execute(tag, shard=None):
        log = tmp_path / (tag + '-fixtures.txt')
        xml = tmp_path / (tag + '.xml')
        command = [sys.executable, '-m', 'pytest', str(tmp_path), '-q', '--tb=short',
                   '--junitxml=' + str(xml)]
        if workers == 2 and shard:
            command += ['-n', '2', '--dist=loadgroup', '--max-worker-restart=0']
        if shard:
            command += ['--runtime-shard=' + shard]
        result = subprocess.run(command, cwd=tmp_path, env=dict(env, FIXTURE_LOG=str(log)),
                                capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        cases = {case.get('classname') + '::' + case.get('name').split('@', 1)[0]
                 for case in ET.parse(xml).getroot().iter('testcase')}
        fixtures = log.read_text().splitlines() if log.exists() else []
        assert len(fixtures) == len(set(fixtures)), 'A module fixture was compiled twice'
        return cases, fixtures

    baseline, _ = execute('baseline')
    seen = set()
    fixtures = []
    for index in range(1, 4):
        cases, compiled = execute(f'shard-{index}', f'{index}/3')
        assert seen.isdisjoint(cases), 'A regression ran in more than one shard'
        seen.update(cases)
        fixtures.extend(compiled)
        explicit = [name for name in cases if name.startswith(('test_second::', 'test_third::'))]
        assert len(explicit) in (0, 30), 'Explicit cross-file group was split'
    assert len(baseline) == 75
    assert seen == baseline, 'Partitioning omitted or added cases'
    assert sorted(fixtures) == ['test_second', 'test_shared', 'test_third']
