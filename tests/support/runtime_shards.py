"""Partition complete suites without splitting their existing fixture groups."""
import hashlib
import re

import pytest

from parallel_groups import group_for


def parse_shard(value):
    if not re.fullmatch(r'[1-9][0-9]*/[1-9][0-9]*', value):
        raise ValueError('Shard must be INDEX/TOTAL (for example 1/3).')
    index, total = map(int, value.split('/'))
    if index > total:
        raise ValueError('Shard index must not exceed its total.')
    return index, total


def shard_for(nodeid, total, groups=()):
    # Match xdist's combined explicit group name, or the existing automatic
    # file/case policy. Both stay independent of collection order and host OS.
    nodeid = re.sub(r'@tsfp-[0-9a-f]{24}$', '', nodeid).replace('\\', '/')
    identity = '_'.join(sorted(set(groups))) if groups else group_for(nodeid)
    return int.from_bytes(hashlib.sha256(identity.encode()).digest()[:8], 'big') % total + 1


def pytest_addoption(parser):
    parser.addoption('--runtime-shard', help='Run one complete fixture-preserving shard, INDEX/TOTAL.')


def pytest_configure(config):
    value = config.getoption('runtime_shard')
    if value:
        try:
            parse_shard(value)
        except ValueError as error:
            raise pytest.UsageError(str(error)) from error


@pytest.hookimpl(wrapper=True, tryfirst=True)
def pytest_collection_modifyitems(config, items):
    # Filter before xdist adds group suffixes. Respect explicit groups before
    # the automatic policy adds its markers. Normal pytest -k/-m/--lf filters
    # still run and are reported as partial runs by the timing monitor.
    value = config.getoption('runtime_shard')
    if value:
        index, total = parse_shard(value)
        items[:] = [item for item in items if shard_for(
            item.nodeid, total,
            [str(mark.kwargs.get('name', mark.args[0] if mark.args else 'default'))
             for mark in item.iter_markers('xdist_group')]) == index]
        # This is the declared collection for this profile, rather than an
        # ad-hoc deselection. Each shard has its own timing identity/baseline.
    return (yield)


def pytest_report_header(config):
    value = config.getoption('runtime_shard')
    if value:
        return f'Runtime shard {value} (fixture groups preserved)'
