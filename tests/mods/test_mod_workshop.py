"""Asset-free adversarial tests for the local mod workshop foundation."""
import importlib.util
import json
from pathlib import Path
import stat
import subprocess
import sys
import threading
import time
from urllib.request import Request, urlopen
from urllib.error import HTTPError
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/mods'))
import mod_package as packages
import mod_repository
from mod_package import ModError, build_package, digest, inspect_package
from mod_repository import ClamScanner, Repository, SCAN_TTL, stage_install
from mod_server import fetch_install, make_server
from mod_publication import export_snapshot, validate_snapshot


class Scanner:
    """Synthetic scanner only: never exposed by the operator CLI."""
    def __init__(self, status='clean'):
        self.status = status

    def scan(self, directory):
        assert (Path(directory) / 'mod.json').is_file()
        return {'engine': 'TEST ONLY', 'version': 'fixture', 'time': int(time.time()),
                'status': self.status, 'detail': 'Synthetic test result',
                'database_max_age_days': 2, 'isolation': 'macos-seatbelt-v2'}


@pytest.fixture
def scanner_process_stub(tmp_path, monkeypatch):
    """Unit-only process seam; production has no unsandboxed scanner option."""
    class Worker:
        scratch = tmp_path
        def __init__(self, executable, content, database):
            self.executable = executable
        def __enter__(self):
            return self
        def __exit__(self, *args):
            pass
        def run(self, arguments, **kwargs):
            return subprocess.run([self.executable, *arguments], **kwargs)
        def scan(self, arguments, **kwargs):
            return self.run(arguments, capture_output=True, **kwargs)
    monkeypatch.setattr(mod_repository, 'ScanSandbox', Worker)


def create_package(tmp_path, mod_id='test-map', version='1.0.0', dependencies=None):
    source = tmp_path / (mod_id + version)
    source.mkdir()
    m = {'schema': 1, 'id': mod_id, 'version': version, 'name': 'Community Test',
         'author': 'A Creator', 'description': 'A harmless fixture',
         'game': 'tsfp-us-45410066', 'api': 'tsfp-mod-v1-draft',
         'dependencies': dependencies or {}, 'files': {}}
    (source / 'mod.json').write_text(json.dumps(m))
    (source / 'scripts').mkdir()
    (source / 'scripts/main.lua').write_text('return {example = true}\n')
    output = tmp_path / (mod_id + version + '.zip')
    build_package(source, output)
    return output


def rewrite(package, entries=None, change=None):
    with zipfile.ZipFile(package) as archive:
        content = {n: archive.read(n) for n in archive.namelist()}
    if change:
        manifest = json.loads(content['mod.json'])
        change(manifest)
        content['mod.json'] = json.dumps(manifest).encode()
    if entries:
        content.update(entries)
    with zipfile.ZipFile(package, 'w') as archive:
        for name, data in content.items():
            archive.writestr(name, data)


def approved(tmp_path):
    package = create_package(tmp_path)
    repo = Repository(tmp_path / 'repository')
    result = repo.submit(package, Scanner())
    repo.approve(result['sha256'], 'Fixture reviewer: inspected content')
    return repo, package, result['sha256']


def test_deterministic_creator_package_and_inactive_install(tmp_path):
    package = create_package(tmp_path)
    second = tmp_path / 'second.zip'
    build_package(tmp_path / 'test-map1.0.0', second)
    assert package.read_bytes() == second.read_bytes()
    sha = digest(package.read_bytes())
    mods = tmp_path / 'Mods'
    receipt = stage_install(package, sha, mods)
    assert receipt['state'] == 'staged-inactive'
    assert (mods / 'test-map@1.0.0/content/scripts/main.lua').read_text().startswith('return')
    assert not list(mods.glob('.install-*'))
    with pytest.raises(ModError, match='already installed'):
        stage_install(package, sha, mods)


@pytest.mark.parametrize('name', ['../outside.lua', '/root.lua', 'C:/evil.lua',
    'scripts/a:evil.lua', 'scripts/a\\evil.lua', 'scripts/CON.lua',
    'scripts/name.lua.', 'scripts/../evil.lua', 'scripts//x.lua',
    'scripts/evil.exe', 'scripts/.hidden.lua', 'scripts/lpt1.lua'])
def test_hostile_archive_paths_rejected(tmp_path, name):
    package = create_package(tmp_path)
    rewrite(package, {name: b'return true'})
    target = tmp_path / 'extract'
    target.mkdir()
    with pytest.raises(ModError):
        inspect_package(package, target)
    assert not list(target.rglob('*'))


