"""Immutable release admission/copy tests; never change accounts or start services."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import shutil
import subprocess
import sys
import tempfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('host_release_test', ROOT / 'tools/mods/hosting/host_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)
spec = importlib.util.spec_from_file_location('host_installer_test', ROOT / 'tools/mods/hosting/install-release-mac.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)
spec = importlib.util.spec_from_file_location('host_preparer_test', ROOT / 'tools/mods/hosting/prepare-release-mac.py')
preparer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preparer)

SOURCE_KEYS = ['tools/mods/mod_api.py', 'tools/mods/mod_runtime_files.py', 'tools/mods/scan_guardian_mac.c',
    'tools/mods/scan_child.py', 'build/scan-guardian/scan-guardian-mac',
    'tools/mods/mod_host_identity.py', 'tools/mods/hosting/locked-service-bootstrap-mac.py',
    'tools/mods/hosting/initialize-private-scan-mac.py', 'tools/mods/hosting/system-scan-monitor-mac.py',
    'tools/mods/mod_system_host.py', 'tools/mods/hosting/system-host-service-mac.py',
    'tools/mods/hosting/system-host-monitor-mac.py', 'tools/mods/hosting/initialize-private-host-mac.py',
    'tools/mods/hosting/prepare-installed-host-mac.py', 'tools/mods/hosting/accept-installed-scanner-mac.py',
    'tools/mods/hosting/register-installed-host-mac.py', 'tools/mods/hosting/accept-private-host-services-mac.py']


@pytest.mark.parametrize('variant',['exact','declared-crlf','undeclared-crlf','mixed','changed','canonical-cr','wrong-path'])
def test_source_freeze_accepts_only_exact_git_or_declared_crlf_rendering(monkeypatch,variant):
    name='tools/mods/hosting/private-recovery-windows.ps1'
    canonical=b'first\nsecond\n'
    current=canonical if variant=='exact' else canonical.replace(b'\n',b'\r\n')
    if variant=='mixed':current=b'first\r\nsecond\n'
    if variant=='changed':current=b'other\r\nsecond\r\n'
    if variant=='canonical-cr':canonical=b'first\r\nsecond\r\n';current=canonical.replace(b'\n',b'\r\n')
    attribute=(name+': eol: crlf\n').encode()
    if variant=='undeclared-crlf':attribute=(name+': eol: unspecified\n').encode()
    if variant=='wrong-path':attribute=b'other.ps1: eol: crlf\n'
    calls=[]
    monkeypatch.setattr(preparer,'git',lambda *args:(calls.append(args),attribute)[1])
    assert preparer.matches_revision(name,current,canonical)==(variant in ('exact','declared-crlf'))
    if calls:assert calls==[('check-attr','--source=HEAD','eol','--',name)]


def record(wire, mode=0o644):
    return dict(bytes=len(wire), mode=mode, sha256=hashlib.sha256(wire).hexdigest())


@pytest.mark.parametrize('value', ['../escape', '/absolute', 'a//b', 'a/./b', 'a\\b', 'C:escape', 'line\nfeed'])
def test_unsafe_component_paths_refused(value):
    with pytest.raises(ValueError):
        release.normalize({value: record(b'x')})


@pytest.mark.parametrize('files', [
    {'A': record(b'x'), 'a': record(b'y')},
    {'parent': record(b'x'), 'parent/child': record(b'y')},
    {'link': dict(symlink='../outside')},
    {'link': dict(symlink='/outside')},
    {'file': dict(bytes=1, mode=0o4755, sha256='0' * 64)},
    {'file': dict(bytes=True, mode=0o644, sha256='0' * 64)},
])
def test_ambiguous_or_escaping_inventory_refused(files):
    with pytest.raises(ValueError):
        release.normalize(files)


def test_metadata_duplicate_and_noninteger_values_refused():
    for wire in [b'{"a":1,"a":2}', b'{"a":NaN}', b'{"a":1.5}']:
        with pytest.raises(ValueError):
            release.decode(wire, 4096)


def test_root_admission_precedes_account_or_bundle_reads(monkeypatch):
    monkeypatch.setattr(installer.sys, 'platform', 'darwin')
    monkeypatch.setattr(installer.os, 'geteuid', lambda: 501, raising=False)
    monkeypatch.setattr(installer.identity, 'existing', lambda: pytest.fail('Read privileged identity before root admission'))
    with pytest.raises(ValueError, match='administrator sudo'):
        installer.install({})


@pytest.fixture
def bundle():
    if os.name != 'posix' or os.getuid() == 0:
        pytest.skip('Unprivileged POSIX input ownership and descriptor-relative I/O')
    with tempfile.TemporaryDirectory(prefix='.tsfp-host-release-', dir=Path.home()) as temporary:
        base = Path(temporary).resolve()
        runtime = base / 'runtime'
        source = base / 'source'
        runtime.mkdir(mode=0o700)
        source.mkdir(mode=0o700)
        runtime_files = {}
        python = 'Frameworks/Python.framework/Versions/3.12/bin/python3.12'
        for key in [python, 'bin/clamscan', 'bin/freshclam', 'bin/caddy', 'Certs/clamav/clamav.crt']:
            wire = ('fixture ' + key).encode()
            path = runtime / key
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(wire)
            mode = 0o644 if key.startswith('Certs/') else 0o755
            path.chmod(mode)
            runtime_files[key] = record(wire, mode)
        runtime_meta = dict(schema=1, producer='tsfp-host-runtime-v1', python=python, files=runtime_files,
                            applications=dict(packages={name: {} for name in ('gunicorn', 'cryptography', 'cffi', 'pycparser')}))
        raw = json.dumps(runtime_meta).encode()
        (runtime / 'runtime.json').write_bytes(raw)
        (runtime / 'runtime.json').chmod(0o600)
        source_files = {}
        for key in SOURCE_KEYS:
            wire = ('fixture ' + key).encode()
            path = source / key
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(wire)
            mode = 0o755 if key.endswith('/scan-guardian-mac') else 0o644
            path.chmod(mode & ~0o200)
            source_files[key] = record(wire, mode)
        guardian = dict(schema=1, producer='tsfp-scan-guardian-v1',
                        source_sha256=source_files['tools/mods/scan_guardian_mac.c']['sha256'],
                        sha256=source_files['build/scan-guardian/scan-guardian-mac']['sha256'])
        wire = json.dumps(guardian).encode()
        key = 'build/scan-guardian/receipt.json'
        (source / key).write_bytes(wire)
        (source / key).chmod(0o444)
        source_files[key] = record(wire)
        data = dict(schema=1, producer='tsfp-host-release-v1', owner_uid=os.getuid(), source_revision='a' * 40,
                    runtime_root=str(runtime), runtime_sha256=hashlib.sha256(raw).hexdigest(),
                    source_root=str(source), source_files=source_files)
        descriptor = base / 'release.json'
        descriptor.write_text(json.dumps(data))
        descriptor.chmod(0o600)
        yield base, descriptor, data


def test_real_input_plan_verifies_all_components_without_writing_root(bundle):
    base, descriptor, _ = bundle
    plan = release.plan(descriptor, hashlib.sha256(descriptor.read_bytes()).hexdigest())
    assert plan['uid'] == os.getuid() and len(plan['components']) == 2
    assert not (base / 'target').exists()


@pytest.mark.parametrize('missing',['tools/mods/mod_host_identity.py','tools/mods/hosting/locked-service-bootstrap-mac.py',
                                    'tools/mods/hosting/initialize-private-scan-mac.py','tools/mods/hosting/system-scan-monitor-mac.py',
                                    'tools/mods/mod_system_host.py','tools/mods/hosting/system-host-service-mac.py',
                                    'tools/mods/hosting/system-host-monitor-mac.py','tools/mods/hosting/initialize-private-host-mac.py',
                                    'tools/mods/hosting/prepare-installed-host-mac.py','tools/mods/hosting/accept-installed-scanner-mac.py',
                                    'tools/mods/hosting/register-installed-host-mac.py','tools/mods/hosting/accept-private-host-services-mac.py'])
def test_incomplete_privilege_boundary_component_refused_before_copy(bundle,missing):
    base,descriptor,data=bundle
    del data['source_files'][missing]
    descriptor.write_text(json.dumps(data))
    with pytest.raises(ValueError,match='Incomplete'):release.plan(descriptor,hashlib.sha256(descriptor.read_bytes()).hexdigest())
    assert not (base/'target').exists()


@pytest.mark.parametrize('change', ['tamper', 'unrecorded', 'symlink', 'hardlink', 'writable-parent'])
def test_changed_component_is_preserved_and_refused(bundle, change):
    base, descriptor, _ = bundle
    path = base / 'runtime/bin/clamscan'
    if change == 'tamper':
        path.write_bytes(b'changed')
    elif change == 'unrecorded':
        (base / 'runtime/unknown').write_bytes(b'preserve')
    elif change == 'symlink':
        path.rename(base / 'original')
        path.symlink_to(base / 'original')
    elif change == 'hardlink':
        os.link(path, base / 'original')
    else:
        path.parent.chmod(0o777)
    with pytest.raises((ValueError, OSError)):
        release.plan(descriptor, hashlib.sha256(descriptor.read_bytes()).hexdigest())
    assert path.exists()


def test_exclusive_copy_cannot_replace_existing_release_files(bundle):
    base, descriptor, _ = bundle
    plan = release.plan(descriptor, hashlib.sha256(descriptor.read_bytes()).hexdigest())
    _, source, records = plan['components'][1]
    target = base / 'target'

    def create(path):
        path.mkdir(mode=0o755)
        path.chmod(0o755)

    release.copy_component(source, target, records, os.getuid(), create)
    release.verify(target, records, os.getuid(), directory_mode=0o755)
    with pytest.raises(FileExistsError):
        release.copy_component(source, target, records, os.getuid(), create)
    assert (target / 'tools/mods/mod_api.py').read_bytes().startswith(b'fixture')


def test_wrong_descriptor_digest_fails_before_component_copy(bundle):
    _, descriptor, _ = bundle
    with pytest.raises(ValueError, match='descriptor digest'):
        release.plan(descriptor, '0' * 64)


@pytest.fixture
def installed_tree(private_posix_path, monkeypatch):
    host = private_posix_path/'installed';host.mkdir(mode=0o755)
    monkeypatch.setattr(release, 'INSTALLED_ROOT', host)
    monkeypatch.setattr(release, 'ROOT_UID', os.geteuid())
    runtime = {}
    python = 'Frameworks/Python.framework/Versions/3.12/bin/python3.12'
    runtime_payloads = {name:('fixture '+name).encode() for name in
                        (python, 'bin/clamscan','bin/freshclam','bin/caddy','Certs/clamav/clamav.crt','lib/engine.dylib')}
    for name,data in runtime_payloads.items():runtime[name]=record(data,0o755 if name==python or name.startswith('bin/') else 0o644)
    runtime['lib/current.dylib']=dict(symlink='engine.dylib')
    metadata=json.dumps(dict(schema=1,producer='tsfp-host-runtime-v1',python=python,files=runtime,
                            applications=dict(packages={key:{} for key in ('gunicorn','cryptography','cffi','pycparser')}))).encode()
    sources={name:('fixture '+name).encode() for name in SOURCE_KEYS}
    sources['config/mods/public-workshop.json']=json.dumps(dict(schema=1,origin='https://workshop.example.test:8443',
        trusted_root_sha256='f'*64,status='active')).encode()
    source_records={name:record(data,0o755 if name.endswith('/scan-guardian-mac') else 0o644) for name,data in sources.items()}
    guardian=json.dumps(dict(schema=1,producer='tsfp-scan-guardian-v1',
        source_sha256=source_records['tools/mods/scan_guardian_mac.c']['sha256'],
        sha256=source_records['build/scan-guardian/scan-guardian-mac']['sha256'])).encode()
    sources['build/scan-guardian/receipt.json']=guardian
    source_records['build/scan-guardian/receipt.json']=record(guardian)
    data=dict(schema=1,producer='tsfp-host-release-v1',owner_uid=501,source_revision='a'*40,
              runtime_root=str(private_posix_path/'unavailable/runtime'),source_root=str(private_posix_path/'unavailable/source'),
              runtime_sha256=hashlib.sha256(metadata).hexdigest(),source_files=source_records)
    descriptor=json.dumps(data).encode();identity=hashlib.sha256(descriptor).hexdigest()
    root=host/'releases'/identity;root.mkdir(parents=True,mode=0o755)
    for prefix,payloads,records in (('source',sources,source_records),('runtime',runtime_payloads,runtime)):
        for name,payload in payloads.items():
            path=root/prefix/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(payload);path.chmod(records[name]['mode'])
    (root/'runtime/lib/current.dylib').symlink_to('engine.dylib')
    (root/'runtime/runtime.json').write_bytes(metadata);(root/'runtime/runtime.json').chmod(0o644)
    common=dict(schema=1,descriptor_sha256=identity,source_revision='a'*40,uid=600,services_started=False)
    for name,value in [('intent.json',dict(common,phase='preparing')),
                       ('installation.json',dict(common,phase='complete',producer='tsfp-installed-host-v1',
                           release=str(root),smoke={},public_endpoint_started=False))]:
        path=root/name;path.write_text(json.dumps(value));path.chmod(0o600)
    (root/'release.json').write_bytes(descriptor);(root/'release.json').chmod(0o600)
    for path in (host,*host.rglob('*')):
        if path.is_dir():path.chmod(0o755)
    return root,identity


def test_installed_verification_never_reads_staging_or_private_data(installed_tree, monkeypatch):
    root,identity=installed_tree
    original=release.read_file
    reads=[]
    def read(parent,relative,*args):
        assert Path(parent).is_relative_to(root)
        reads.append(Path(parent)/relative)
        return original(parent,relative,*args)
    monkeypatch.setattr(release,'read_file',read)
    result=release.installed(identity,600)
    assert result['root']==root and result['python'].is_file() and result['uid']==600
    assert root/'runtime/lib/engine.dylib' in reads
    assert not (root.parent.parent.parent/'unavailable').exists()


@pytest.mark.parametrize('damage', ['unknown','code','runtime','mode','receipt-mode','hardlink','fifo','escaping-link',
                                  'directory-mode','marker-uid','marker-bool','marker-revision','intent','descriptor'])
def test_installed_verification_refuses_tampered_or_incomplete_tree(installed_tree,damage):
    root,identity=installed_tree
    path=root/'source/tools/mods/mod_api.py'
    if damage=='unknown':(root/'unknown').write_bytes(b'preserve')
    elif damage=='code':path.write_bytes(b'changed')
    elif damage=='runtime':(root/'runtime/lib/engine.dylib').write_bytes(b'changed')
    elif damage=='mode':path.chmod(0o666)
    elif damage=='receipt-mode':(root/'installation.json').chmod(0o644)
    elif damage=='hardlink':os.link(path,root.parent/'saved')
    elif damage=='fifo':path.unlink();os.mkfifo(path)
    elif damage=='escaping-link':
        path=root/'runtime/lib/current.dylib';path.unlink();path.symlink_to('/etc/passwd')
    elif damage=='directory-mode':path.parent.chmod(0o777)
    elif damage in ('marker-uid','marker-bool','marker-revision','intent'):
        path=root/('intent.json' if damage=='intent' else 'installation.json');value=json.loads(path.read_bytes())
        value.update({'uid':601} if damage=='marker-uid' else {'schema':True} if damage=='marker-bool'
                     else {'source_revision':'b'*40} if damage=='marker-revision' else {'phase':'complete'})
        path.write_text(json.dumps(value))
    else:(root/'release.json').write_bytes(b'changed')
    with pytest.raises((ValueError,OSError)):release.installed(identity,600)
    assert root.exists()


@pytest.mark.parametrize('identity,uid', [('../escape',600),('a'*64,True),('a'*64,0),('A'*64,600)])
def test_installed_identity_validation_precedes_filesystem_reads(monkeypatch,identity,uid):
    monkeypatch.setattr(release,'read_file',lambda *args:pytest.fail('Read files before rejecting invalid identity'))
    with pytest.raises(ValueError):release.installed(identity,uid)


def upgrade_module():
    spec=importlib.util.spec_from_file_location('upgrade_entry_test',ROOT/'tools/mods/hosting/upgrade-installed-host-mac.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def test_upgrade_entry_defaults_to_plan_without_reading_installed_or_private_state():
    result=subprocess.run([sys.executable,'-B',str(ROOT/'tools/mods/hosting/upgrade-installed-host-mac.py')],
                          capture_output=True,check=True,timeout=10)
    assert json.loads(result.stdout)==dict(schema=1,phase='plan-only',installed_releases=2,jobs=10,
                                           services_changed=False,network_started=False,data_written=False)


def test_upgrade_root_admission_precedes_helper_or_installed_reads(monkeypatch):
    upgrade=upgrade_module()
    monkeypatch.setattr(upgrade.sys,'platform','darwin')
    monkeypatch.setattr(upgrade.os,'geteuid',lambda:501,raising=False)
    monkeypatch.setattr(upgrade,'helper',lambda _:pytest.fail('Loaded privileged helper before root admission'))
    with pytest.raises(ValueError,match='administrator'):upgrade.root_context('a'*64)
    with pytest.raises(ValueError,match='exact'):upgrade.root_context('../escape')


@pytest.mark.parametrize('change', ['same','pending','origin','pin','path','credentials','schema','unknown-field'])
def test_installed_upgrade_inspection_preserves_authority_across_verified_trees(installed_tree,monkeypatch,change):
    root,identity=installed_tree
    candidate=root.parent/'candidate';shutil.copytree(root,candidate,symlinks=True)
    path=candidate/'source/config/mods/public-workshop.json'
    profile=json.loads(path.read_bytes())
    if change=='pending':profile['status']='pending-public-acceptance'
    elif change=='origin':profile['origin']='https://other.example.test:8443'
    elif change=='pin':profile['trusted_root_sha256']='e'*64
    elif change=='path':profile['origin']+='/'
    elif change=='credentials':profile['origin']='https://user:password@workshop.example.test:8443'
    elif change=='schema':profile['schema']=True
    elif change=='unknown-field':profile['other']=True
    path.write_text(json.dumps(profile))
    descriptor=json.loads((candidate/'release.json').read_bytes())
    descriptor['source_revision']='b'*40
    descriptor['source_files']['config/mods/public-workshop.json']=record(path.read_bytes())
    data=json.dumps(descriptor).encode();new_identity=hashlib.sha256(data).hexdigest()
    final=root.parent/new_identity;candidate.rename(final)
    (final/'release.json').write_bytes(data)
    for name in ('intent.json','installation.json'):
        path=final/name;value=json.loads(path.read_bytes())
        value.update(descriptor_sha256=new_identity,source_revision='b'*40)
        if name=='installation.json':value['release']=str(final)
        path.write_text(json.dumps(value))
    upgrade=upgrade_module();monkeypatch.setattr(upgrade,'release',release)
    if change in ('same','pending'):
        report=upgrade.inspection(identity,new_identity,600)
        assert report['installed_sources_verified'] and report['installed_runtimes_verified']
        assert not report['services_changed'] and not report['network_started'] and not report['data_written']
        assert report['source_revisions']==dict(before='a'*40,after='b'*40)
    else:
        with pytest.raises(ValueError):upgrade.inspection(identity,new_identity,600)
