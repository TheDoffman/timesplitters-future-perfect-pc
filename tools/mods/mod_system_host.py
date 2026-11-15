"""Fixed system-host identities and unprivileged root-health admission adapter."""
import hashlib
import importlib.util
import os
from pathlib import Path

from mod_package import require

ROOT = Path('/Library/Application Support/TSFP Mod Workshop')
ROLES = ('api', 'publication', 'gateway')
POLICY = 'root-system-host-v1'


def paths(role):
    require(role in ROLES, 'Invalid fixed system host role')
    config = ROOT/'data/host'/(role+'.json')
    return dict(config=config, state=config.parent/(role+'-state'),
                monitor=ROOT/'monitor'/role,
                label='org.timesplitters.workshop.'+role+'.'+hashlib.sha256(str(config).encode()).hexdigest()[:24])


def monitor():
    source = Path(__file__).with_name('hosting')/'system-host-monitor-mac.py'
    spec = importlib.util.spec_from_file_location('system_host_monitor', source)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def supervisor(data, kind, binding):
    # The privileged monitor imports only the fixed paths above. Worker health
    # integration loads the ordinary service machinery after credential dropping.
    from mod_service_watch import Supervisor
    return _system_supervisor(Supervisor)(data, kind, binding)


def _system_supervisor(base):
    class SystemSupervisor(base):
        def __init__(self, data, kind, binding):
            expected = paths(kind)
            require(data.get('system_monitor') is True and data.get('label') == expected['label']
                    and data.get('config') == expected['config'] and data.get('state') == expected['state'],
                    'System host configuration binding changed')
            super().__init__(data, kind, binding, POLICY)

        def healthy(self):
            monitor().healthy(self.kind, os.geteuid(), os.getpid(), self.data['system_release'])

        def target(self):
            raise ValueError('Only the protected root monitor controls system host jobs')

        def generate(self, *_):
            raise ValueError('Use the protected system host generator')
    return SystemSupervisor
