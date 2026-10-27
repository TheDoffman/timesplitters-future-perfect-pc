"""Offline merge-comment tests; no credentials or GitHub writes."""


import contextlib
import importlib.util
import io
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('issue_resolution', Path(__file__).resolve().parents[2] / 'tools/ci/issue-resolution.py')
resolution = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(resolution)
REPO = 'owner/game'
BODY = '## Change\nVisible ghosts.\n\n## Cause and fix\nShared depth and fresh texture snapshots restore the ghost.\n\n## Validation\nFour GPU checks passed. Mac pending.\n\n## Mac port backlog\nQueued.'


class FakeGitHub:
    def __init__(self):
        self.pull = {'merged': True, 'baseRefName': 'main', 'body': BODY,
                     'url': 'https://github.com/owner/game/pull/127'}
        self.links = [[(111, REPO)]]
        self.comments = {}
        self.requests = []
        self.error = False

    def request(self, method, path, payload=None):
        self.requests.append((method, path, payload))
        if path == 'graphql':
            if self.error:
                return {'errors': [{'message': 'Access denied'}]}
            page = int(payload['variables']['after'] or 0)
            connection = {'nodes': [{'number': number, 'repository': {'nameWithOwner': repo}}
                                    for number, repo in self.links[page]],
                          'pageInfo': {'hasNextPage': page + 1 < len(self.links), 'endCursor': str(page + 1)}}
            return {'data': {'repository': {'defaultBranchRef': {'name': 'main'},
                    'pullRequest': dict(self.pull, closingIssuesReferences=connection)}}}
        if method == 'GET':
            number = int(path.split('/issues/')[1].split('/')[0])
            page = int(path.split('page=')[-1])
            return self.comments.get(number, [])[(page-1)*100:page*100]
        if method == 'POST':
            number = int(path.split('/issues/')[1].split('/')[0])
            comment = {'id': number*1000+len(self.comments.get(number, [])),
                       'body': payload['body'], 'user': {'login': 'github-actions[bot]', 'type': 'Bot'}}
            self.comments.setdefault(number, []).append(comment)
            return comment
        if method == 'PATCH':
            number = int(path.split('/')[-1])
            for comments in self.comments.values():
                for comment in comments:
                    if comment['id'] == number:
                        comment['body'] = payload['body']
                        return comment
        raise AssertionError((method, path, payload))

    def writes(self):
        return [r for r in self.requests if r[0] in ('POST', 'PATCH') and r[1] != 'graphql']


class ResolutionTests(unittest.TestCase):
    def run_publish(self, api, dry_run=False):
        with contextlib.redirect_stdout(io.StringIO()):
            return resolution.publish(api, REPO, 127, dry_run)

    def test_posts_explanation_validation_and_pr_link(self):
        api = FakeGitHub()
        self.assertEqual(self.run_publish(api), [111])
        body = api.comments[111][0]['body']
        self.assertIn('https://github.com/owner/game/pull/127', body)
        self.assertIn('Shared depth and fresh texture snapshots', body)
        self.assertIn('Four GPU checks passed. Mac pending.', body)
        self.assertNotIn('## Mac port backlog', body)
        self.assertEqual(len(api.writes()), 1)

    def test_unmerged_closed_pr_does_not_comment(self):
        api = FakeGitHub(); api.pull['merged'] = False
        self.assertEqual(self.run_publish(api), [])
        self.assertEqual(api.writes(), [])

    def test_nondefault_branch_does_not_comment(self):
        api = FakeGitHub(); api.pull['baseRefName'] = 'development'
        self.assertEqual(self.run_publish(api), [])
        self.assertEqual(api.writes(), [])

    def test_no_closing_links_ignores_incidental_mentions(self):
        api = FakeGitHub(); api.links = [[]]; api.pull['body'] = 'Related to #555.'
        self.assertEqual(self.run_publish(api), [])
        self.assertEqual(api.writes(), [])

    def test_multiple_paginated_links_deduplicate_and_exclude_other_repos(self):
        api = FakeGitHub(); api.links = [[(111, REPO), (222, 'another/repo')], [(111, REPO), (128, 'OWNER/GAME')]]
        self.assertEqual(self.run_publish(api), [111, 128])
        self.assertEqual(sorted(api.comments), [111, 128])

    def test_rerun_does_not_duplicate(self):
        api = FakeGitHub(); self.run_publish(api); self.run_publish(api)
        self.assertEqual(len(api.writes()), 1)

    def test_corrected_description_updates_own_comment(self):
        api = FakeGitHub(); self.run_publish(api)
        api.pull['body'] = BODY.replace('Four GPU', 'Five GPU')
        self.run_publish(api)
        self.assertEqual(len(api.comments[111]), 1)
        self.assertEqual(api.writes()[-1][0], 'PATCH')
        self.assertIn('Five GPU', api.comments[111][0]['body'])

    def test_human_comment_with_marker_is_preserved(self):
        api = FakeGitHub(); human = {'id': 5, 'body': '<!-- tsfp-resolution:pr-127 -->\nPersonal notes.',
                                   'user': {'login': 'maintainer', 'type': 'User'}}
        api.comments[111] = [human.copy()]
        self.run_publish(api)
        self.assertEqual(api.comments[111][0], human)
        self.assertEqual(len(api.comments[111]), 2)

    def test_existing_bot_comment_on_second_page_is_found(self):
        api = FakeGitHub()
        api.comments[111] = [{'id': n, 'body': 'Discussion', 'user': {'login': 'human', 'type': 'User'}} for n in range(100)]
        self.run_publish(api); self.run_publish(api)
        self.assertEqual(len(api.comments[111]), 101)
        self.assertEqual(len(api.writes()), 1)

    def test_missing_or_placeholder_explanation_writes_nothing(self):
        for body in ('## Change\nA fix.', '## Cause and fix\n', '## Cause and fix\n' + resolution.PLACEHOLDER):
            with self.subTest(body=body):
                api = FakeGitHub(); api.pull['body'] = body
                with self.assertRaisesRegex(ValueError, 'Cause and fix'):
                    self.run_publish(api)
                self.assertEqual(api.writes(), [])

    def test_dry_run_never_posts(self):
        api = FakeGitHub()
        self.assertEqual(self.run_publish(api, True), [111])
        self.assertEqual(api.writes(), [])

    def test_graphql_errors_do_not_post_partial_comments(self):
        api = FakeGitHub(); api.error = True
        with self.assertRaises(RuntimeError): self.run_publish(api)
        self.assertEqual(api.writes(), [])

    def test_code_fence_headings_and_commands_are_only_text(self):
        body = '## Cause and fix\nBefore.\n```sh\n## Validation\n$(do-not-execute)\n```\nAfter.\n## Validation\nPassed.'
        extracted = resolution.section(body, 'Cause and fix')
        self.assertIn('$(do-not-execute)', extracted)
        self.assertTrue(extracted.endswith('After.'))
        self.assertEqual(resolution.section(body, 'Validation'), 'Passed.')

    def test_long_explanation_fails_before_writes(self):
        api = FakeGitHub(); api.pull['body'] = '## Cause and fix\n' + 'x'*13000
        with self.assertRaises(ValueError): self.run_publish(api)
        self.assertEqual(api.writes(), [])

    def test_repository_and_number_validation(self):
        for repo, number in [('owner/repo/else', 1), (REPO, 0)]:
            api = FakeGitHub()
            with self.assertRaises(ValueError): resolution.publish(api, repo, number)
            self.assertEqual(api.requests, [])


if __name__ == '__main__':
    unittest.main()