def test_duplicate_case_and_symlink_entries_rejected(tmp_path):
    package = create_package(tmp_path)
    with zipfile.ZipFile(package, 'a') as archive:
        archive.writestr('scripts/MAIN.lua', b'return true')
    with pytest.raises(ModError, match='case-colliding'):
        inspect_package(package)
    other = create_package(tmp_path, 'link-test')
    with zipfile.ZipFile(other, 'a') as archive:
        info = zipfile.ZipInfo('scripts/link.lua')
        info.external_attr = (stat.S_IFLNK | 0o777) << 16
        archive.writestr(info, b'../../outside')
    with pytest.raises(ModError, match='regular files'):
        inspect_package(other)


@pytest.mark.parametrize('mutation', ['hash', 'extra', 'size', 'unknown', 'dependency', 'game', 'api'])
def test_manifest_and_payload_tampering_rejected(tmp_path, mutation):
    package = create_package(tmp_path)
    if mutation == 'hash':
        rewrite(package, {'scripts/main.lua': b'changed'})
    elif mutation == 'extra':
        rewrite(package, {'scripts/extra.lua': b'return true'})
    else:
        def change(m):
            if mutation == 'size':
                m['files']['scripts/main.lua']['size'] += 1
            elif mutation == 'unknown':
                m['install_command'] = 'evil'
            elif mutation == 'dependency':
                m['dependencies'] = {'../escape': '1.0.0'}
            else:
                m[mutation] = 'unsupported'
        rewrite(package, change=change)
    with pytest.raises(ModError):
        inspect_package(package)


def test_duplicate_json_keys_rejected(tmp_path):
    package = create_package(tmp_path)
    rewrite(package, {'mod.json': b'{"schema":1,"schema":1}'})
    with pytest.raises(ModError, match='Duplicate JSON'):
        inspect_package(package)


@pytest.mark.parametrize('manifest', [b'\xef\xbb\xbf{}', '{}'.encode('utf-16')])
def test_manifest_requires_unambiguous_utf8_without_bom(tmp_path, manifest):
    package = create_package(tmp_path)
    rewrite(package, {'mod.json': manifest})
    with pytest.raises(ModError):
        inspect_package(package)


@pytest.mark.parametrize('name,data', [
    ('scripts/main.lua', b'\x1bLua\x00binary'),
    ('scripts/main.lua', b'\xff\xfe'),
    ('scripts/main.lua', b'MZdisguised executable'),
    ('textures/fake.dds', b'not a DDS'),
    ('audio/fake.wav', b'not a WAV'),
    ('audio/fake.ogg', b'not an Ogg'),
    ('maps/fake.json', b'[]'),
])
def test_disguised_binary_and_wrong_payload_types(tmp_path, name, data):
    package = create_package(tmp_path)
    def change(m):
        m['files'][name] = {'sha256': digest(data), 'size': len(data)}
    rewrite(package, {name: data}, change)
    with pytest.raises(ModError):
        inspect_package(package)


def test_file_directory_collision_rejected_before_extraction(tmp_path):
    package = create_package(tmp_path)
    data = b'return true'
    name = 'scripts/main.lua/child.lua'
    def change(m):
        m['files'][name] = {'sha256': digest(data), 'size': len(data)}
    rewrite(package, {name: data}, change)
    target = tmp_path / 'extracted'
    target.mkdir()
    with pytest.raises(ModError, match='collision'):
        inspect_package(package, target)
    assert list(target.iterdir()) == []


def test_expansion_and_entry_limits_before_extract(tmp_path, monkeypatch):
    package = create_package(tmp_path)
    monkeypatch.setattr(packages, 'MAX_FILE', 10)
    with pytest.raises(ModError, match='limit'):
        inspect_package(package)
    monkeypatch.setattr(packages, 'MAX_FILE', 64 * 1024 * 1024)
    monkeypatch.setattr(packages, 'MAX_TOTAL', 10)
    with pytest.raises(ModError, match='total limit'):
        inspect_package(package)
    monkeypatch.setattr(packages, 'MAX_FILES', 0)
    with pytest.raises(ModError, match='entry count'):
        inspect_package(package)


@pytest.mark.parametrize('status,state', [('clean', 'pending-review'),
                                         ('blocked', 'rejected'), ('error', 'scan-error')])
def test_scanning_and_manual_review_gate(tmp_path, status, state):
    package = create_package(tmp_path)
    repo = Repository(tmp_path / 'repository')
    result = repo.submit(package, Scanner(status))
    assert result['state'] == state
    assert repo.catalog() == []
    with pytest.raises(ModError):
        repo.approved_blob(result['sha256'])
    if status != 'clean':
        with pytest.raises(ModError):
            repo.approve(result['sha256'], 'Operator reviewed')
    else:
        repo.approve(result['sha256'], 'Operator reviewed')
        assert repo.catalog('creator')[0]['sha256'] == result['sha256']
        assert repo.catalog('no-match') == []


