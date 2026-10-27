"""Bounded repository navigation and enforceable context maintenance (no game data).

Windows: .venv/Scripts/python.exe tools/repo-context.py find FlipGate
Use refresh after staging intended patches; check --base REV enforces guide updates.
"""
import argparse
import fnmatch
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CONTEXT = 'docs/agent-context/'
INDEX = CONTEXT + 'patch-index.json'


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args]).decode('utf-8')


def tracked(root):
    return git(root, 'ls-files', '-z').rstrip('\0').split('\0')


def safe_file(root, name):
    """Context is an explicit public-doc allowlist, never a general file reader."""
    relative = Path(name)
    if relative.is_absolute() or re.match(r'^[A-Za-z]:', name) or '..' in relative.parts or '\\' in name:
        raise ValueError(f'Invalid context path: {name}')
    target = root / relative
    if not target.resolve().is_relative_to(root.resolve()):
        raise ValueError(f'Redirected context path: {name}')
    current = root
    for part in relative.parts:
        current /= part
        if current.is_symlink() or current.is_junction():
            raise ValueError(f'Redirected context path: {name}')
    if target.stat().st_size > 65536:
        raise ValueError(f'Context file too large: {name}')
    return target


def manifest(root):
    data = json.loads(safe_file(root, CONTEXT + 'manifest.json').read_text(encoding='utf-8'))
    if data['version'] != 1 or data['overview'] != CONTEXT + 'README.md':
        raise ValueError('Unsupported context manifest')
    ids = set()
    for topic in data['topics']:
        if not re.fullmatch(r'[a-z-]+', topic['id']) or topic['id'] in ids:
            raise ValueError('Invalid or duplicate context topic')
        ids.add(topic['id'])
        if topic['file'] != CONTEXT + topic['id'] + '.md':
            raise ValueError('Only named agent-context guides can be included')
        safe_file(root, topic['file'])
    return data


def excerpt(root, name):
    text = safe_file(root, name).read_text(encoding='utf-8')
    match = re.search(r'<!-- packet:start -->\s*([\s\S]*?)\s*<!-- packet:end -->', text)
    if not match or len(match[1].encode('utf-8')) > 3000:
        raise ValueError(f'Missing/oversized advisory excerpt: {name}')
    return match[1]


def packet(root, query):
    data = manifest(root)
    scores = [(sum(bool(re.search(r'(?<!\w)' + re.escape(k) + r'(?:s|ing)?(?!\w)', query, re.I))
                   for k in topic['keywords']), -i, topic)
              for i, topic in enumerate(data['topics'])]
    parts = [data['overview']] + [t['file'] for score, _, t in sorted(scores, reverse=True)[:2] if score]
    text = '\n\n'.join('Reference: ' + name + '\n' + excerpt(root, name) for name in parts)
    # Keep the same cap used by the machine router; UTF-8 truncation is explicit.
    if len(text.encode('utf-8')) > 6500:
        text = text.encode('utf-8')[:6400].decode('utf-8', errors='ignore') + '\n[Context excerpt truncated]'
    return text


def patch_index(root):
    spec = importlib.util.spec_from_file_location('context_patch_loader', root / 'tools/apply-patches.py')
    loader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loader)
    paths = [root / name for name in tracked(root)
             if name.startswith('patches/') and name.endswith('.patch')
             and 'legacy' not in Path(name).parts]
    records = loader.patch_records(paths)
    locations = {p.name: p.relative_to(root).as_posix() for p in paths}
    # Existing Windows worktrees can retain CRLF despite the LF attribute.
    # Normalize loader text for fresh Linux/macOS checkout reproducibility.
    loader_bytes = (root / 'tools/apply-patches.py').read_text(encoding='utf-8').encode('utf-8')
    return {'version': 1, 'loaderSha256': hashlib.sha256(loader_bytes).hexdigest(),
            'patches': [{'name': record['name'], 'path': locations[record['name']],
                         'sha256': record['sha256'],
                         'targets': sorted(set(re.findall(r'^\+\+\+ b/(.+)$', record['content'], re.M)))}
                        for record in records]}


def index_text(root):
    return json.dumps(patch_index(root), indent=2) + '\n'


def find(root, query, limit=20):
    """Literal query; newest effective patches first; do not scan ignored runtime/assets."""
    names = tracked(root)
    patches = patch_index(root)['patches']
    indexed = {p['path'] for p in patches}
    experiments = sorted(p.relative_to(root).as_posix() for p in (root / 'patches').rglob('*.patch')
                         if 'legacy' not in p.relative_to(root / 'patches').parts
                         if p.relative_to(root).as_posix() not in indexed)
    candidates = [(p, 'untracked experiment') for p in experiments]
    candidates += [(p['path'], 'active order ' + str(i + 1))
                   for i, p in reversed(list(enumerate(patches)))]
    candidates += [(n, 'tracked source') for n in names if n.startswith(('src/', 'tools/launcher/', 'tests/'))
                   and Path(n).suffix in ('.c', '.h', '.inc', '.def', '.cs', '.xaml', '.py')]
    if not query.strip():
        raise ValueError('Provide a nonempty literal symbol or path')
    needle, matches = query.casefold(), []
    for name, kind in candidates:
        path = root / name
        if path.is_symlink() or not path.resolve().is_relative_to(root.resolve()):
            continue
        # Patch targets/hunk anchors give useful context without full patch dumps.
        target = anchor = ''
        for line_number, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            if line.startswith('+++ b/'):
                target = line[6:]
            if line.startswith('@@'):
                anchor = line[:150]
            if needle not in line.casefold() and not (line_number == 1 and needle in name.casefold()):
                continue
            removed = name.endswith('.patch') and line.startswith('-') and not line.startswith('---')
            matches.append((removed, f'{name}:{line_number} [{kind}' + ('; REMOVED line' if removed else '')
                            + f'] {target}\n  {anchor}\n  {line[:200]}'))
    found = [text for _, text in sorted(matches, key=lambda item: item[0])[:limit]]
    return '\n'.join(found) + f'\n{len(matches)} matching lines; showing {len(found)}. Later patches may supersede earlier hunks.\n'


