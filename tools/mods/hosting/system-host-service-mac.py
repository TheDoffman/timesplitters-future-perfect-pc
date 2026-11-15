"""Locked-account fixed host workers; configuration and networking stay non-root."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from mod_backup import private_directory, regular
from mod_host_identity import require_locked_process
from mod_package import ModError, read_json, require
from mod_system_host import paths, supervisor, ROLES, ROOT
from mod_trust import origin as checked_origin, trusted_root


def helper(name):
    require(name in ('initialize-private-scan-mac.py','gateway-service-mac.py','live-publication-mac.py','gunicorn.conf.py'),
            'Invalid fixed host helper')
    spec = importlib.util.spec_from_file_location('host_'+name.replace('-', '_').replace('.', '_'), Path(__file__).with_name(name))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def installed_release():
    require(sys.platform == 'darwin', 'Run system host workers on the Mac')
    require_locked_process()
    initializer = helper('initialize-private-scan-mac.py')
    release = initializer.installed_release()
    require(Path(__file__).absolute() == release/'source/tools/mods/hosting/system-host-service-mac.py',
            'Use the immutable installed host worker')
    initializer.check(release)
    return release


def api_config():
    expected = paths('api')
    private_directory(expected['config'].parent)
    regular(expected['config'], 8192)
    data = read_json(expected['config'].read_bytes())
    require(isinstance(data, dict) and set(data) == {'schema', 'origin', 'repository', 'state'}
            and type(data['schema']) is int and data['schema'] == 1, 'Invalid system API configuration')
    checked_origin(data['origin'])
    require(data['repository'] == str(ROOT/'data/scan/repository') and data['state'] == str(expected['state']),
            'System API paths changed')
    data['repository'] = private_directory(data['repository'])
    data['state'] = private_directory(data['state'])
    return dict(data, config=expected['config'], label=expected['label'], uid=os.geteuid())


def config(role, release):
    expected = paths(role)
    api = api_config()
    if role == 'api':
        data = api
    else:
        name = 'gateway-service-mac.py' if role == 'gateway' else 'live-publication-mac.py'
        data = helper(name).config(expected['config'])
        require(data['state'] == expected['state'] and data['caddy'] == str(release/'runtime/bin/caddy'),
                'System public server paths changed')
        if role == 'gateway':
            require(data['origin'] == api['origin'], 'System host origins differ')
        else:
            from mod_signing import load_signer
            require(data['repository'] == api['repository']
                    and data['trusted_root'] == str(ROOT/'data/host/trusted-root.json'), 'System publication paths changed')
            require(all(Path(p).parent == ROOT/'data/host' and re.fullmatch(r'catalog-[1-4]\.pem', Path(p).name)
                        for p in data['catalog_keys'])
                    and all(Path(p).parent == ROOT/'data/host/root-updates' for p in data['root_updates']),
                    'System signing paths changed')
            signer = load_signer(data['trusted_root'], data['catalog_keys'], data['root_updates'])
            require(trusted_root(signer.pinned_root, signer.updates)['origin'] == api['origin'], 'System signing origin differs')
    require(data['label'] == expected['label'], 'System host service identity changed')
    return dict(data, system_monitor=True, system_release=str(release))


def wait_healthy(watch, seconds=30):
    deadline = time.monotonic()+seconds
    while True:
        watch.status(dict(status='waiting'))
        try:
            watch.healthy()
            return
        except (ModError, OSError, ValueError):
            require(time.monotonic() < deadline, 'System host monitor admission timed out')
            time.sleep(.5)


def api_child(data):
    from gunicorn.app.base import BaseApplication
    from mod_api import create_app
    settings = helper('gunicorn.conf.py')
    class Application(BaseApplication):
        def load_config(self):
            for key in vars(settings):
                if key in self.cfg.settings:
                    self.cfg.set(key, getattr(settings, key))
        def load(self):
            return create_app()
    os.environ['TSFP_WORKSHOP_REPOSITORY'] = str(data['repository'])
    os.environ['TSFP_WORKSHOP_ORIGIN'] = data['origin']
    Application().run()


def serve_api(data):
    import fcntl
    import resource
    import stat
    from mod_scan_sandbox import GuardedChild, verified_guardian
    require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, 'API controller must own child exit status')
    descriptor = os.open(data['state']/'api.lock', os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW, 0o600)
    child = None
    read = write = None
    try:
        info = os.fstat(descriptor)
        require(stat.S_ISREG(info.st_mode) and info.st_uid == os.geteuid() and info.st_nlink == 1
                and stat.S_IMODE(info.st_mode) == 0o600, 'Unsafe API controller lock')
        fcntl.flock(descriptor, fcntl.LOCK_EX|fcntl.LOCK_NB)
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_NOFILE, (256, 256))
        resource.setrlimit(resource.RLIMIT_FSIZE, (512*1024**2, 512*1024**2))
        watch = supervisor(data, 'api', 'origin')
        wait_healthy(watch)
        read, write = os.pipe()
        process = subprocess.Popen([str(verified_guardian()), str(read), '--deadline-seconds', '3600',
                                    str(Path(sys.executable).resolve()), '-I', '-B', str(Path(__file__).absolute()),
                                    '--role', 'api', '--api-child'], stdin=subprocess.DEVNULL,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                   close_fds=True, pass_fds=(read,), start_new_session=True, cwd=data['state'],
                                   env=dict(PATH='/usr/bin:/bin', HOME=str(data['state']), TMPDIR=str(data['state']), LC_ALL='C', TZ='UTC'))
        child = GuardedChild(process, write)
        write = None
        os.close(read); read = None
        started, written = time.monotonic(), 0
        while time.monotonic()-started < 3300:
            watch.healthy()
            require(child.poll() is None, 'Private API stopped unexpectedly')
            if time.monotonic()-written >= 5:
                watch.status(dict(status='serving')); written = time.monotonic()
            time.sleep(.5)
    finally:
        if child is not None:
            child.kill(); child.wait(timeout=10)
        for fd in (read, write, descriptor):
            if fd is not None: os.close(fd)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--role', required=True, choices=ROLES)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--run', action='store_true')
    mode.add_argument('--api-child', action='store_true')
    args = parser.parse_args()
    try:
        require(not args.api_child or args.role == 'api', 'Invalid fixed API child role')
        release = installed_release()
        data = config(args.role, release)
        if args.check:
            print(json.dumps(dict(schema=1, role=args.role, release=str(release), phase='ready',
                                  services_started=False, network_started=False)))
        elif args.api_child:
            api_child(data)
        elif args.role == 'api':
            serve_api(data)
        else:
            helper('gateway-service-mac.py' if args.role == 'gateway' else 'live-publication-mac.py').serve(data, managed=True)
        return 0
    except (OSError, ValueError, ModError, subprocess.SubprocessError) as error:
        print('System host worker: '+str(error)[:2000], file=sys.stderr); return 2


if __name__ == '__main__':
    raise SystemExit(main())