def test_immutable_versions_and_revocation(tmp_path):
    repo, package, sha = approved(tmp_path)
    with pytest.raises(ModError, match='already exists'):
        repo.submit(package, Scanner())
    repo.revoke(sha, 'Operator: report received')
    assert repo.catalog() == []
    with pytest.raises(ModError):
        repo.approved_blob(sha)
    with pytest.raises(ModError):
        repo.approve(sha, 'Attempted revival')
    with pytest.raises(ModError):
        repo.rescan(sha, Scanner())
    with repo.db() as db:
        assert [r['action'] for r in db.execute('SELECT action FROM events ORDER BY sequence')] == [
            'submitted', 'approved', 'revoked']


def test_expired_scan_unavailable_and_rescan_requires_review(tmp_path, monkeypatch):
    repo, _, sha = approved(tmp_path)
    now = time.time()
    monkeypatch.setattr(time, 'time', lambda: now + SCAN_TTL + 1)
    assert repo.catalog() == []
    with pytest.raises(ModError):
        repo.approved_blob(sha)
    repo.rescan(sha, Scanner())
    assert repo.catalog() == []
    repo.approve(sha, 'Reviewed after rescan')
    assert len(repo.catalog()) == 1


def test_repository_and_download_tampering_rejected(tmp_path):
    repo, package, sha = approved(tmp_path)
    repo.blob(sha).write_bytes(b'changed bytes')
    with pytest.raises(ModError, match='integrity'):
        repo.approved_blob(sha)
    mods = tmp_path / 'Mods'
    with pytest.raises(ModError, match='digest'):
        stage_install(package, '0' * 64, mods)
    assert list(mods.iterdir()) == []


def test_exact_dependencies_and_side_by_side_versions(tmp_path):
    addon = create_package(tmp_path, 'addon', dependencies={'base': '1.0.0'})
    mods = tmp_path / 'Mods'
    with pytest.raises(ModError, match='dependency first'):
        stage_install(addon, digest(addon.read_bytes()), mods)
    for version in ('1.0.0', '2.0.0'):
        base = create_package(tmp_path, 'base', version)
        stage_install(base, digest(base.read_bytes()), mods)
    stage_install(addon, digest(addon.read_bytes()), mods)
    assert len(list(mods.iterdir())) == 3


def test_scanner_absence_fails_closed(tmp_path):
    report = ClamScanner(str(tmp_path / 'not-installed')).scan(tmp_path)
    assert report['status'] == 'error'


@pytest.mark.parametrize('code,status', [(0, 'clean'), (1, 'blocked'), (2, 'error'), (99, 'error')])
def test_clamav_exit_codes_and_limit_flags(tmp_path, monkeypatch, scanner_process_stub, code, status):
    def run(command, **kwargs):
        assert kwargs.get('shell', False) is False
        if '--version' in command:
            return subprocess.CompletedProcess(command, 0, stdout=b'ClamAV test/database/date')
        assert '--alert-exceeds-max=yes' in command
        assert '--max-filesize=65M' in command
        assert '--fail-if-cvd-older-than=2' in command
        assert '--official-db-only=yes' in command
        assert '--follow-dir-symlinks=0' in command
        assert '--follow-file-symlinks=0' in command
        assert kwargs['timeout'] == 180
        return subprocess.CompletedProcess(command, code, stdout=b'')
    monkeypatch.setattr(subprocess, 'run', run)
    assert ClamScanner().scan(tmp_path)['status'] == status


def test_scanner_explicit_database_is_used_for_version_and_scan(tmp_path, monkeypatch, scanner_process_stub):
    commands = []
    def run(command, **kwargs):
        commands.append(command)
        assert '--database=' + str(tmp_path.resolve()) in command
        return subprocess.CompletedProcess(command, 0,
            stdout=b'ClamAV test/database/date' if '--version' in command else b'')
    monkeypatch.setattr(subprocess, 'run', run)
    report = ClamScanner(database=tmp_path).scan(tmp_path)
    assert report['status'] == 'clean'
    assert report['database_max_age_days'] == 2
    assert report['isolation'] == 'macos-seatbelt-v2'
    assert len(commands) == 2


