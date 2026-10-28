"""Publish a tested, pushed commit with matching version, notes, tag and assets.

No checkout switching, version edits, force pushes or published-release updates.
GitHub CLI authentication is used; tokens never enter command arguments or files.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
from contextlib import contextmanager
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import time
from urllib.parse import urlsplit
import zipfile

ROOT = _layout_root
TEXTURE_SPEC = importlib.util.spec_from_file_location('publish_textures', ROOT/'tools/textures/build_textures.py')
CHANNEL_SPEC = importlib.util.spec_from_file_location('release_channels', ROOT/'tools/release/release_channels.py')
CHANNELS = importlib.util.module_from_spec(CHANNEL_SPEC)
CHANNEL_SPEC.loader.exec_module(CHANNELS)
TEXTURES = importlib.util.module_from_spec(TEXTURE_SPEC)
TEXTURE_SPEC.loader.exec_module(TEXTURES)
SEMVER = re.compile(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z')


def version_key(version):
    match = SEMVER.fullmatch(version.removeprefix('v'))
    if not match:
        raise ValueError('VERSION must contain a semantic version, for example 0.2.0-alpha.3')
    major, minor, patch, prerelease, _ = match.groups()
    identifiers = []
    for item in prerelease.split('.') if prerelease else []:
        if item.isdigit() and len(item)>1 and item.startswith('0'):
            raise ValueError('Numeric prerelease identifiers must not start with zero')
        identifiers.append((0, int(item)) if item.isdigit() else (1, item))
    return int(major), int(minor), int(patch), prerelease is None, tuple(identifiers)


def github_repository(remote):
    if remote.startswith('git@github.com:'):
        path = remote[len('git@github.com:'):]
    else:
        url = urlsplit(remote)
        if url.scheme != 'https' or url.hostname != 'github.com' or url.username or url.password:
            raise ValueError('origin must use a GitHub HTTPS or git@github.com SSH URL')
        path = url.path.lstrip('/')
    path = path.rstrip('/').removesuffix('.git')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', path):
        raise ValueError('Could not determine the GitHub repository from origin')
    return path


def last_published(pages):
    releases = [release for page in pages for release in page]
    published = []
    for release in releases:
        tag = release.get('tag_name', '')
        if (release['draft'] or not release.get('published_at')
                or not tag.startswith('v') or not SEMVER.fullmatch(tag[1:])):
            continue
        try:
            version_key(tag[1:])
        except ValueError:
            continue
        published.append(release)
    return releases, max(published, key=lambda release: release['published_at'], default=None)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def game_release_path(path):
    """Game source/runtime inputs, excluding launcher and release housekeeping."""
    return (path.startswith(('src/', 'patches/', 'config/', 'assets/textures/'))
            or path in ('tsfp.ps1', 'setup.ps1', 'requirements.txt', 'CMakeLists.txt',
                        'tools/build/build-game-icon.ps1', 'tools/launcher/IconBuilder.cs',
                        'tools/launcher/GameMonogram.cs')
            or path in {
                'tools/build/build-native-mac.py',
                'tools/build/crash_helper.c',
                'tools/build/fetch-video.ps1',
                'tools/build/native_mac_common.py',
                'tools/build/network-identity.py',
                'tools/build/recomp-generation.py',
                'tools/network/bundle_online.py',
                'tools/network/lobby_server.py',
                'tools/network/online.py',
                'tools/network/online_client.py',
                'tools/network/online_host.py',
                'tools/network/online_paths.py',
                'tools/network/online_transport.py',
                'tools/play-native-mac.py',
                'tools/play.py',
                'tools/playable-launch.ps1',
                'tools/project_paths.py',
                'tools/release/Assemble.cmd',
                'tools/release/Assemble.ps1',
                'tools/release/Import-game.ps1',
                'tools/release/ImportGame.cs',
                'tools/release/Play.cmd',
                'tools/release/crash-symbols.py',
                'tools/start-game.ps1',
                'tools/textures/texture-pack.py'})


def check_directory(root, path, *, descendants=True):
    """Validate all ancestors before tools recursively create/clean work files."""
    root, path = Path(root).absolute(), Path(path).absolute()
    if path == root or not path.is_relative_to(root):
        raise ValueError('Release work must stay inside the workspace')
    for item in [root, *[parent for parent in reversed(path.parents) if parent.is_relative_to(root)], path]:
        if item.is_symlink() or (item.exists() and os.name == 'nt' and item.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT):
            raise ValueError(f'Redirected release work path: {item}')
    if descendants and path.is_dir():
        for item in path.rglob('*'):
            if item.is_symlink() or (os.name == 'nt' and item.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT):
                raise ValueError(f'Redirected release work path: {item}')


def clean_pytest_current_links(root, path):
    """Unlink pytest's sibling aliases, never their targets or arbitrary links."""
    root, path = Path(root).absolute(), Path(path).absolute()
    if path != root/'build/release-tests':
        return
    check_directory(root, path, descendants=False)
    if not path.is_dir():
        return
    links = []
    for item in path.rglob('*'):
        redirected = item.is_symlink() or (os.name == 'nt' and item.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)
        if not redirected:
            continue
        target = item.resolve()
        if (not item.is_symlink() or item.parent != path or not item.name.endswith('current') or
                target.parent != path.resolve() or
                not re.fullmatch(re.escape(item.name[:-7])+r'[0-9]+', target.name) or
                (target.exists() and not target.is_dir())):
            raise ValueError(f'Unexpected link in release test files: {item}')
        links.append(item)
    # Validate every link before mutating any; unlink never follows the target.
    for link in links:
        link.unlink()


