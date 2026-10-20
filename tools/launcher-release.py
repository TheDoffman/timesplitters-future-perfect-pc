#!/usr/bin/env python3
"""Validate and publish one tested launcher release for Windows and native Mac."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import urllib.error
import urllib.parse
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = 'TheDoffman/timesplitters-future-perfect-pc'
PLATFORMS = ('windows-x64', 'macos-arm64')
VERSION = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z.-]+))?\Z')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    checksum = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            checksum.update(block)
    return checksum.hexdigest()


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def source_version():
    source = (ROOT/'tools/launcher/LauncherVersion.cs').read_text()
    version = re.search(r'const string Value = "([^"]+)"', source).group(1)
    match = VERSION.fullmatch(version)
    require(match is not None, 'Use a numeric launcher version with an optional prerelease suffix.')
    if match[4]:
        require(all(part and (not part.isdigit() or part == '0' or not part.startswith('0'))
                    for part in match[4].split('.')), 'Invalid prerelease identifier.')
    for attribute in ('AssemblyVersion', 'AssemblyFileVersion'):
        require(re.search(attribute + r'\("' + re.escape('.'.join(match.groups()[:3])) + r'\.0"\)', source),
                'Keep launcher assembly and package versions in sync.')
    return version


def plan_release(event, kind, ref, sha, repository):
    version = source_version()
    publish = (kind == 'push' and ref.startswith('refs/tags/launcher-v')) or (
        kind == 'workflow_dispatch' and str(event.get('inputs', {}).get('publish', '')).lower() == 'true')
    tag = 'launcher-v' + version
    require(re.fullmatch('[0-9a-f]{40}', sha), 'Missing exact source revision.')
    if publish:
        require(repository == REPOSITORY, 'Publishing is restricted to the project repository.')
        default = event['repository']['default_branch']
        require((kind == 'push' and ref == 'refs/tags/' + tag) or
                (kind == 'workflow_dispatch' and ref == 'refs/heads/' + default),
                'Release tag must match LauncherVersion.cs; manual publication must use the default branch.')
        require(git('rev-parse', 'HEAD') == sha, 'Checkout does not match the requested release source.')
        subprocess.run(['git', 'fetch', '--no-tags', 'origin', default], cwd=ROOT, check=True)
        subprocess.run(['git', 'merge-base', '--is-ancestor', sha, 'FETCH_HEAD'], cwd=ROOT, check=True)
    return {'schema': 1, 'version': version, 'tag': tag, 'source_revision': sha,
            'repository': repository, 'publish': publish, 'prerelease': '-' in version,
            'default_branch': event.get('repository', {}).get('default_branch', 'main')}


def manifest_name(platform):
    return 'launcher-update.json' if platform == 'windows-x64' else 'launcher-update-macos-arm64.json'


def package_names(version, platform):
    archive = f'TimeSplitters-Launcher-v{version}-{platform}.zip'
    return [archive, archive + '.sha256', manifest_name(platform),
            'launcher-build-' + platform + '.json']


def allowed_file(name, platform):
    if platform == 'windows-x64':
        return name in ('TimeSplitters Launcher.exe', 'Bin/TimeSplitters.App.exe')
    prefix = 'TimeSplitters Launcher.app/Contents/'
    return name in (prefix+'Info.plist', prefix+'MacOS/TimeSplitters.Launcher',
                    prefix+'_CodeSignature/CodeResources') or (
        name.startswith(prefix+'MacOS/') and name.endswith('.dylib')) or (
        name.startswith(prefix+'Resources/') and name.endswith(('.txt', '.icns')))


def verify_package(directory, plan, platform):
    require(platform in PLATFORMS, 'Unsupported launcher platform.')
    directory = Path(directory)
    archive = directory / package_names(plan['version'], platform)[0]
    metadata = json.loads((directory/manifest_name(platform)).read_text(encoding='utf-8-sig'))
    limit = 16*1024**2 if platform == 'windows-x64' else 256*1024**2
    require(0 < archive.stat().st_size <= limit, 'Launcher archive size exceeds updater limits.')
    checksum = digest(archive)
    require(metadata == {'schema': 1, 'version': plan['version'], 'asset_name': archive.name,
                         'size': archive.stat().st_size, 'sha256': checksum}, 'Update manifest mismatch.')
    require(archive.with_suffix('.zip.sha256').read_text().split() == [checksum, archive.name],
            'Archive checksum sidecar mismatch.')
    with zipfile.ZipFile(archive) as package:
        entries = package.infolist()
        require(len(entries) <= (12 if platform == 'windows-x64' else 4096), 'Too many package entries.')
        require(sum(e.file_size for e in entries) <= limit*2, 'Expanded package exceeds updater limits.')
        names = [e.filename for e in entries]
        require(len(names) == len(set(n.casefold() for n in names)), 'Duplicate package path.')
        for entry in entries:
            path = PurePosixPath(entry.filename)
            require(not entry.is_dir() and not path.is_absolute() and
                    all(p not in ('', '.', '..') for p in entry.filename.split('/')) and
                    '\\' not in entry.filename and ':' not in entry.filename and
                    not stat.S_ISLNK(entry.external_attr >> 16), 'Unsafe package path.')
        installed = json.loads(package.read('Support/Launcher/manifest.json'))
        require(installed['schema'] == 1 and installed['version'] == plan['version'], 'Installed manifest version mismatch.')
        files = installed['files']
        require(set(names) == set(files) | {'Support/Launcher/manifest.json'}, 'Unmanifested package files.')
        required = ({'TimeSplitters Launcher.exe', 'Bin/TimeSplitters.App.exe'} if platform == 'windows-x64'
                    else {'TimeSplitters Launcher.app/Contents/Info.plist',
                          'TimeSplitters Launcher.app/Contents/_CodeSignature/CodeResources',
                          'TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher'})
        require(required <= set(files), 'Missing launcher entry point.')
        for name, expected in files.items():
            require(allowed_file(name, platform), 'Unexpected launcher payload: ' + name)
            require(hashlib.sha256(package.read(name)).hexdigest() == expected, 'Installed file hash mismatch.')
        if platform == 'macos-arm64':
            executable = package.getinfo('TimeSplitters Launcher.app/Contents/MacOS/TimeSplitters.Launcher')
            require(executable.external_attr >> 16 & 0o111, 'Mac app executable permissions are missing.')
    return archive


def stamp(directory, plan, platform):
    require(git('rev-parse', 'HEAD') == plan['source_revision'] and source_version() == plan['version'],
            'Package builder must use the planned source and version.')
    directory = Path(directory)
    archive = verify_package(directory, plan, platform)
    names = package_names(plan['version'], platform)
    write_json(directory/names[3], {'schema': 1, 'platform': platform, 'version': plan['version'],
        'source_revision': plan['source_revision'], 'archive_sha256': digest(archive)})


def release_assets(directory, plan):
    directory = Path(directory)
    assets = []
    for platform in PLATFORMS:
        archive = verify_package(directory, plan, platform)
        names = package_names(plan['version'], platform)
        receipt = json.loads((directory/names[3]).read_text())
        require(receipt == {'schema': 1, 'platform': platform, 'version': plan['version'],
                           'source_revision': plan['source_revision'], 'archive_sha256': digest(archive)},
                'Platform packages must come from the same tested source revision.')
        assets.extend(directory/name for name in names)
    return assets


class GitHub:
    def __init__(self, token):
        require(bool(token), 'GITHUB_TOKEN is required for publication.')
        self.token = token

    def call(self, method, path, data=None, upload=None):
        # Credentials only go to fixed GitHub API hosts, never a release-supplied URL.
        url = ('https://uploads.github.com' if upload else 'https://api.github.com') + path
        headers = {'Authorization': 'Bearer ' + self.token, 'Accept': 'application/vnd.github+json',
                   'X-GitHub-Api-Version': '2022-11-28', 'User-Agent': 'TSFP-launcher-release'}
        body = Path(upload).read_bytes() if upload else json.dumps(data).encode() if data is not None else None
        headers['Content-Type'] = 'application/octet-stream' if upload else 'application/json'
        try:
            opener = urllib.request.build_opener(NoRedirect)
            with opener.open(urllib.request.Request(url, data=body, headers=headers, method=method), timeout=120) as response:
                result = response.read(8*1024**2)
                return json.loads(result) if result else None
        except urllib.error.HTTPError as error:
            if error.code == 404 and method == 'GET':
                return None
            raise RuntimeError(f'GitHub {method} {path.split("?")[0]} failed: HTTP {error.code}') from None


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def tag_commit(api, base, tag):
    ref = api.call('GET', base+'/git/ref/tags/'+urllib.parse.quote(tag, safe=''))
    if ref is None:
        return None
    obj = ref['object']
    for _ in range(5):
        if obj['type'] == 'commit':
            return obj['sha']
        require(obj['type'] == 'tag', 'Release tag must resolve to a commit.')
        obj = api.call('GET', base+'/git/tags/'+obj['sha'])['object']
    raise ValueError('Excessively nested release tag.')


def asset_matches(record, path):
    return (record.get('state') == 'uploaded' and record.get('size') == path.stat().st_size and
            record.get('digest') == 'sha256:' + digest(path))


def publication_assets(directory, plan):
    # Verify both platforms before preparing aliases or contacting GitHub.
    assets = {path.name: path for path in release_assets(directory, plan)}
    for platform in PLATFORMS:
        archive = assets[package_names(plan['version'], platform)[0]]
        alias = f'TimeSplitters-Launcher-{platform}.zip'
        # Upload the verified archive again under the stable name. Never trust
        # a stale alias on disk, or make another local copy of the large ZIP.
        assets[alias] = archive
        checksum = Path(directory)/(alias + '.sha256')
        checksum.write_text(digest(archive) + '  ' + alias + '\n', encoding='utf-8')
        assets[checksum.name] = checksum
    return assets


def publish(directory, plan, api):
    require(plan['publish'] and plan['repository'] == REPOSITORY, 'This run is validation-only.')
    expected = publication_assets(directory, plan)
    base = '/repos/' + REPOSITORY
    version, sha, tag = plan['version'], plan['source_revision'], plan['tag']
    require(tag == 'launcher-v'+version, 'Release tag/version mismatch.')
    existing_sha = tag_commit(api, base, tag)
    require(existing_sha in (None, sha), 'The release tag points at a different commit; it will not be moved.')
    release = api.call('GET', base+'/releases/tags/'+urllib.parse.quote(tag, safe=''))
    marker = '<!-- launcher-release-source:' + sha + ' -->'
    if release:
        require(marker in release.get('body', '') and release.get('target_commitish') == sha,
                'Existing release belongs to another publication; it will not be overwritten.')
    if not plan['prerelease']:
        latest = api.call('GET', base+'/releases/latest')
        if latest and latest['tag_name'].startswith('launcher-v') and latest['tag_name'] != tag:
            match = VERSION.fullmatch(latest['tag_name'][10:])
            require(match and tuple(map(int, version.split('.'))) > tuple(map(int, match.groups()[:3])),
                    'A stable launcher release must be newer than the current Latest launcher.')
    if existing_sha is None:
        api.call('POST', base+'/git/refs', {'ref': 'refs/tags/'+tag, 'sha': sha})
    if release is None:
        notes = api.call('POST', base+'/releases/generate-notes', {'tag_name': tag, 'target_commitish': sha})
        download = 'https://github.com/'+REPOSITORY+'/releases/download/'+tag+'/'
        links = ' · '.join('['+label+']('+download+package_names(version, platform)[0]+')'
                           for label, platform in [('Download Windows x64', 'windows-x64'),
                                                   ('Download Mac (Apple Silicon)', 'macos-arm64')])
        body = ('One launcher release for Windows x64 and native Apple Silicon Mac. Download the ZIP for your platform; '
                'existing launchers select their matching update automatically. Saves and settings are preserved.\n\n'
                + links + '\n\n' +
                'Mac v1.2.19 users: manually replace the app and Support/Launcher/manifest.json with this release; '
                'that older updater cannot install the newly sealed app bundle. Preserve game files, saves, settings and ISO.\n\n'
                'The Mac app is ad-hoc signed; Developer ID signing/notarization and clean-machine installation '
                'remain pending. No game runtime or original game data is included.\n\n' + notes.get('body', '') + '\n\n' + marker)
        release = api.call('POST', base+'/releases', {'tag_name': tag, 'target_commitish': sha,
            'name': 'Launcher v'+version, 'body': body, 'draft': True,
            'prerelease': plan['prerelease'], 'make_latest': 'false'})
    endpoint = base+'/releases/'+str(release['id'])
    remote = api.call('GET', endpoint+'/assets?per_page=100')
    require(all(a['name'] in expected for a in remote), 'Release has unrelated assets; inspect it before retrying.')
    for name, path in expected.items():
        matches = [a for a in remote if a['name'] == name]
        require(len(matches) <= 1, 'Duplicate remote release asset.')
        if matches and asset_matches(matches[0], path):
            continue
        require(release['draft'], 'Published release assets are immutable; use a new launcher version.')
        if matches:
            api.call('DELETE', base+'/releases/assets/'+str(matches[0]['id']))
        api.call('POST', endpoint+'/assets?name='+urllib.parse.quote(name, safe=''), upload=path)
    remote = api.call('GET', endpoint+'/assets?per_page=100')
    require(len(remote) == len(expected) and {a['name'] for a in remote} == set(expected) and
            all(asset_matches(a, expected[a['name']]) for a in remote),
            'Uploaded release checksums differ; release stays a draft.')
    if release['draft']:
        api.call('PATCH', endpoint, {'draft': False, 'prerelease': plan['prerelease'],
                                    'make_latest': 'false' if plan['prerelease'] else 'true'})
    # Releases created with GITHUB_TOKEN do not trigger the release-event workflow.
    api.call('POST', base+'/actions/workflows/issue-form-versions.yml/dispatches', {'ref': plan['default_branch']})
    return 'https://github.com/'+REPOSITORY+'/releases/tag/'+tag


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('plan', 'stamp', 'verify', 'publish'))
    parser.add_argument('--plan', type=Path, default=Path('build/launcher-release-plan.json'))
    parser.add_argument('--directory', type=Path, default=Path('build/launcher-release'))
    parser.add_argument('--platform', choices=PLATFORMS)
    args = parser.parse_args()
    if args.command == 'plan':
        event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
        plan = plan_release(event, os.environ['GITHUB_EVENT_NAME'], os.environ['GITHUB_REF'],
                            os.environ.get('LAUNCHER_VALIDATION_SHA') or os.environ['GITHUB_SHA'], os.environ['GITHUB_REPOSITORY'])
        require(not os.environ.get('LAUNCHER_VALIDATION_SHA') or not plan['publish'],
                'An alternate validation source can never publish a release.')
        require(not os.environ.get('LAUNCHER_VALIDATION_SHA') or
                git('rev-parse', 'HEAD') == plan['source_revision'],
                'Validation checkout does not match its exact requested source.')
        write_json(args.plan, plan)
        if os.environ.get('GITHUB_OUTPUT'):
            with open(os.environ['GITHUB_OUTPUT'], 'a') as output:
                output.write('publish='+str(plan['publish']).lower()+'\nversion='+plan['version']+'\n')
        print('Validated launcher', plan['version'], 'at', plan['source_revision'])
    else:
        plan = json.loads(args.plan.read_text())
        if args.command == 'stamp':
            stamp(args.directory, plan, args.platform)
        elif args.command == 'verify':
            print('Verified both platform packages:', len(release_assets(args.directory, plan)), 'release assets')
        else:
            print(publish(args.directory, plan, GitHub(os.environ.get('GITHUB_TOKEN'))))


if __name__ == '__main__':
    main()
