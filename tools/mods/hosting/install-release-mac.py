"""Plan or install an immutable root-owned release; never registers services or public routing."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys


def sibling(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


release = sibling('host_release', 'host_release.py')
identity = sibling('service_identity', 'provision-service-mac.py')
SMOKE = '''import hashlib,json,os,sys,tempfile
from pathlib import Path
root,data,uid=Path(sys.argv[1]),Path(sys.argv[2]),int(sys.argv[3])
assert os.getuid()==os.geteuid()==uid and os.getgid()==os.getegid()==uid
# macOS getgroups reports the user's directory-services access groups even when
# the subprocess kernel supplementary groups were explicitly cleared.
assert 0 not in os.getgroups() and 80 not in os.getgroups()
assert Path(sys.prefix).resolve().is_relative_to(root/'runtime')
sys.path.insert(0,str(root/'source/tools/mods'))
from mod_host_identity import kernel_groups
assert set(kernel_groups()) <= {uid}
import importlib.metadata
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from mod_scan_sandbox import ScanSandbox,verified_guardian
from mod_runtime_files import runtime_access
key=Ed25519PrivateKey.generate();message=b'locked account smoke';key.public_key().verify(key.sign(message),message)
access=runtime_access(root/'runtime/bin/clamscan')
guardian=verified_guardian()
with tempfile.TemporaryDirectory(prefix='release-smoke-',dir=data) as temporary:
 scratch=Path(temporary);content=scratch/'content';database=scratch/'database'
 content.mkdir(mode=0o700);database.mkdir(mode=0o700)
 with ScanSandbox(str(root/'runtime/bin/clamscan'),content,database) as worker:
  result=worker.run(['--database='+str(database),'--version'],capture_output=True,check=True,timeout=10)
  version=result.stdout.decode().strip();assert version.startswith('ClamAV ')
print(json.dumps(dict(uid=uid,gid=os.getgid(),groups=os.getgroups(),kernel_groups=kernel_groups(),prefix=sys.prefix,scanner_version=version,
 packages={n:importlib.metadata.version(n) for n in ('gunicorn','cryptography','cffi','pycparser')},
 guardian_sha256=hashlib.sha256(guardian.read_bytes()).hexdigest(),runtime_images=len(access['images']),
 certificate_verified=True,temporary_state_removed=True,network_started=False,services_started=False)))
'''


def new_directory(path):
    path.mkdir(mode=0o700)
    subprocess.run(['/bin/chmod', '-N', str(path)], check=True, capture_output=True, timeout=10)
    path.chmod(0o755)


def check_installed(target, plan):
    expected = {'release.json', 'intent.json', 'installation.json'}
    for prefix, _, records in plan['components']:
        release.verify(target / prefix, records, 0, directory_mode=0o755)
        expected.update(prefix + '/' + key for key in records)
    release.require(release.inventory(target, 0, 0o755) == expected, 'Installed release has unknown files; preserve it')
    marker = release.decode(release.read_file(target, 'installation.json', 0, 8192), 8192)
    release.require(marker.get('producer') == 'tsfp-installed-host-v1' and marker.get('phase') == 'complete'
                    and marker.get('descriptor_sha256') == plan['identity'], 'Partial/unknown release; preserve it')
    release.require(release.read_file(target, 'release.json', 0, 512 * 1024) == plan['descriptor_wire'],
                    'Installed descriptor changed')
    for name in ('intent.json', 'release.json', 'installation.json'):
        release.require(stat.S_IMODE((target / name).lstat().st_mode) == 0o600, 'Installed private receipt mode changed')
    return marker


def install(plan):
    release.require(sys.platform == 'darwin' and os.geteuid() == 0, 'Installation requires local Mac administrator sudo')
    release.require(int(os.environ.get('SUDO_UID', plan['uid'])) == plan['uid'], 'Descriptor belongs to another operator')
    account = identity.existing()
    target = identity.ROOT / 'releases' / plan['identity']
    identity.unlinked(target)
    if target.exists():
        return check_installed(target, plan)
    release.require(shutil.disk_usage(target.parent).free >= plan['bytes'] + 512 * 1024**2, 'Insufficient release installation space')
    new_directory(target)
    identity.write_new(target / 'intent.json', dict(schema=1, phase='preparing', descriptor_sha256=plan['identity'],
                       source_revision=plan['descriptor']['source_revision'], uid=account['uid'], services_started=False))
    with (target / 'release.json').open('xb') as stream:
        stream.write(plan['descriptor_wire'])
        stream.flush()
        os.fsync(stream.fileno())
    (target / 'release.json').chmod(0o600)
    for prefix, source, records in plan['components']:
        release.copy_component(source, target / prefix, records, plan['uid'], new_directory)
        release.verify(target / prefix, records, 0, directory_mode=0o755)
    python = target / 'runtime' / plan['runtime']['python']
    result = subprocess.run([str(python), '-I', '-B', '-c', SMOKE, str(target), str(identity.ROOT / 'data'), str(account['uid'])],
                            user=account['uid'], group=account['gid'], extra_groups=[], start_new_session=True,
                            stdin=subprocess.DEVNULL, capture_output=True, timeout=45, cwd=target,
                            env=dict(PATH='/usr/bin:/bin', HOME=str(identity.ROOT / 'data'),
                                     TMPDIR=str(identity.ROOT / 'data'), LC_ALL='C', TZ='UTC'))
    release.require(result.returncode == 0 and len(result.stdout) <= 8192 and len(result.stderr) <= 8192,
                    'Locked-account smoke failed (exit ' + str(result.returncode)
                    + '); preserve the staged release: ' + result.stderr.decode('utf-8', 'replace')[-512:])
    smoke = release.decode(result.stdout, 8192)
    release.require(smoke.get('uid') == account['uid'] and smoke.get('gid') == account['gid']
                    and isinstance(smoke.get('groups'), list) and all(type(g) is int and g not in (0, 80) for g in smoke['groups'])
                    and isinstance(smoke.get('kernel_groups'), list)
                    and all(type(g) is int and g == account['uid'] for g in smoke['kernel_groups'])
                    and smoke.get('certificate_verified') is True
                    and smoke.get('temporary_state_removed') is True, 'Unexpected locked-account smoke report')
    # Runtime and code must remain unchanged after the locked process uses them.
    for prefix, _, records in plan['components']:
        release.verify(target / prefix, records, 0, directory_mode=0o755)
    value = dict(schema=1, producer='tsfp-installed-host-v1', phase='complete', descriptor_sha256=plan['identity'],
                 source_revision=plan['descriptor']['source_revision'], uid=account['uid'], release=str(target),
                 smoke=smoke, services_started=False, public_endpoint_started=False)
    identity.write_new(target / 'installation.json', value)
    return check_installed(target, plan)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', required=True, type=Path)
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    try:
        release.require(sys.platform == 'darwin', 'Run host release planning/installation on the Mac')
        if args.apply:
            release.require(sys.platform == 'darwin' and os.geteuid() == 0, 'Installation requires local Mac administrator sudo')
        plan = release.plan(args.bundle, args.sha256)
        value = install(plan) if args.apply else dict(schema=1, phase='plan-only', descriptor_sha256=plan['identity'],
                release=str(identity.ROOT / 'releases' / plan['identity']), bytes=plan['bytes'],
                source_files=len(plan['components'][1][2]), runtime_files=len(plan['components'][0][2]),
                services_started=False, public_endpoint_started=False)
        print(json.dumps(value, indent=2))
        return 0
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('Host release installation: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
