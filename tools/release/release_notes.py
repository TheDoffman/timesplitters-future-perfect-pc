"""Require reviewed player-facing notes before a game or launcher publication."""
from pathlib import Path
import re


def validate_release_notes(text):
    if len(text.encode('utf-8')) > 64 * 1024:
        raise ValueError('Reviewed release notes must be at most 64 KiB.')
    if '<!-- tsfp-release ' in text or '<!-- launcher-release-source:' in text:
        raise ValueError('Maintainer notes must not override channel metadata or source metadata.')
    if 'release-notes:template' in text or re.search(r'\b(?:TODO|TBD)\b', text):
        raise ValueError('Replace the release notes template and placeholders before publishing.')
    visible = re.sub(r'<!--.*?-->', '', text, flags=re.S)
    visible = re.sub(r'^```[^\n]*\n.*?^```[^\n]*$', '', visible, flags=re.M | re.S)
    if not re.search(r'^## What changed\s*$', visible, re.M):
        raise ValueError('Reviewed release notes need a "## What changed" section.')
    changes = re.split(r'^## What changed\s*$', visible, maxsplit=1, flags=re.M)[1]
    changes = re.split(r'^## ', changes, maxsplit=1, flags=re.M)[0]
    entries = re.findall(r'^### [^\n]+\n(.*?)(?=^### |\Z)', changes, re.M | re.S)
    if not entries:
        raise ValueError('Explain each release change under a ### heading with **Before:** and **Now:** paragraphs; PR titles alone are not release notes.')
    for entry in entries:
        for label in ('Before', 'Now'):
            match = re.search(r'^\*\*'+label+r':\*\*\s*(.*?)(?=^\*\*[^\n]+:\*\*|\Z)', entry, re.M | re.S)
            if not match or not match[1].strip():
                raise ValueError('Every release change needs nonempty **Before:** and **Now:** explanations.')
    return text.strip()


def read_release_notes(root, kind, tag, override=None):
    if kind not in ('game', 'launcher') or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*', tag):
        raise ValueError('Invalid release notes kind or tag.')
    path = Path(override) if override else Path(root)/'docs/release/notes'/(tag+'.md')
    if not path.is_file():
        raise ValueError(f'Write and review player-facing {kind} release notes at {path} before publishing. See docs/release/release-notes.md.')
    if path.stat().st_size > 64 * 1024:
        raise ValueError('Reviewed release notes must be at most 64 KiB.')
    return validate_release_notes(path.read_text(encoding='utf-8-sig'))