def test_engine_warning_with_clean_exit_is_withheld(tmp_path, monkeypatch, scanner_process_stub):
    def run(command, **kwargs):
        data = b'ClamAV test/database/date' if '--version' in command else b'WARNING: incomplete scan\n'
        return subprocess.CompletedProcess(command, 0, stdout=data)
    monkeypatch.setattr(subprocess, 'run', run)
    report = ClamScanner(database=tmp_path).scan(tmp_path)
    assert report['status'] == 'error' and 'incomplete scan' in report['detail']


def test_scanner_missing_explicit_database_fails_before_execution(tmp_path, monkeypatch):
    monkeypatch.setattr(subprocess, 'run', lambda *a, **kw: pytest.fail('Must not execute'))
    assert ClamScanner(database=tmp_path / 'missing').scan(tmp_path)['status'] == 'error'


@pytest.mark.parametrize('policy', ['database_max_age_days', 'isolation'])
def test_legacy_clean_scan_requires_rescan_before_approval_or_publication(tmp_path, policy):
    repo, _, sha = approved(tmp_path)
    with repo.db() as db:
        row = db.execute('SELECT scan FROM releases WHERE sha256=?', (sha,)).fetchone()
        scan = json.loads(row['scan'])
        del scan[policy]
        db.execute('UPDATE releases SET scan=? WHERE sha256=?', (json.dumps(scan), sha))
    assert repo.catalog() == []
    with pytest.raises(ModError):
        repo.approved_blob(sha)
    repo.rescan(sha, Scanner())
    repo.approve(sha, 'Fixture reviewer: rescanned under enforced database policy')
    assert len(repo.catalog()) == 1


def test_scanner_timeout_blocks(tmp_path, monkeypatch, scanner_process_stub):
    def run(command, **kwargs):
        raise subprocess.TimeoutExpired(command, 180)
    monkeypatch.setattr(subprocess, 'run', run)
    assert ClamScanner().scan(tmp_path)['status'] == 'error'


def test_unsupported_scanner_host_never_runs_unsandboxed(tmp_path, monkeypatch):
    import mod_scan_sandbox
    monkeypatch.setattr(mod_scan_sandbox.sys, 'platform', 'unsupported-test-host')
    monkeypatch.setattr(subprocess, 'run', lambda *a, **kw: pytest.fail('Unsafe fallback'))
    report = ClamScanner(database=tmp_path).scan(tmp_path)
    assert report['status'] == 'error'
    assert 'isolation' not in report


def test_loopback_search_download_install_and_revoke(tmp_path):
    repo, _, sha = approved(tmp_path)
    server = make_server(repo, 0)
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    url = f'http://127.0.0.1:{server.server_port}'
    try:
        with urlopen(url + '/v1/catalog?q=community', timeout=5) as response:
            assert json.load(response)['releases'][0]['sha256'] == sha
        with urlopen(url, timeout=5) as response:
            assert b'Mod Workshop' in response.read()
            assert response.headers['X-Content-Type-Options'] == 'nosniff'
        receipt = fetch_install(url, sha, tmp_path / 'Mods')
        assert receipt['state'] == 'staged-inactive'
        repo.revoke(sha, 'Fixture report')
        with pytest.raises(ModError, match='unavailable'):
            fetch_install(url, sha, tmp_path / 'OtherMods')
        request = Request(url, headers={'Host': 'untrusted.example'})
        with pytest.raises(HTTPError) as error:
            urlopen(request, timeout=5)
        assert error.value.code == 403
    finally:
        server.shutdown()
        worker.join(timeout=5)
        server.server_close()


@pytest.mark.parametrize('url', ['https://example.com', 'http://localhost:8765',
    'http://127.0.0.1:8765/private', 'http://user@127.0.0.1:8765', 'file:///etc/passwd'])
def test_preview_client_rejects_untrusted_repository_urls(tmp_path, url):
    with pytest.raises(ModError):
        fetch_install(url, '0' * 64, tmp_path / 'Mods')


def test_cli_workflow_and_no_scan_bypass(tmp_path):
    spec = importlib.util.spec_from_file_location('mod_workshop_cli', ROOT / 'tools/mods/mod-workshop.py')
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    package = create_package(tmp_path)
    assert cli.main(['validate', str(package)]) == 0
    assert cli.main(['submit', str(package), '--repository', str(tmp_path / 'repo'),
                     '--clamscan', str(tmp_path / 'missing')]) == 2
    assert cli.main(['search', '--repository', str(tmp_path / 'repo')]) == 0
    with pytest.raises(SystemExit):
        cli.main(['submit', str(package), '--repository', str(tmp_path / 'repo'), '--skip-scan'])


