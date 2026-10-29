"""Keep shared fixtures together; distribute independent migration scenarios."""
import hashlib

import pytest


# These cases compile or migrate in separate tmp_path directories and have no
# shared compiled fixture. Keep the default conservative for all other files.
INDEPENDENT_CASE_FILES = frozenset({
    'test_patch_updates.py', 'test_native_timing.py', 'test_native_vehicle_timing.py',
})


def group_for(nodeid):
    path = nodeid.split('::', 1)[0].replace('\\', '/')
    identity = nodeid if path.rsplit('/', 1)[-1] in INDEPENDENT_CASE_FILES else path
    return 'tsfp-' + hashlib.sha256(identity.encode('utf-8')).hexdigest()[:24]


@pytest.hookimpl(tryfirst=True)
def pytest_collection_modifyitems(config, items):
    # xdist resets dist='no' inside workers and retains the chosen mode here.
    if (config.getoption('dist', default='no') != 'loadgroup'
            and not config.getoption('loadgroup', default=False)):
        return
    for item in items:
        if not item.get_closest_marker('xdist_group'):
            item.add_marker(pytest.mark.xdist_group(group_for(item.nodeid)))