def missing_updates(data, changed):
    changed = set(changed)
    return sorted({rule['guide'] for rule in data['owners']
                   if any(fnmatch.fnmatchcase(name, pattern) for name in changed for pattern in rule['patterns'])
                   and rule['guide'] not in changed})


def changed_paths(root, base):
    # Includes tracked working-tree edits for local use; CI's tree is clean.
    names = set(git(root, 'diff', '--no-renames', '--name-only', '-z', base).rstrip('\0').split('\0'))
    # New guide/tool files must count before their first commit, but unrelated
    # untracked work (e.g. another task's experimental patch) is not this change.
    names.update(n for n in git(root, 'ls-files', '--others', '--exclude-standard', '-z').split('\0')
                 if n.startswith(CONTEXT))
    return names - {''}


def version_reference_only(root, base, name):
    """Exempt generated release data, never form structure or empty-list regressions."""
    if name != '.github/ISSUE_TEMPLATE/01-bug-report.yml':
        return False
    path = root / name
    if not path.is_file() or path.is_symlink():
        return False
    try:
        before = git(root, 'show', f'{base}:{name}')
    except subprocess.CalledProcessError:
        return False
    after = path.read_text(encoding='utf-8')
    version = r'        - `v[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?`\n'
    for kind in ('GAME', 'LAUNCHER'):
        pattern = re.compile(
            rf'(^        <!-- BEGIN AUTO {kind} VERSIONS -->\n)(.*?)'
            rf'(^        <!-- END AUTO {kind} VERSIONS -->$)', re.M | re.S)
        old, new = list(pattern.finditer(before)), list(pattern.finditer(after))
        if len(old) != 1 or len(new) != 1:
            return False
        for block in (old[0][2], new[0][2]):
            if not (re.fullmatch(r'(?:' + version + r'){1,30}', block)
                    or block == '        No published releases.\n'):
                return False
        if old[0][2] != new[0][2] and new[0][2] == '        No published releases.\n':
            return False
        before = pattern.sub(lambda m: m[1] + m[3], before)
        after = pattern.sub(lambda m: m[1] + m[3], after)
    return before == after


def check(root, base=None):
    data = manifest(root)
    for name in [data['overview']] + [t['file'] for t in data['topics']]:
        excerpt(root, name)
    allowed = {t['file'] for t in data['topics']}
    for rule in data['owners']:
        if rule['guide'] not in allowed or not rule['patterns']:
            raise ValueError('Invalid context ownership rule')
    if (root / INDEX).read_text(encoding='utf-8') != index_text(root):
        raise ValueError('Patch index is stale: stage intended patches, then run tools/repo-context.py refresh')
    # Check local Markdown file links, ignoring external links and anchors.
    for doc in (root / CONTEXT).glob('*.md'):
        for link in re.findall(r'\]\(([^)]+)\)', doc.read_text(encoding='utf-8')):
            target = link.split('#', 1)[0]
            if target and '://' not in target and not (doc.parent / target).exists():
                raise ValueError(f'{doc.name}: missing linked file {target}')
    if base:
        missing = missing_updates(data, [name for name in changed_paths(root, base)
                                         if not version_reference_only(root, base, name)])
        if missing:
            raise ValueError('Changed systems require guide updates: ' + ', '.join(missing))
    return 'Repository context, links and patch index are current.'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('refresh')
    checker = commands.add_parser('check')
    checker.add_argument('--base')
    checker.add_argument('--ci', action='store_true')
    search = commands.add_parser('find')
    search.add_argument('query')
    search.add_argument('--limit', type=int, default=20, choices=range(1, 41))
    p = commands.add_parser('packet')
    p.add_argument('query')
    args = parser.parse_args()
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    if args.command == 'refresh':
        (ROOT / INDEX).write_text(index_text(ROOT), encoding='utf-8', newline='\n')
        print('Refreshed Git-indexed active patches; untracked experiments excluded.')
    elif args.command == 'find':
        print(find(ROOT, args.query, args.limit), end='')
    elif args.command == 'packet':
        print(packet(ROOT, args.query))
    else:
        base = args.base
        if args.ci:
            event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
            base = event.get('pull_request', {}).get('base', {}).get('sha') or event.get('before')
            if base and set(base) != {'0'}:
                subprocess.run(['git', '-C', str(ROOT), 'fetch', '--no-tags', '--depth=1', 'origin', base], check=True)
            else:
                base = None
        print(check(ROOT, base))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
