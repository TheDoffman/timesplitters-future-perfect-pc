"""Refresh issue-form version references and label reports using their answers."""

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
import json
import os
from pathlib import Path
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request

ROOT = _layout_root
FORMS = ('.github/ISSUE_TEMPLATE/01-bug-report.yml',)
SEMVER = re.compile(
    r'v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'
    r'(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?'
    r'(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?')
LABELS = {
    'bug': ('d73a4a', 'Something is not working'),
    'enhancement': ('0052cc', 'An improvement or feature request'),
    'platform: windows': ('0078d4', 'Reported on Windows'),
    'platform: macos': ('555555', 'Reported on macOS'),
}


class GitHub:
    def __init__(self, repository, token=None):
        if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
            raise ValueError('Use a repository in owner/name form.')
        self.base = 'https://api.github.com/repos/' + repository + '/'
        self.repository = repository
        self.token = token

    def request(self, method, path, payload=None):
        headers = {'Accept': 'application/vnd.github+json',
                   'X-GitHub-Api-Version': '2022-11-28',
                   'User-Agent': 'TSFP-Issue-Forms'}
        if self.token:
            headers['Authorization'] = 'Bearer ' + self.token
        data = None if payload is None else json.dumps(payload).encode('utf-8')
        if data is not None:
            headers['Content-Type'] = 'application/json'
        request = urllib.request.Request(self.base + path, data=data,
                                         headers=headers, method=method)
        with urllib.request.urlopen(request, timeout=30) as response:
            content = response.read()
        return json.loads(content) if content else None

    def releases(self):
        result = []
        for page in range(1, 101):
            records = self.request('GET', f'releases?per_page=100&page={page}')
            if not isinstance(records, list):
                raise ValueError('GitHub did not return a release list.')
            result.extend(records)
            if len(records) < 100:
                return result
        raise ValueError('Release list exceeded 100 pages; forms were not changed.')


def version_key(version):
    match = SEMVER.fullmatch(version)
    if not match:
        raise ValueError('Invalid release version: ' + version)
    major, minor, patch, pre = match.groups()
    identifiers = ()
    if pre is not None:
        parts = pre.split('.')
        if any(p.isdecimal() and len(p) > 1 and p[0] == '0' for p in parts):
            raise ValueError('Invalid numeric prerelease identifier.')
        identifiers = tuple((0, int(p)) if p.isdecimal() else (1, p) for p in parts)
    return int(major), int(minor), int(patch), pre is None, identifiers


def release_versions(releases):
    versions = {'GAME': set(), 'LAUNCHER': set()}
    for release in releases:
        if release.get('draft') or not release.get('published_at'):
            continue
        tag = release.get('tag_name', '')
        kind = 'LAUNCHER' if tag.startswith('launcher-v') else 'GAME'
        version = tag.removeprefix('launcher-') if kind == 'LAUNCHER' else tag
        try:
            version_key(version)
        except ValueError:
            continue
        versions[kind].add(version)
    # Keep the reference list short; users can enter older/custom builds directly.
    return {kind: sorted(values, key=lambda v: (version_key(v), v), reverse=True)[:30]
            for kind, values in versions.items()}


def refresh_versions(root, releases, check=False):
    versions = release_versions(releases)
    updates = {}
    for relative in FORMS:
        path = root / relative
        original = path.read_text(encoding='utf-8')
        updated = original
        for kind, values in versions.items():
            pattern = re.compile(
                rf'(^        <!-- BEGIN AUTO {kind} VERSIONS -->\n).*?'
                rf'(^        <!-- END AUTO {kind} VERSIONS -->$)', re.M | re.S)
            block = pattern.search(original)
            if block is None:
                raise ValueError(f'Expected one {kind} version block in {relative}.')
            if not values and re.search(r'^        - `v', block.group(0), re.M):
                raise ValueError(f'Refusing to erase existing {kind} versions from an empty '
                                 'release result; verify GitHub releases and retry.')
            reference = ''.join('        - `' + v + '`\n' for v in values)
            if not reference:
                reference = '        No published releases.\n'
            updated, count = pattern.subn(lambda m: m[1] + reference + m[2], updated)
            if count != 1:
                raise ValueError(f'Expected one {kind} version block in {relative}.')
        if updated != original:
            updates[path] = updated
    # Validate every version block before writing.
    if not check:
        for path, text in updates.items():
            path.write_text(text, encoding='utf-8', newline='\n')
    return list(updates)


