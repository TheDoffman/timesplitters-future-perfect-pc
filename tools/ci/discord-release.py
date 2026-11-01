"""Announce a published game release through a channel-specific Discord webhook."""

import importlib.util
import json
import os
from pathlib import Path
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

REPOSITORY = 'TheDoffman/timesplitters-future-perfect-pc'
SPEC = importlib.util.spec_from_file_location(
    'discord_release_channels', Path(__file__).resolve().parents[1] / 'release/release_channels.py')
CHANNELS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHANNELS)
WEBHOOK = re.compile(r'https://discord\.com/api/webhooks/[0-9]+/[A-Za-z0-9._-]+\Z')


def announcement(event):
    """Return (route, payload), or None for drafts and non-game releases."""
    if event.get('action') != 'published':
        return None
    if event.get('repository', {}).get('full_name') != REPOSITORY:
        raise ValueError('Unexpected release repository.')
    release = event['release']
    if release.get('draft'):
        return None
    tag = release.get('tag_name', '')
    try:
        channel = CHANNELS.identity(tag)
    except ValueError:
        return None  # Launcher/texture releases have independent tag names.
    preview = channel['channel'] != 'stable'
    if bool(release.get('prerelease')) != preview:
        raise ValueError('Game tag and GitHub prerelease status disagree; no announcement sent.')
    route = 'preview' if preview else 'stable'
    url = f'https://github.com/{REPOSITORY}/releases/tag/' + urllib.parse.quote(tag, safe='')
    # Hide publisher metadata comments; keep public notes bounded to one card.
    notes = re.sub(r'<!--.*?-->', '', release.get('body') or '', flags=re.S).strip()
    if len(notes) > 1800:
        notes = notes[:1797].rstrip() + '…'
    instructions = ('Opt-in test build. In the launcher, select **Preview** and the '
                    f'**{channel["name"]}** testing track. Review compatibility and known issues first.'
                    if preview else
                    'In the launcher, keep **Stable** selected and choose **Check updates**.')
    description = instructions + ('\n\n' + notes if notes else '')
    description += f'\n\n[View release & downloads]({url})'
    title = f'TimeSplitters: Future Perfect Recomp · {tag}'
    return route, {
        'username': 'TimeSplitters Releases',
        'allowed_mentions': {'parse': []},
        'embeds': [{
            'title': title[:256], 'url': url,
            'description': description,
            'color': 0xE5A543 if preview else 0x5865F2,
            'footer': {'text': f'Game release · {channel["name"]} · Experimental fan project'},
        }],
    }


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ValueError('Discord webhook redirects are not permitted.')


def test_announcement(event):
    """A manual connection test uses the same delivery path without a release."""
    route = event.get('inputs', {}).get('channel', 'stable')
    if route not in ('stable', 'preview'):
        raise ValueError('Test channel must be stable or preview.')
    label = 'Stable' if route == 'stable' else 'Preview'
    return route, {
        'username': 'TimeSplitters Releases',
        'allowed_mentions': {'parse': []},
        'embeds': [{
            'title': f'TEST · {label} game release notifications',
            'description': ('This is a connection test for TimeSplitters: Future Perfect Recomp.\n\n'
                            'If you can see this card, GitHub Actions can post to this channel.\n'
                            '**No game release was created or published.**'),
            'color': 0xE5A543 if route == 'preview' else 0x5865F2,
            'footer': {'text': 'Manual connection test · Not a release announcement'},
        }],
    }


def send(webhook, payload):
    if not WEBHOOK.fullmatch(webhook):
        raise ValueError('Missing or invalid Discord webhook secret for the selected release channel.')
    request = urllib.request.Request(
        webhook + '?wait=true', data=json.dumps(payload).encode('utf-8'),
        headers={'Content-Type': 'application/json', 'User-Agent': 'TSFP-release-notifications/1'},
        method='POST')
    try:
        with urllib.request.build_opener(NoRedirect()).open(request, timeout=30) as response:
            if response.status != 200:
                raise ValueError('Discord did not confirm the announcement.')
    except urllib.error.HTTPError as error:
        raise ValueError(f'Discord rejected the announcement (HTTP {error.code}).') from None
    except (urllib.error.URLError, TimeoutError, OSError):
        # Never log request/response bodies or exception text containing the URL.
        # No automatic retry: a timeout may occur after Discord accepted a post.
        raise ValueError('Discord delivery was not confirmed; inspect the channel before rerunning.') from None


def main():
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text(encoding='utf-8'))
    testing = os.environ.get('GITHUB_EVENT_NAME') == 'workflow_dispatch'
    result = test_announcement(event) if testing else announcement(event)
    if result is None:
        print('No published game release to announce.')
        return
    route, payload = result
    secret = f'DISCORD_{route.upper()}_RELEASE_WEBHOOK'
    send(os.environ.get(secret, ''), payload)
    kind = 'connection test' if testing else 'game release announcement'
    print(f'Published {kind} to the {route} channel.')


if __name__ == '__main__':
    try:
        main()
    except ValueError as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
