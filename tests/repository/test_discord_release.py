"""Offline release routing and webhook boundary regressions."""

import importlib.util
from pathlib import Path
import unittest
import json
import os
import tempfile
from unittest.mock import Mock, patch
import urllib.error

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('discord_release', ROOT / 'tools/ci/discord-release.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def event(tag='v0.4.1', preview=False, **changes):
    release = dict(tag_name=tag, prerelease=preview, draft=False, body='Improved controls.')
    release.update(changes)
    return dict(action='published', repository={'full_name': MODULE.REPOSITORY}, release=release)


class DiscordReleaseTests(unittest.TestCase):
    def test_manual_cards_are_labeled_and_do_not_impersonate_a_release(self):
        for selected in ('stable', 'preview'):
            route, payload = MODULE.test_announcement({'inputs': {'channel': selected}})
            self.assertEqual(route, selected)
            self.assertTrue(payload['embeds'][0]['title'].startswith('TEST'))
            self.assertIn('No game release', payload['embeds'][0]['description'])
            self.assertEqual(payload['allowed_mentions'], {'parse': []})
            self.assertNotIn('url', payload['embeds'][0])
        with self.assertRaises(ValueError):
            MODULE.test_announcement({'inputs': {'channel': 'other'}})

    def test_manual_dispatch_follows_release_destinations(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'event.json'
            for selected in ('stable', 'preview'):
                path.write_text(json.dumps({'inputs': {'channel': selected}}), encoding='utf-8')
                env = {'GITHUB_EVENT_PATH': str(path), 'GITHUB_EVENT_NAME': 'workflow_dispatch',
                       'DISCORD_STABLE_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/123/stable',
                       'DISCORD_PREVIEW_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/456/preview'}
                with patch.dict(os.environ, env, clear=True), patch.object(MODULE, 'send') as send:
                    MODULE.main()
                    expected = ['stable']
                    self.assertEqual([call.args[0].rsplit('/', 1)[1] for call in send.call_args_list], expected)

    def test_published_release_delivery_reaches_the_correct_channels(self):
        secrets = {'DISCORD_STABLE_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/123/stable',
                   'DISCORD_PREVIEW_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/456/preview'}
        for tag, preview, expected in [('v0.4.1', False, ['stable']),
                                       ('v0.5.0-beta.1', True, ['stable']),
                                       ('v0.5.0-story-coop.2', True, ['stable'])]:
            with patch.object(MODULE, 'send') as send:
                route, payload = MODULE.announcement(event(tag, preview))
                MODULE.deliver(route, payload, secrets)
                self.assertEqual([call.args[0].rsplit('/', 1)[1] for call in send.call_args_list], expected)
                self.assertTrue(all(call.args[1] == payload for call in send.call_args_list))

    def test_preview_secret_is_not_needed_for_either_release_type(self):
        secrets = {'DISCORD_STABLE_RELEASE_WEBHOOK': 'https://discord.com/api/webhooks/123/stable'}
        with patch.object(MODULE, 'send') as send:
            MODULE.deliver('preview', {}, secrets)
            send.assert_called_once_with(secrets['DISCORD_STABLE_RELEASE_WEBHOOK'], {})
            send.reset_mock()
            MODULE.deliver('stable', {}, secrets)
            send.assert_called_once()

    def test_stable_goes_to_announcements_with_exact_game_link(self):
        route, payload = MODULE.announcement(event())
        self.assertEqual(route, 'stable')
        embed = payload['embeds'][0]
        self.assertTrue(embed['url'].endswith('/releases/tag/v0.4.1'))
        self.assertNotIn('/latest', embed['description'])
        self.assertIn('**Stable**', embed['description'])

    def test_beta_and_feature_previews_keep_preview_labels(self):
        for tag, track in [('v0.5.0-beta.1', 'Beta'), ('v0.5.0-story-coop.2', 'Story Coop Preview')]:
            with self.subTest(tag=tag):
                route, payload = MODULE.announcement(event(tag, True))
                self.assertEqual(route, 'preview')
                self.assertIn(track, payload['embeds'][0]['description'])
                self.assertIn('Opt-in test build', payload['embeds'][0]['description'])

    def test_ignore_launcher_texture_and_draft_releases(self):
        for tag in ['launcher-v1.4.2', 'textures-v1.0.0', 'not-a-version']:
            self.assertIsNone(MODULE.announcement(event(tag)))
        self.assertIsNone(MODULE.announcement(event(draft=True)))

    def test_only_published_event_sends(self):
        for action in ['created', 'edited', 'deleted', 'unpublished']:
            data = event()
            data['action'] = action
            self.assertIsNone(MODULE.announcement(data))

    def test_mismatched_release_status_fails_closed(self):
        for tag, preview in [('v0.5.0', True), ('v0.5.0-beta.1', False)]:
            with self.assertRaisesRegex(ValueError, 'disagree'):
                MODULE.announcement(event(tag, preview))

    def test_wrong_repository_fails_closed(self):
        data = event()
        data['repository']['full_name'] = 'someone/else'
        with self.assertRaisesRegex(ValueError, 'repository'):
            MODULE.announcement(data)

    def test_notes_are_bounded_and_metadata_is_hidden(self):
        _, payload = MODULE.announcement(event(body='<!-- tsfp-release {"secret":false} -->\n' + 'x' * 9000))
        embed = payload['embeds'][0]
        self.assertLess(len(embed['description']), 4096)
        self.assertNotIn('tsfp-release', embed['description'])
        self.assertIn('View release & downloads', embed['description'])

    def test_release_notes_cannot_ping_members(self):
        _, payload = MODULE.announcement(event(body='@everyone <@123> <@&456>'))
        self.assertEqual(payload['allowed_mentions'], {'parse': []})
        self.assertNotIn('content', payload)

    def test_invalid_webhook_is_rejected_without_network_or_secret_output(self):
        for url in ['', 'http://discord.com/api/webhooks/1/token',
                    'https://evil.test/api/webhooks/1/token',
                    'https://discord.com@evil.test/api/webhooks/1/token',
                    'https://discord.com/api/webhooks/1/token?redirect=evil']:
            with patch.object(MODULE.urllib.request, 'build_opener') as opener:
                with self.assertRaises(ValueError) as error:
                    MODULE.send(url, {})
                self.assertNotIn('token', str(error.exception))
                opener.assert_not_called()

    def test_delivery_is_confirmed_and_timeout_does_not_retry(self):
        url = 'https://discord.com/api/webhooks/123/private-token'
        response = Mock(status=200)
        response.__enter__ = Mock(return_value=response)
        response.__exit__ = Mock(return_value=False)
        opener = Mock()
        opener.open.return_value = response
        with patch.object(MODULE.urllib.request, 'build_opener', return_value=opener):
            MODULE.send(url, {'allowed_mentions': {'parse': []}})
        request = opener.open.call_args.args[0]
        self.assertEqual(request.full_url, url + '?wait=true')
        opener.open.side_effect = urllib.error.URLError(url)
        opener.open.reset_mock()
        with patch.object(MODULE.urllib.request, 'build_opener', return_value=opener):
            with self.assertRaises(ValueError) as error:
                MODULE.send(url, {})
        self.assertNotIn('private-token', str(error.exception))
        self.assertEqual(opener.open.call_count, 1)

    def test_redirect_never_forwards_webhook(self):
        with self.assertRaisesRegex(ValueError, 'redirect'):
            MODULE.NoRedirect().redirect_request(None, None, 302, '', {}, 'https://evil.test')


if __name__ == '__main__':
    unittest.main()