def test_public_snapshot_contains_only_reviewed_bytes(tmp_path):
    repo, package, sha = approved(tmp_path)
    for mod_id, status in [('held', 'clean'), ('detected', 'blocked'), ('scan-failed', 'error')]:
        repo.submit(create_package(tmp_path, mod_id), Scanner(status))
    public = tmp_path / 'public'
    report = export_snapshot(repo, public)
    catalog = validate_snapshot(public)
    assert report['releases'] == 1
    assert catalog['releases'][0]['sha256'] == sha
    assert (public / 'v1/packages' / sha).read_bytes() == package.read_bytes()
    assert {p.relative_to(public).as_posix() for p in public.rglob('*') if p.is_file()} == {
        'v1/catalog', 'v1/packages/' + sha}
    encoded = (public / 'v1/catalog').read_text()
    assert str(repo.root) not in encoded and 'detail' not in encoded and 'note' not in encoded


def test_publication_preserves_existing_snapshots_and_separates_private_storage(tmp_path):
    repo, _, _ = approved(tmp_path)
    public = tmp_path / 'public'
    export_snapshot(repo, public)
    original = (public / 'v1/catalog').read_bytes()
    with pytest.raises(ModError, match='preserved'):
        export_snapshot(repo, public)
    assert (public / 'v1/catalog').read_bytes() == original
    with pytest.raises(ModError, match='outside'):
        export_snapshot(repo, repo.root / 'accidental-public')


def test_publication_revocation_requires_new_snapshot(tmp_path):
    repo, _, sha = approved(tmp_path)
    old = tmp_path / 'old'
    export_snapshot(repo, old)
    repo.revoke(sha, 'Removed by reviewer')
    fresh = tmp_path / 'fresh'
    export_snapshot(repo, fresh)
    assert validate_snapshot(fresh)['releases'] == []
    assert not (fresh / 'v1/packages' / sha).exists()
    # An exported tree is a snapshot: operator must stop/switch the old root.
    assert (old / 'v1/packages' / sha).exists()


@pytest.mark.parametrize('mutation', ['catalog-expiry', 'extra-private-file', 'corrupt-package', 'missing-package'])
def test_public_preflight_rejects_expiry_and_changed_tree(tmp_path, mutation):
    repo, _, sha = approved(tmp_path)
    public = tmp_path / 'public'
    export_snapshot(repo, public)
    if mutation == 'catalog-expiry':
        path = public / 'v1/catalog'
        data = json.loads(path.read_text())
        data['expires'] = int(time.time()) - 1
        path.write_text(json.dumps(data))
    elif mutation == 'extra-private-file':
        (public / 'catalog.sqlite').write_bytes(b'private database must never be served')
    elif mutation == 'corrupt-package':
        (public / 'v1/packages' / sha).write_bytes(b'changed')
    else:
        (public / 'v1/packages' / sha).unlink()
    with pytest.raises(ModError):
        validate_snapshot(public)


def test_failed_publication_does_not_leave_a_partial_snapshot(tmp_path):
    repo, _, _ = approved(tmp_path)
    destination = tmp_path / 'public'
    with pytest.raises(ModError, match='budget'):
        export_snapshot(repo, destination, byte_limit=1)
    assert not destination.exists()
    assert not list(tmp_path.glob('.mod-publication-*'))


def test_mac_host_supervisor_stops_at_publication_expiry(tmp_path, monkeypatch, trust_keys):
    spec = importlib.util.spec_from_file_location('workshop_mac_host', ROOT / 'tools/mods/hosting/host-mac.py')
    host = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(host)
    repo = Repository(tmp_path / 'repository')
    public = tmp_path / 'public'
    from mod_trust import PublicationSigner, make_root
    pinned = make_root(trust_keys[:2], [trust_keys[4]], 1, 'https://workshop.example.test', int(time.time()) - 1)
    export_snapshot(repo, public, signer=PublicationSigner(pinned, [trust_keys[4]]))
    monkeypatch.setattr(host.platform, 'system', lambda: 'Darwin')
    monkeypatch.setattr(host.shutil, 'which', lambda _: '/test/caddy')
    calls = []
    def run(command, **kwargs):
        calls.append(command)
        assert kwargs['env']['TSFP_MOD_PUBLIC_ROOT'] == str(public)
        if command[1] == 'validate':
            return subprocess.CompletedProcess(command, 0)
        assert 0 < kwargs['timeout'] <= SCAN_TTL
        raise subprocess.TimeoutExpired(command, kwargs['timeout'])
    monkeypatch.setattr(host.subprocess, 'run', run)
    with pytest.raises(ModError, match='expired'):
        host.serve(public, pinned)
    assert [call[1] for call in calls] == ['validate', 'run']
