# Discord game release announcements

`.github/workflows/discord-release.yml` listens for published releases. It uses
`tools/ci/discord-release.py` and the shared `release_channels.identity` policy:

| Published game release | Discord destination | Actions repository secret |
| --- | --- | --- |
| Stable, e.g. `v0.4.1` | `#announcements` | `DISCORD_STABLE_RELEASE_WEBHOOK` |
| Beta or named preview, e.g. `v0.5.0-beta.1` | `#playtesting` | `DISCORD_PREVIEW_RELEASE_WEBHOOK` |

Launcher and texture tags, drafts, and release edits do not send announcements.
The GitHub prerelease flag must agree with the game tag; a mismatch fails without
posting. Cards include bounded public release notes, the game version and track,
launcher update instructions, and a link to that exact release. `/releases/latest`
belongs to the launcher and is deliberately not used for game downloads.
Notifications do not ping users or roles.

## One-time connection

1. In Discord, open **Server Settings → Integrations → Webhooks**. Create a
   webhook named **TimeSplitters Releases** for `#announcements`, then a second
   webhook for `#playtesting`. Confirm each selected channel before copying its URL.
2. In this GitHub repository, open **Settings → Secrets and variables → Actions**.
   Save the two webhook URLs under the exact repository-secret names above.
   Never commit or paste the URLs into issues, PRs, documentation, or chat.
3. Merge the workflow through a reviewed PR. Include it in future game release
   branches/tags, including named preview branches. Notification code is checked
   out from the default branch; no release notes are executed as code.
4. To verify delivery without publishing a release, open **Actions → Discord game
   release announcements → Run workflow**, keep branch **main**, and choose
   **stable** (announcements) or **preview** (playtesting). Each run sends one
   clearly labeled TEST card through the same sender and selected channel secret.
   The selected channel must have its webhook secret configured; the other secret
   is not required for this test. Confirm both a successful job and the visible
   card in Discord. This tests delivery, not the GitHub release event itself.
5. After the next real release is published, inspect **Actions → Discord game
   release announcements** and confirm its card reached the correct channel.
   Do not publish a fake release just to test the integration.

The current game publisher uses authenticated GitHub CLI publication. If game
publication is later moved into Actions using `GITHUB_TOKEN`, GitHub suppresses
follow-on release workflows: explicitly invoke the notification as part of that
publisher or dispatch a dedicated notification workflow instead. Do not assume
the release event will run in that case.

Missing/invalid selected-channel secrets fail the notification job. Webhook
secrets stay in the notification step, never in PR checks, command arguments, or
logs. Discord redirects are refused. A post waits for confirmation and is not
automatically retried: timeouts can occur after delivery. Inspect the Discord
channel before rerunning a failed job; a manual rerun can duplicate a post.
Notification failure does not undo or modify the published game release.

## Offline validation

```sh
python -m unittest discover -s tests/repository -p test_discord_release.py -v
```

The workflow runs these regressions for relevant PRs and main pushes as well as
release events. Tests cover stable/preview routing, unrelated tags, drafts,
mismatched flags, bounded metadata-free notes, mention suppression, webhook URL
validation, redirects, and ambiguous delivery. They use mocks and never send a
Discord message. The script uses Python's standard library on either host.
