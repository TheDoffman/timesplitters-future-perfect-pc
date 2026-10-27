"""Keep the pinned known-issues Discussion in sync with open bug reports."""

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
import html
import json
import os
from pathlib import Path
import re
import urllib.request


ROOT = _layout_root
MARKER = '<!-- tsfp-known-issues:v1 -->'
TRUSTED = {'OWNER', 'MEMBER', 'COLLABORATOR'}
QUERY = '''query($owner: String!, $name: String!, $number: Int!) {
  repository(owner: $owner, name: $name) {
    discussion(number: $number) { id title body url }
  }
}'''
UPDATE = '''mutation($id: ID!, $body: String!) {
  updateDiscussion(input: {discussionId: $id, body: $body}) {
    discussion { url }
  }
}'''


class GitHub:
    def __init__(self, token=None):
        self.token = token

    def request(self, method, path, payload=None):
        headers = {'Accept': 'application/vnd.github+json',
                   'X-GitHub-Api-Version': '2022-11-28',
                   'User-Agent': 'TSFP-Known-Issues'}
        if self.token:
            headers['Authorization'] = 'Bearer ' + self.token
        data = None if payload is None else json.dumps(payload).encode('utf-8')
        if data is not None:
            headers['Content-Type'] = 'application/json'
        request = urllib.request.Request('https://api.github.com/' + path,
                                         data=data, headers=headers, method=method)
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)


def pages(api, path):
    """Finish every page before publishing; never replace the post with a partial list."""
    records = []
    separator = '&' if '?' in path else '?'
    for page in range(1, 101):
        batch = api.request('GET', f'{path}{separator}per_page=100&page={page}')
        if not isinstance(batch, list):
            raise ValueError('GitHub did not return a complete list.')
        records.extend(batch)
        if len(batch) < 100:
            return records
    raise ValueError('Too many pages; Discussion was not changed.')


def workaround_section(body):
    """Only explicit Workaround headings count; ignore headings inside code fences."""
    active, level, fence, lines = False, 0, None, []
    for line in (body or '').splitlines():
        stripped = line.lstrip()
        opening = re.match(r'(`{3,}|~{3,})', stripped)
        if fence:
            if re.fullmatch(re.escape(fence[0]) + '{' + str(len(fence)) + r',}\s*', stripped):
                fence = None
            elif active:
                lines.append(line)
            continue
        if opening:
            fence = opening.group(1)
            continue
        heading = re.match(r'^(#{1,6})\s+(.+?)(?:\s+#+)?\s*$', line)
        if heading:
            if active and len(heading[1]) <= level:
                break
            if not active and heading[2].casefold() in ('workaround', 'workarounds'):
                active, level = True, len(heading[1])
                continue
        if active:
            lines.append(line)
    return ' '.join(' '.join(lines).split())


def safe_text(text, limit):
    """Render report text as plain table text, without HTML, pings or Markdown injection."""
    text = ' '.join(''.join(c for c in text if c in '\n\t' or ord(c) >= 32).split())
    if len(text) > limit:
        text = text[:limit - 1].rstrip() + '…'
    text = html.escape(text, quote=False).replace('@', '@\u200b')
    return re.sub(r'([\\`*_{}\[\]()#+.!|~])', r'\\\1', text)


def issue_workaround(api, repository, issue):
    if not issue.get('comments'):
        return 'No workaround documented yet.'
    comments = pages(api, f'repos/{repository}/issues/{int(issue["number"])}/comments')
    candidates = [comment for comment in comments
                  if comment.get('author_association') in TRUSTED
                  and (comment.get('user') or {}).get('type') == 'User'
                  and workaround_section(comment.get('body'))]
    if not candidates:
        return 'No workaround documented yet.'
    latest = max(candidates, key=lambda c: (c.get('updated_at') or '', int(c['id'])))
    text = safe_text(workaround_section(latest['body']), 600)
    url = f'https://github.com/{repository}/issues/{int(issue["number"])}#issuecomment-{int(latest["id"])}'
    return f'{text} ([maintainer note]({url}))'