def publish_version_pr(client, root, changed, base_branch, base_sha):
    """Update one bot-owned PR without pushing to main or rewriting history."""
    if not changed:
        return None
    relatives = [path.relative_to(root).as_posix() for path in changed]
    if any(relative not in FORMS for relative in relatives):
        raise ValueError('Only generated issue-form references may be proposed.')
    base_ref = 'git/ref/heads/' + urllib.parse.quote(base_branch, safe='/')
    if client.request('GET', base_ref)['object']['sha'] != base_sha:
        raise ValueError('The default branch advanced; rerun the version refresh.')

    branch = 'automation/issue-form-versions'
    branch_ref = 'git/ref/heads/' + branch
    previous = None
    try:
        previous = client.request('GET', branch_ref)['object']['sha']
    except urllib.error.HTTPError as error:
        if error.code != 404:
            raise
    if previous and previous != base_sha:
        comparison = client.request('GET', f'compare/{base_sha}...{previous}')
        if any(file['filename'] not in FORMS or
               file.get('previous_filename', file['filename']) not in FORMS
               for file in comparison['files']):
            raise ValueError('The automation branch has other edits; refusing to replace them.')

    base_tree = client.request('GET', 'git/commits/' + base_sha)['tree']['sha']
    tree = client.request('POST', 'git/trees', {
        'base_tree': base_tree,
        'tree': [dict(path=relative, mode='100644', type='blob',
                      content=(root / relative).read_text(encoding='utf-8'))
                 for relative in relatives],
    })['sha']
    # Both the latest main and the previous bot tip remain ancestors. Building
    # the tree from main preserves unrelated changes and avoids force-pushes.
    parents = [base_sha] + ([previous] if previous and previous != base_sha else [])
    commit = client.request('POST', 'git/commits', {
        'message': 'Refresh issue form release references', 'tree': tree, 'parents': parents,
    })['sha']
    if previous:
        client.request('PATCH', 'git/refs/heads/' + branch, {'sha': commit, 'force': False})
    else:
        client.request('POST', 'git/refs', {'ref': 'refs/heads/' + branch, 'sha': commit})

    query = urllib.parse.urlencode(dict(state='open', base=base_branch,
                                       head=client.repository.split('/')[0] + ':' + branch))
    existing = client.request('GET', 'pulls?' + query)
    if existing:
        return existing[0]['html_url']
    created = client.request('POST', 'pulls', {
        'title': 'Refresh published version references in the bug form',
        'head': branch, 'base': base_branch,
        'body': '## Change\n\nRefresh the published game and launcher version references.\n\n'
                '## Cause and fix\n\nPublished releases changed. Regenerate only the bug '
                'form reference lists; required inputs remain empty and enhancement '
                'requests continue to use Ideas discussions. This PR follows main branch protection.\n\n'
                '## Validation\n\nApprove workflows to run if GitHub requests approval, '
                'then wait for the required checks. Enable auto-merge if desired.\n\n'
                '## Mac port backlog\n\nNot applicable: issue-form reference data only; '
                'no game or launcher behavior changes.\n',
    })
    return created['html_url']


def report_labels(body):
    body = (body or '').lstrip('\ufeff')
    fields = {name.strip(): value.strip() for name, value in re.findall(
        r'^### ([^\r\n]+)\r?\n(.*?)(?=^### |\Z)', body, re.M | re.S)}
    if {'Game version', 'Launcher version', 'Platform', 'What happened?'} <= fields.keys():
        desired = {'bug'}
    elif 'What would you like to add or change?' in fields:
        desired = {'enhancement'}
    else:
        return None
    platform = fields.get('Platform', '').casefold()
    if platform == 'windows' or platform.startswith('windows ') or platform in ('win10', 'win11'):
        desired.add('platform: windows')
    elif platform.startswith(('macos', 'mac os')):
        desired.add('platform: macos')
    return desired


def label_report(client, issue):
    if 'pull_request' in issue:
        return False
    desired = report_labels(issue.get('body'))
    if desired is None:
        return False
    current = {label['name'] for label in issue.get('labels', [])}
    missing = desired - current
    for name in sorted(missing):
        path = 'labels/' + urllib.parse.quote(name, safe='')
        try:
            client.request('GET', path)
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise
            color, description = LABELS[name]
            try:
                client.request('POST', 'labels',
                               dict(name=name, color=color, description=description))
            except urllib.error.HTTPError as create_error:
                if create_error.code != 422:
                    raise
                # Another issue may have created the same platform label first.
                client.request('GET', path)
    endpoint = f'issues/{issue["number"]}/labels'
    if missing:
        client.request('POST', endpoint, {'labels': sorted(missing)})
    for name in sorted((current & LABELS.keys()) - desired):
        client.request('DELETE', endpoint + '/' + urllib.parse.quote(name, safe=''))
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('versions', 'labels'))
    parser.add_argument('--repository', default=os.environ.get('GITHUB_REPOSITORY'))
    parser.add_argument('--check', action='store_true', help='Report stale versions without writing.')
    parser.add_argument('--pull-request', action='store_true', help='Propose refreshed versions through a PR.')
    parser.add_argument('--base-branch', default=os.environ.get('TARGET_BRANCH', 'main'))
    args = parser.parse_args()
    if not args.repository:
        parser.error('Set GITHUB_REPOSITORY or pass --repository owner/name.')
    if args.pull_request and (args.check or args.action != 'versions'):
        parser.error('--pull-request requires versions and cannot be combined with --check.')
    client = GitHub(args.repository, os.environ.get('GITHUB_TOKEN'))
    if args.action == 'versions':
        changed = refresh_versions(ROOT, client.releases(), check=args.check)
        print(f'{len(changed)} issue form(s) ' + ('need updates.' if args.check else 'updated.'))
        if args.pull_request:
            base_sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
            url = publish_version_pr(client, ROOT, changed, args.base_branch, base_sha)
            print(url or 'References already match published releases; no PR needed.')
        if args.check and changed:
            raise SystemExit(1)
    else:
        event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
        number = event['issue']['number']
        if not isinstance(number, int) or isinstance(number, bool) or number <= 0:
            raise ValueError('Invalid issue number.')
        # Read the latest report so queued edits do not apply stale platform labels.
        issue = client.request('GET', f'issues/{number}')
        applied = label_report(client, issue)
        print('Report labels synchronized.' if applied else 'No recognized report form; labels unchanged.')


if __name__ == '__main__':
    main()