@contextmanager
def release_lock(path):
    with path.open('a+b') as stream:
        if not stream.tell():
            stream.write(b'0'); stream.flush()
        stream.seek(0)
        try:
            if os.name == 'nt':
                import msvcrt
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError('Another release preparation is already running') from error
        try:
            yield
        finally:
            stream.seek(0)
            if os.name == 'nt':
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(stream, fcntl.LOCK_UN)


class Publisher:
    def __init__(self, root=ROOT, *, branch="main", channel_name=None, notes_file=None):
        self.source_branch = branch
        self.channel_name = channel_name
        self.notes_file = Path(notes_file).resolve() if notes_file else None
        self.root = Path(root).resolve()
        self.output = self.root/'build/releases'
        self.python = self.root/'.venv/Scripts/python.exe'
        self.timings = []
        self._verified_archive = None

    @contextmanager
    def timed(self, stage):
        start = time.perf_counter()
        outcome = 'failed'
        try:
            yield
            outcome = 'passed'
        finally:
            seconds = time.perf_counter() - start
            self.timings.append(dict(stage=stage, seconds=round(seconds, 3), outcome=outcome))
            print(f'[release timing] {stage}: {seconds:.2f}s ({outcome})', flush=True)

    def timed_run(self, stage, arguments):
        with self.timed(stage):
            return self.run(arguments, live=True)

    def save_timings(self):
        path = self.output/'last-run-timings.json'
        check_directory(self.root, path)
        temporary = path.with_suffix('.json.tmp')
        check_directory(self.root, temporary)
        temporary.write_text(json.dumps(dict(source_commit=getattr(self, 'commit', None),
                                             stages=self.timings), indent=2)+'\n', encoding='utf-8')
        temporary.replace(path)

    def run(self, arguments, *, live=False, payload=None):
        environment = dict(os.environ, GH_PROMPT_DISABLED='1', GIT_TERMINAL_PROMPT='0',
                           PYTHONDONTWRITEBYTECODE='1')
        result = subprocess.run([str(arg) for arg in arguments], cwd=self.root, env=environment,
                                input=json.dumps(payload) if payload is not None else None,
                                capture_output=not live, text=True, encoding='utf-8', errors='replace')
        if result.returncode:
            raise RuntimeError(f'{Path(arguments[0]).name} failed (exit {result.returncode}).\n'
                               + ((result.stderr or result.stdout or '') if not live else 'See output above.'))
        return (result.stdout or '').strip()

    def git(self, *arguments):
        return self.run(['git', *arguments])

    def gh(self, *arguments, payload=None):
        return self.run(['gh', *arguments], payload=payload)

    def clean(self):
        if self.git('status', '--porcelain'):
            raise ValueError('Commit/merge all source changes first. Releases require a clean checkout.')

    def preflight(self):
        self.clean()
        if (not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._/-]*', self.source_branch)
                or '..' in self.source_branch or self.source_branch.endswith('/')
                or self.git('branch', '--show-current') != self.source_branch):
            raise ValueError('Check out the intended branch and pass --branch for a non-main release.')
        if not shutil.which('gh'):
            raise ValueError('Install GitHub CLI (gh), then run gh auth login before releasing.')
        self.repo = github_repository(self.git('remote', 'get-url', 'origin'))
        self.gh('auth', 'status', '--hostname', 'github.com')
        self.git('fetch', 'origin', f'refs/heads/{self.source_branch}:refs/remotes/origin/{self.source_branch}', '--tags')
        self.commit = self.git('rev-parse', 'HEAD')
        if self.commit != self.git('rev-parse', 'refs/remotes/origin/'+self.source_branch):
            raise ValueError('The source branch must match its origin branch exactly. Push or pull --ff-only, then retry.')
        self.version = (self.root/'VERSION').read_text().strip()
        if self.version.startswith('v'):
            raise ValueError('VERSION contains the number without the v prefix')
        key = version_key(self.version)
        self.tag = 'v'+self.version
        self.prerelease = not key[3]
        self.channel = CHANNELS.identity(self.version, publishing=True)
        if self.channel_name:
            if self.channel['channel']=='beta' and self.channel_name!='Beta':
                raise ValueError('The beta track is named Beta; custom display names are for feature tracks.')
            if not self.prerelease or not self.channel_name.strip() or len(self.channel_name)>80 or any(ord(c)<32 for c in self.channel_name):
                raise ValueError('A preview display name must be 1-80 printable characters.')
            self.channel['name'] = self.channel_name
        if not self.prerelease and self.source_branch != 'main' and not self.source_branch.startswith(('release/', 'hotfix/')):
            raise ValueError('Stable releases require main or an explicit release/ or hotfix/ branch.')
        self.name = 'TimeSplitters-Future-Perfect-PC-'+self.tag+'-windows-x64'
        self.stage = self.output/self.name
        self.archive = self.output/(self.name+'.zip')
        self.checksum = self.output/(self.name+'.zip.sha256')
        self.metadata = self.output/(self.name+'.BUILD.json')
        self.notes = self.output/(self.name+'.notes.md')
        self.plan = self.output/(self.name+'.release.json')
        pages = json.loads(self.gh('api', '--paginate', '--slurp', f'repos/{self.repo}/releases?per_page=100'))
        releases, _ = last_published(pages)
        if any(release['tag_name'] == self.tag for release in releases):
            raise ValueError(f'{self.tag} already has a release or draft. Inspect it on GitHub; it will not be overwritten.')
        published = [r for r in releases if not r.get('draft') and r.get('published_at')]
        same_series, stable, previews = [], [], []
        for release in published:
            try:
                tag = release['tag_name']
                if not tag.startswith('v'):
                    continue
                old_key = version_key(tag)
                old_channel = CHANNELS.identity(tag)['channel']
            except (ValueError, KeyError):
                continue
            if old_channel == 'stable' and not release.get('prerelease'):
                stable.append(release)
            elif release.get('prerelease') and old_key < key:
                previews.append(release)
            if (old_channel == self.channel['channel'] and
                    (old_key[:3] == key[:3] if self.prerelease else old_key[:2] == key[:2])):
                same_series.append(release)
        if same_series and key <= max(version_key(r['tag_name']) for r in same_series):
            raise ValueError('Update VERSION beyond the published versions in this release series.')
        self.previous = None
        self.baseline = None
        if self.prerelease:
            candidates = (sorted(same_series, key=lambda r: version_key(r['tag_name']), reverse=True)
                          + sorted(stable, key=lambda r: version_key(r['tag_name']), reverse=True))
        else:
            candidates = sorted((r for r in stable if version_key(r['tag_name']) < key),
                                key=lambda r: version_key(r['tag_name']), reverse=True)
            # Bootstrap the first Stable changelog from the latest published game
            # preview (including historical alphas), then use Stable baselines forever.
            if not stable:
                candidates = sorted(previews, key=lambda r: r['published_at'], reverse=True)
        # Notes compare only against an ancestor; unrelated experimental branches
        # must never become a baseline merely because they were published later.
        for release in candidates:
            revision = self.git('rev-parse', '--verify', 'refs/tags/'+release['tag_name']+'^{commit}')
            try:
                self.git('merge-base', '--is-ancestor', revision, self.commit)
            except RuntimeError:
                continue
            self.previous, self.baseline = release, revision
            break
        if not self.prerelease and self.source_branch != 'main' and self.previous not in stable:
            raise ValueError('A maintenance branch must descend from a published Stable release.')
        if self.baseline == self.commit:
            raise ValueError('There are no new source commits since the last published release')
        self.stable_base = None
        for release in sorted(stable, key=lambda r: version_key(r['tag_name']), reverse=True):
            revision = self.git('rev-parse', '--verify', 'refs/tags/'+release['tag_name']+'^{commit}')
            try:
                self.git('merge-base', '--is-ancestor', revision, self.commit)
            except RuntimeError:
                continue
            self.stable_base = release['tag_name']
            break
        self.channel['latest_stable'] = max(stable, key=lambda r: version_key(r['tag_name']))['tag_name'] if stable else None
        self.channel['includes_latest_stable'] = bool(stable and self.channel['latest_stable'] == self.stable_base)
        self.channel.update(source_commit=self.commit, source_branch=self.source_branch,
                            stable_base=self.stable_base, platforms=['windows-x64'])
        self.require_repository_textures()
        self.validate_remote()

    def require_repository_textures(self):
        """Enforce the repository delivery policy without hydrating optional assets."""
        manifest = json.loads((self.root/'config/textures/texture-pack.json').read_text())
        if manifest.get('schema') == 2 and manifest.get('delivery') == 'optional':
            self.texture_identity = TEXTURES.optional_identity(self.root)
            self.texture_files = {}
            return
        if (manifest.get('schema') != 2 or manifest.get('delivery') != 'bundled'
                or manifest.get('path') != 'assets/textures'):
            raise ValueError('Releases require the bundled repository texture pack in assets/textures.')
        _, records, identity = TEXTURES.ensure_pack(self.root)
        if any(manifest.get(key) != identity[key] for key in ('texture_count', 'installed_bytes')):
            raise ValueError('Repository textures do not match the declared count/size in config/textures/texture-pack.json.')
        for path, public, _ in records:
            if path.with_suffix('.json').read_bytes() != public:
                raise ValueError(f'Repository texture metadata must be sanitized: {path.name}')
        self.texture_identity = identity
        self.texture_files = {name: sha for name, _, sha in TEXTURES.zip_entries(records)}

    def require_package_textures(self, build):
        files = {name: sha for name, sha in build.get('files', {}).items() if name.startswith('Textures/')}
        if build.get('textures') != self.texture_identity or files != self.texture_files:
            raise ValueError('Optional texture delivery requires a game package without textures.' if self.texture_identity is None
                             else 'Release package must include every repository texture and its metadata with matching hashes/count/size.')

    def validate_remote(self):
        if self.git('rev-parse', 'HEAD') != self.commit or self.git('branch', '--show-current') != self.source_branch:
            raise ValueError('The checkout changed during release preparation')
        self.clean()
        rows = self.git('ls-remote', 'origin', 'refs/heads/'+self.source_branch, 'refs/tags/'+self.tag, 'refs/tags/'+self.tag+'^{}').splitlines()
        refs = dict((row.split()[1], row.split()[0]) for row in rows)
        if refs.get('refs/heads/'+self.source_branch) != self.commit:
            raise ValueError('The remote source branch changed. Update it and prepare the release again.')
        tagged = refs.get('refs/tags/'+self.tag+'^{}', refs.get('refs/tags/'+self.tag))
        if tagged and tagged != self.commit:
            raise ValueError('The release tag already points at a different commit; it will not be moved')

    def game_pr_notes(self, generated):
        commits = set(self.git('rev-list', f'{self.baseline}..{self.commit}'
                               if self.baseline else self.commit).splitlines())
        numbers = dict.fromkeys(re.findall(
            rf'https://github\.com/{re.escape(self.repo)}/pull/(\d+)\b', generated))
        rows = []
        for number in numbers:
            endpoint = f'repos/{self.repo}/pulls/{number}'
            pr = json.loads(self.gh('api', endpoint))
            if not pr.get('merged_at') or pr.get('merge_commit_sha') not in commits:
                continue
            pages = json.loads(self.gh('api', '--paginate', '--slurp', endpoint+'/files?per_page=100'))
            files = [item for page in pages for item in page]
            if pr['changed_files'] > 3000 and len(files) == 3000:
                # GitHub caps this endpoint at 3,000 files. The merge commit was
                # already checked against the release range above; use its full
                # first-parent diff, including both sides of renamed paths.
                merge = pr['merge_commit_sha']
                names = self.git('diff', '--no-renames', '--name-only', '-z', merge+'^1', merge)
                files = [{'filename': name} for name in names.rstrip('\0').split('\0') if name]
                if not files:
                    raise ValueError(f'Empty local changed-file list for PR #{number}')
            elif len(files) != pr['changed_files']:
                raise ValueError(f'Incomplete changed-file list for PR #{number}; cannot classify release notes safely.')
            if not any(game_release_path(item[key]) for item in files
                       for key in ('filename', 'previous_filename') if key in item):
                continue
            title = pr['title'].replace('\n', ' ').replace('\r', ' ')
            title = title.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
            title = re.sub(r'([\\`*_[\]])', r'\\\1', title)
            rows.append(f'- {title} ([#{number}](https://github.com/{self.repo}/pull/{number}))')
        changes = '\n'.join(rows) or 'No merged game-related pull requests in this release.'
        if self.previous:
            changes += f'\n\n**Full Changelog**: https://github.com/{self.repo}/compare/{self.previous["tag_name"]}...{self.tag}'
        return changes

    def write_notes(self):
        payload = dict(tag_name=self.tag, target_commitish=self.commit)
        if self.previous:
            payload['previous_tag_name'] = self.previous['tag_name']
        generated = json.loads(self.gh('api', '--method', 'POST', f'repos/{self.repo}/releases/generate-notes',
                                       '--input', '-', payload=payload)).get('body', '').strip()
        generated = self.game_pr_notes(generated)
        extra_notes = ''
        if self.notes_file:
            if self.notes_file.stat().st_size>64*1024:
                raise ValueError('Maintainer release notes must be at most 64 KiB.')
            extra_notes = self.notes_file.read_text(encoding='utf-8').strip()
            if '<!-- tsfp-release ' in extra_notes:
                raise ValueError('Maintainer notes must not override channel metadata.')
        baseline = self.previous['tag_name'] if self.previous else 'the beginning of the project'
        channel_metadata = '<!-- tsfp-release '+json.dumps(self.channel, separators=(',', ':'))+' -->'
        stable_note = self.stable_base or 'No published Stable release is an ancestor of this build.'
        missing_note = ('This build does not include the latest Stable release, '+self.channel['latest_stable']+'.'
                        if self.prerelease and self.channel['latest_stable'] and not self.channel['includes_latest_stable'] else '')
        body = f'''TimeSplitters: Future Perfect PC **{self.tag}** for Windows 10/11 x64.

This is an experimental PC port. Original US Xbox game files are required and are not included.

{channel_metadata}

**Game channel:** {self.channel['name']}. **Included Stable baseline:** {stable_note}

{missing_note}

{'Opt-in test build. Features and save/network compatibility may change. Review Known issues before testing.' if self.prerelease else 'Stable game release. The port remains experimental; see known platform limitations.'}

{extra_notes}

## Changes since {baseline}

{generated}

## Download and play

### Windows launcher (recommended)

1. [Download the Windows launcher](https://github.com/{self.repo}/releases/latest) and extract its ZIP.
2. Open **TimeSplitters Launcher.exe**.
3. Set **Update channel** to **{self.channel['name']}**, then select **Install release**.
4. Import your US Xbox game using **Locate ISO** or **Use extracted files**. You only need to do this once.
5. Select **Play Game**.

The launcher downloads and verifies updates, keeping your game data, settings and saves.

### Portable ZIP (alternative)

1. Download **{self.archive.name}** from **Assets** below.
2. Extract the entire ZIP to a writable folder.
3. Run **Play.cmd** and select your US Xbox ISO or extracted game folder when prompted.

When upgrading, preserve **Data**, **Settings**, **Saves** and **Textures**.
Use the companion `.zip.sha256` file to verify the download.

**Requirements:** Windows 10/11 x64 and original US Xbox game files.
Python, Git, Visual Studio and an emulator are not required. After importing the game, the ISO is no longer needed.

'''
        self.notes.write_text(body, encoding='utf-8', newline='\n')

    def recycle(self, path):
        clean_pytest_current_links(self.root, path)
        check_directory(self.root, path)
        self.run(['powershell.exe', '-NoProfile', '-File', self.root/'tools/release/recycle-release-work.ps1',
                  '-LiteralPath', path], live=True)

    def build_package(self):
        for path in (self.stage, self.archive, self.checksum, self.metadata):
            if path.exists():
                raise ValueError(f'Existing release output: {path.name}. Preserve/recycle the old output or use a new VERSION; nothing was overwritten.')
        # An existing setup environment can predate newly pinned dependencies.
        # Prepare the same interpreter that runs the release tests and tools.
        self.timed_run('Dependencies', [self.python, '-m', 'pip', 'install', '-r',
                  self.root/'requirements.txt'])
        tests = self.root/'build/release-tests'
        clean_pytest_current_links(self.root, tests)
        check_directory(self.root, tests)
        # Build already validates/applies the complete patch stack.
        self.timed_run('Game build and native tests', ['powershell.exe', '-NoProfile', '-File', self.root/'tsfp.ps1', 'Build'])
        try:
            self.timed_run('Release regression suite', [self.python, '-m', 'pytest', 'tests', '-q', '-p', 'no:cacheprovider',
                      '--durations=20', '-o', 'tmp_path_retention_policy=failed', '--basetemp', tests])
        except Exception:
            if tests.exists():
                try:
                    self.recycle(tests)
                except Exception as cleanup_error:
                    print(f'Release test cleanup also failed: {cleanup_error}', file=sys.stderr)
            raise
        else:
            if tests.exists():
                self.recycle(tests)
        self.timed_run('Package and launcher checks', [self.python, self.root/'tools/release/package-release.py'])
        self.verify_package()
        # Hashes alone do not prove the installed launcher accepts the layout.
        # Exercise its production unpack/install path on this exact ZIP.
        self.timed_run('Install exact package', [self.root/'build/launcher/launcher-tests.exe', '--verify-package',
                  self.archive, self.tag])
        self.timed_run('Portable online checks', [self.python, self.root/'tests/network/portable_online.py', '--stage', self.stage,
                  '--game', self.root/'private/game'])
        self.verify_package()  # A check must not modify files being distributed.
        shutil.copyfile(self.stage/'BUILD.json', self.metadata)

    def verify_package(self):
        with self.timed('Package integrity'):
            self._verify_package()

    def _verify_package(self):
        staged = self.stage.exists()
        build = json.loads(((self.stage/'BUILD.json') if staged else self.metadata).read_text())
        if build['source_commit'] != self.commit or build['version'] != self.tag or build['source_changes']:
            raise ValueError('Package source/version does not match the clean release commit')
        self.require_package_textures(build)
        if build.get('release_channel') != CHANNELS.identity(self.tag):
            raise ValueError('Package channel does not match the release version.')
        expected = self.checksum.read_text(encoding='ascii').split()
        archive_digest = digest(self.archive)
        if expected != [archive_digest, self.archive.name]:
            raise ValueError('Package checksum mismatch')
        if self.archive.stat().st_size >= 2*1024**3:
            raise ValueError('ZIP exceeds the GitHub per-asset limit; do not publish an incomplete download')
        # Always rehash bytes, including staged files: installer/probe checks may
        # have modified them. Only decompression is memoized, never file hashes.
        identity = (archive_digest, json.dumps(build, sort_keys=True), self.name)
        for name, expected_hash in build['files'].items():
            if name.startswith('/') or '\\' in name or ':' in name or '..' in name.split('/'):
                raise ValueError(f'Unsafe package path: {name}')
            if name.startswith(('Data/', 'Saves/', 'Logs/')) or name.lower().endswith(('.iso', '.xbe', '.pak', '.img', '.pdb')):
                raise ValueError(f'Unexpected private game/test file in package: {name}')
            texture = re.fullmatch(r'Textures/[0-9a-f]{16}\.(dds|json)', name)
            if name.startswith('Textures/') and (not texture or not build.get('textures')):
                raise ValueError(f'Unexpected texture package member: {name}')
            if staged and not texture and digest(self.stage/name) != expected_hash:
                raise ValueError(f'Staged file changed: {name}')
        if self._verified_archive == identity:
            return
        with zipfile.ZipFile(self.archive) as archive:
            # Reading each member to EOF below checks ZIP CRC as well as SHA-256.
            # testzip() would decompress the entire archive a second time.
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise ValueError('Duplicate ZIP members')
            prefix = self.name+'/'
            if set(names) != {prefix+name for name in build['files']} | {prefix+'BUILD.json'}:
                raise ValueError('ZIP members do not match BUILD.json')
            for name, expected_hash in build['files'].items():
                with archive.open(prefix+name) as stream:
                    if hashlib.file_digest(stream, 'sha256').hexdigest() != expected_hash:
                        raise ValueError(f'Archived file changed: {name}')
            if json.loads(archive.read(prefix+'BUILD.json')) != build:
                raise ValueError('Archived build metadata differs from the validated stage')
        self._verified_archive = identity

    def prepare(self):
        self.output.mkdir(parents=True, exist_ok=True)
        if self.plan.exists():
            prepared = json.loads(self.plan.read_text())
            if (prepared['commit'] != self.commit or prepared['tag'] != self.tag or prepared['repo'] != self.repo
                    or prepared.get('channel') != self.channel):
                raise ValueError('Prepared output belongs to another commit/version/repository')
            if set(prepared['assets']) != {path.name for path in (self.archive, self.checksum, self.metadata)}:
                raise ValueError('Prepared release has an incomplete asset list')
            for name, expected in prepared['assets'].items():
                if Path(name).name != name or digest(self.output/name) != expected:
                    raise ValueError('Prepared release assets changed; they will not be published')
            print('Reusing the validated package for this exact source commit.', flush=True)
            # The staging folder may already have been recycled.
            self.verify_package()
        else:
            self.build_package()
        self.require_package_textures(json.loads(self.metadata.read_text()))
        assets = {path.name: digest(path) for path in (self.archive, self.checksum, self.metadata)}
        temporary = self.plan.with_suffix('.json.tmp')
        temporary.write_text(json.dumps(dict(commit=self.commit, tag=self.tag, repo=self.repo,
                              previous=self.previous['tag_name'] if self.previous else None, channel=self.channel, assets=assets), indent=2)+'\n')
        temporary.replace(self.plan)
        self.validate_remote()
        self.write_notes()  # Refresh the baseline; a network failure can resume without rebuilding.
        if self.stage.exists():
            self.verify_package()
            self.recycle(self.stage)
        print(f'Notes: {self.notes}\nPackage: {self.archive}', flush=True)

    def require_launcher(self):
        build = json.loads(self.metadata.read_text())
        self.require_package_textures(build)
        minimum = CHANNELS.MINIMUM_LAUNCHER
        latest = json.loads(self.gh('api', f'repos/{self.repo}/releases/latest'))
        tag = latest.get('tag_name', '')
        version = tag.removeprefix('launcher-v')
        if (not tag.startswith('launcher-v') or not SEMVER.fullmatch(version)
                or latest.get('draft') or latest.get('prerelease')
                or not latest.get('published_at')
                or version_key(version) < version_key(minimum)):
            raise ValueError(f'Publish a stable launcher {minimum} or newer before publishing game channels.')
        assets = {a['name'] for a in latest.get('assets', [])
                  if a.get('state') == 'uploaded' and a.get('size', 0) > 0}
        required = {f'TimeSplitters-Launcher-v{version}-{platform}.zip'
                    for platform in ('windows-x64', 'macos-arm64')}
        if not required <= assets:
            raise ValueError('The compatible launcher release must contain both Windows and Mac packages.')

    def publish(self, draft=False):
        self.validate_remote()
        plan = json.loads(self.plan.read_text())
        for path in (self.archive, self.checksum, self.metadata):
            if digest(path) != plan['assets'][path.name]:
                raise ValueError('Prepared assets changed before upload')
        self.verify_package()
        self.require_launcher()
        # Pin the tag before creating the draft. GitHub rejects a concurrent
        # attempt to create the same ref, so an existing tag is never moved.
        existing = self.git('ls-remote', 'origin', 'refs/tags/'+self.tag)
        if not existing:
            created = json.loads(self.gh('api', '--method', 'POST', f'repos/{self.repo}/git/refs', '--input', '-',
                                 payload=dict(ref='refs/tags/'+self.tag, sha=self.commit)))
            if created['ref'] != 'refs/tags/'+self.tag or created['object']['sha'] != self.commit:
                raise ValueError('GitHub tag did not match the tested commit; no release was created')
        self.validate_remote()
        arguments = ['release', 'create', self.tag, '--repo', self.repo, '--target', self.commit,
                     '--verify-tag', '--latest=false',
                     '--title', 'TSFP '+self.tag,
                     '--notes-file', self.notes, '--draft', self.archive, self.checksum, self.metadata]
        if self.prerelease:
            arguments.append('--prerelease')
        self.gh(*arguments)
        uploaded = json.loads(self.gh('release', 'view', self.tag, '--repo', self.repo,
                                     '--json', 'assets,isDraft,targetCommitish,url'))
        actual = {asset['name']: asset['size'] for asset in uploaded['assets']}
        expected = {path.name: path.stat().st_size for path in (self.archive, self.checksum, self.metadata)}
        if not uploaded['isDraft'] or actual != expected or uploaded['targetCommitish'] != self.commit:
            raise ValueError('Draft metadata/assets did not match. Inspect the draft on GitHub; it was not published.')
        if not draft:
            self.validate_remote()
            # Reserve /releases/latest for the stable launcher download.
            self.gh('release', 'edit', self.tag, '--repo', self.repo, '--draft=false', '--latest=false')
        print(('Draft created: ' if draft else 'Published: ')+uploaded['url'], flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--prepare-only', action='store_true', help='Build, test and save notes locally; no tag/release is created')
    group.add_argument('--draft', action='store_true', help='Upload a draft instead of publishing it')
    parser.add_argument('--branch', default='main', help='Exact pushed source branch; Stable also allows release/ and hotfix/ branches')
    parser.add_argument('--channel-name', help='Human-readable preview name, for example Story Co-op Preview')
    parser.add_argument('--notes-file', type=Path, help='Reviewed Markdown with feature notes, known issues and test instructions')
    args = parser.parse_args()
    publisher = Publisher(branch=args.branch, channel_name=args.channel_name, notes_file=args.notes_file)
    check_directory(publisher.root, publisher.output)
    publisher.output.mkdir(parents=True, exist_ok=True)
    with release_lock(publisher.output/'.publish.lock'):
        try:
            with publisher.timed('Total'):
                with publisher.timed('Preflight'):
                    publisher.preflight()
                publisher.prepare()
                if not args.prepare_only:
                    with publisher.timed('Upload and publication'):
                        publisher.publish(args.draft)
                else:
                    print('Prepared locally. Run Publish-Release.ps1 again to publish this validated package.')
        finally:
            # Timing output must not obscure the original release failure.
            try:
                publisher.save_timings()
            except (OSError, ValueError) as error:
                print(f'Could not save release timings: {error}', file=sys.stderr)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        print(f'Release stopped: {error}', file=sys.stderr)
        sys.exit(1)
