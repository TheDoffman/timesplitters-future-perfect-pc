"""Copy a merged PR's reviewed explanation to the issues it closes."""

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
import urllib.request


QUERY = """
query($owner: String!, $name: String!, $number: Int!, $after: String) {
  repository(owner: $owner, name: $name) {
    defaultBranchRef { name }
    pullRequest(number: $number) {
      merged baseRefName url body
      closingIssuesReferences(first: 100, after: $after) {
        nodes { number repository { nameWithOwner } }
        pageInfo { hasNextPage endCursor }
      }
    }
  }
}
"""
PLACEHOLDER = ("Explain what caused the problem and how this change fixes it. Describe the "
               "behavior before and after, including any remaining limits. For enhancements, "
               "explain how the new behavior works.")


class GitHub:
    def __init__(self, token):
        self.token = token

    def request(self, method, path, payload=None):
        headers = {'Authorization': 'Bearer ' + self.token,
                   'Accept': 'application/vnd.github+json',
                   'X-GitHub-Api-Version': '2022-11-28',
                   'User-Agent': 'TSFP-Issue-Resolution'}
        data = None if payload is None else json.dumps(payload).encode('utf-8')
        if data is not None:
            headers['Content-Type'] = 'application/json'
        request = urllib.request.Request('https://api.github.com/' + path,
                                         data=data, headers=headers, method=method)
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)


def section(body, wanted):
    """Read an H2 section without treating fenced code as Markdown headings."""
    lines, active, fence = [], False, None
    for line in (body or '').splitlines():
        stripped = line.lstrip()
        opening = re.match(r'(`{3,}|~{3,})', stripped)
        if fence:
            if active:
                lines.append(line)
            if re.fullmatch(re.escape(fence[0]) + '{' + str(len(fence)) + r',}\s*', stripped):
                fence = None
            continue
        if opening:
            fence = opening.group(1)
        heading = re.match(r'^##[ \t]+(.+?)(?:[ \t]+#+)?[ \t]*$', line)
        if heading:
            if active:
                break
            active = heading.group(1).strip().casefold() == wanted.casefold()
        elif active:
            lines.append(line)
    return '\n'.join(lines).strip()


def snapshot(api, repository, number):
    owner, name = repository.split('/')
    after, seen, issues, pull, default = None, set(), set(), None, None
    for _ in range(100):
        result = api.request('POST', 'graphql', {'query': QUERY, 'variables': {
            'owner': owner, 'name': name, 'number': number, 'after': after}})
        if result.get('errors'):
            raise RuntimeError('GitHub could not read the PR closing-issue links.')
        repo = (result.get('data') or {}).get('repository')
        if not repo or not repo.get('pullRequest') or not repo.get('defaultBranchRef'):
            raise ValueError('The pull request or repository default branch is unavailable.')
        if pull is None:
            pull, default = repo['pullRequest'], repo['defaultBranchRef']['name']
        connection = repo['pullRequest']['closingIssuesReferences']
        for issue in connection['nodes']:
            if issue['repository']['nameWithOwner'].casefold() == repository.casefold():
                issues.add(int(issue['number']))
        page = connection['pageInfo']
        if not page['hasNextPage']:
            return pull, default, sorted(issues)
        after = page['endCursor']
        if not after or after in seen:
            raise ValueError('Closing-issue pagination did not advance.')
        seen.add(after)
    raise ValueError('Too many closing-issue pages; no comments were written.')


def comment_body(pull, number, default):
    explanation = section(pull['body'], 'Cause and fix')
    if not explanation or ' '.join(explanation.split()).startswith(PLACEHOLDER):
        raise ValueError('Fill in the PR "Cause and fix" section, then rerun this workflow.')
    validation = section(pull['body'], 'Validation')
    body = (f'<!-- tsfp-resolution:pr-{number} -->\n'
            f'Resolved in [PR #{number}]({pull["url"]}), merged into `{default}`.\n\n'
            f'### How it was fixed\n\n{explanation}')
    if validation and not validation.startswith('Record relevant tests and host results.'):
        body += '\n\n### Validation and remaining limits\n\n' + validation
    if len(body) > 12000:
        raise ValueError('Keep the PR cause/fix and validation explanation under 12,000 characters.')
    return body + '\n'


def existing_comment(api, repository, issue, marker):
    for page in range(1, 101):
        comments = api.request('GET', f'repos/{repository}/issues/{issue}/comments?per_page=100&page={page}')
        for comment in comments:
            user = comment.get('user') or {}
            if (user.get('login') == 'github-actions[bot]' and user.get('type') == 'Bot'
                    and (comment.get('body') or '').startswith(marker + '\n')):
                return comment
        if len(comments) < 100:
            return None
    raise ValueError('Too many comment pages; refusing to risk a duplicate.')


def publish(api, repository, number, dry_run=False):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository) or number < 1:
        raise ValueError('A repository owner/name and positive PR number are required.')
    pull, default, issues = snapshot(api, repository, number)
    if not pull['merged'] or pull['baseRefName'] != default:
        print('No comments: PR has not merged into the default branch.')
        return []
    if not issues:
        print('No comments: no closing issues in this repository.')
        return []
    body = comment_body(pull, number, default)
    if dry_run:
        print(json.dumps({'issues': issues, 'body': body}, indent=2))
        return issues
    marker = f'<!-- tsfp-resolution:pr-{number} -->'
    for issue in issues:
        previous = existing_comment(api, repository, issue, marker)
        if previous and previous['body'] == body:
            print(f'Issue #{issue}: explanation already present.')
            continue
        path = (f'repos/{repository}/issues/comments/{int(previous["id"])}' if previous
                else f'repos/{repository}/issues/{issue}/comments')
        api.request('PATCH' if previous else 'POST', path, {'body': body})
        print(f'Issue #{issue}: explanation {"updated" if previous else "posted"}.')
    return issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--pr', type=int, help='Read-only preview of an already merged PR (requires --dry-run).')
    args = parser.parse_args()
    if args.pr and not args.dry_run:
        parser.error('--pr is available only with --dry-run; publishing requires a merge event.')
    repository = os.environ['GITHUB_REPOSITORY']
    number = args.pr
    if number is None:
        event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
        if (os.environ.get('GITHUB_EVENT_NAME') != 'pull_request_target'
                or event.get('action') != 'closed'
                or not event.get('pull_request', {}).get('merged')):
            print('No comments: this is not a PR merge event.')
            return
        if event['repository']['full_name'] != repository:
            raise ValueError('Merge event repository does not match the workflow repository.')
        number = int(event['pull_request']['number'])
    publish(GitHub(os.environ['GITHUB_TOKEN']), repository, number, args.dry_run)


if __name__ == '__main__':
    main()