def render(api, repository, labels):
    records = pages(api, f'repos/{repository}/issues?state=open&sort=created&direction=asc')
    issues = {int(issue['number']): issue for issue in records
              if 'pull_request' not in issue and issue.get('state') == 'open'
              and labels.intersection(label['name'].casefold() for label in issue['labels'])}
    base = f'https://github.com/{repository}'
    body = (f'{MARKER}\n'
            'This list is updated automatically from open bug reports. '
            'Closed or untagged reports are removed, '
            'and reopened bugs return if still labeled. '
            'A closed report does not necessarily mean a fix is in a published build.\n\n'
            'Workarounds below come from explicit maintainer notes on the linked issues. '
            'Follow each report for affected versions, evidence and the latest status.\n\n'
            f'## Open bug reports ({len(issues)})\n\n')
    if issues:
        body += '| Report | Workaround |\n| --- | --- |\n'
        for number, issue in sorted(issues.items()):
            title = safe_text(issue['title'], 200)
            note = issue_workaround(api, repository, issue)
            body += f'| [#{number} — {title}]({base}/issues/{number}) | {note} |\n'
    else:
        body += 'No open issues currently have a tracked bug label.\n'
    body += (f'\n## Reporting something else\n\n'
             f'Check the [issue list]({base}/issues) before opening a '
             f'[bug report]({base}/issues/new?template=01-bug-report.yml). '
             'Include your installed game version, launcher version, platform and steps to reproduce. '
             'Add evidence to an existing report if it covers the same problem.\n\n'
             'Maintainers: add or edit a comment with a `### Workaround` heading on an issue '
             'to update its workaround here. This post is generated; keep notes on the issue.\n')
    if len(body) > 60000:
        raise ValueError('Known-issues post is too large; Discussion was not changed.')
    return body


def discussion(api, repository, number):
    owner, name = repository.split('/')
    result = api.request('POST', 'graphql', {'query': QUERY, 'variables': {
        'owner': owner, 'name': name, 'number': number}})
    if result.get('errors'):
        raise RuntimeError('GitHub could not read the configured Discussion.')
    target = ((result.get('data') or {}).get('repository') or {}).get('discussion')
    if not target:
        raise ValueError('Configured Discussion is missing; refusing to create a duplicate.')
    return target


def publish(api, repository, config, dry_run=False):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository):
        raise ValueError('Use a repository in owner/name form.')
    number = config['discussion_number']
    if type(number) is not int or number < 1 or not config['labels']:
        raise ValueError('A positive Discussion number and bug labels are required.')
    body = render(api, repository, {label.casefold() for label in config['labels']})
    if dry_run:
        print(body)
        return False
    target = discussion(api, repository, number)
    if target['title'] != config['title']:
        raise ValueError('Discussion title differs from configuration; refusing to edit another post.')
    if target['body'] == body:
        print('Known-issues Discussion already up to date: ' + target['url'])
        return False
    # The explicitly configured editing token updates the original post only.
    # Never fall back to publishing a Discussion or snapshot comment.
    result = api.request('POST', 'graphql', {'query': UPDATE,
        'variables': {'id': target['id'], 'body': body}})
    if result.get('errors'):
        errors = '; '.join(f'{error.get("type", "GraphQL")}: {error.get("message", "Unknown error")}'
                           for error in result['errors'])
        raise RuntimeError('GitHub rejected the known-issues Discussion update: ' + errors)
    if not ((result.get('data') or {}).get('updateDiscussion') or {}).get('discussion'):
        raise RuntimeError('GitHub could not update the known-issues Discussion.')
    print('Known-issues Discussion updated: ' + result['data']['updateDiscussion']['discussion']['url'])
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dry-run', action='store_true', help='Print a read-only preview; never edit Discussions.')
    parser.add_argument('--repository', default=os.environ.get('GITHUB_REPOSITORY'))
    args = parser.parse_args()
    if not args.repository:
        parser.error('Set GITHUB_REPOSITORY or use --repository owner/name.')
    token = os.environ.get('KNOWN_ISSUES_TOKEN')
    if not token and not args.dry_run:
        parser.error('Publishing requires the KNOWN_ISSUES_TOKEN repository secret; no snapshot was posted.')
    config = json.loads((ROOT / '.github/known-issues.json').read_text(encoding='utf-8'))
    publish(GitHub(token), args.repository, config, args.dry_run)


if __name__ == '__main__':
    main()
