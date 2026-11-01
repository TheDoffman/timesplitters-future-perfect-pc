# Discord game release announcements

`.github/workflows/discord-release.yml` listens for published releases. It uses
`tools/ci/discord-release.py` and the shared `release_channels.identity` policy:

| Published game release | Discord destination | Actions repository secret |
| --- | --- | --- |
| Stable, e.g. `v0.4.1` | `#release-updates` | `DISCORD_STABLE_RELEASE_WEBHOOK` |
| Beta or named preview, e.g. `v0.5.0-beta.1` | `#release-updates` | `DISCORD_STABLE_RELEASE_WEBHOOK` |

Launcher and texture tags, drafts, and release edits do not send announcements.
The GitHub prerelease flag must agree with the game tag; a mismatch fails without
posting. Cards include bounded public release notes, the game version and track,
launcher update instructions, and a link to that exact release. `/releases/latest`
belongs to the launcher and is deliberately not used for game downloads.
Real releases mention only their matching opt-in alert role, configured through
`DISCORD_STABLE_RELEASE_ROLE_ID` or `DISCORD_PREVIEW_RELEASE_ROLE_ID` in the workflow.
The current roles are **Stable Release Alerts** (`1566334829791027310`) and
**Preview Testing Alerts** (`1566334962834087997`). These are public Discord role
identifiers, not credentials. Release-note mentions cannot ping members, other
roles, or everyone. Manual connection tests never ping any role.
Both release types use the existing
release-updates webhook; `DISCORD_PREVIEW_RELEASE_WEBHOOK` is no longer used.

The automated feed is [#release-updates](https://discord.com/channels/1566295768489263136/1566483143748231228).
`#announcements` is reserved for posts from the project team. Both release webhooks
were moved to the new channel in Discord; the saved webhook URLs still work.
The legacy secret name `DISCORD_STABLE_RELEASE_WEBHOOK` remains the single sender
for Stable and Preview posts. New members see release-updates by default.

## One-time connection

1. In Discord, open **Server Settings → Integrations → Webhooks**. Create a
   webhook named **TimeSplitters Releases** for `#release-updates`. Confirm the
   selected channel before copying its URL. The existing webhook can be reused.
2. In this GitHub repository, open **Settings → Secrets and variables → Actions**.
   Save its URL as `DISCORD_STABLE_RELEASE_WEBHOOK` for both release types.
   Never commit or paste the URLs into issues, PRs, documentation, or chat.
3. Merge the workflow through a reviewed PR. Include it in future game release
   branches/tags, including named preview branches. Notification code is checked
   out from the default branch; no release notes are executed as code.
4. To verify delivery without publishing a release, open **Actions → Discord game
   release announcements → Run workflow**, keep branch **main**, and choose
   **stable** or **preview** to choose the TEST card's release label. Both post
   one card to release-updates using the same webhook. Confirm a successful job
   and the visible card in Discord. This tests delivery, not the GitHub release
   event itself.
5. After the next real release is published, inspect **Actions → Discord game
   release announcements** and confirm its card reached the correct channel.
   Do not publish a fake release just to test the integration.

The current game publisher uses authenticated GitHub CLI publication. If game
publication is later moved into Actions using `GITHUB_TOKEN`, GitHub suppresses
follow-on release workflows: explicitly invoke the notification as part of that
publisher or dispatch a dedicated notification workflow instead. Do not assume
the release event will run in that case.

Missing/invalid required-channel secrets fail before any messages are sent. Webhook
secrets stay in the notification step, never in PR checks, command arguments, or
logs. Discord redirects are refused. A post waits for confirmation and is not
automatically retried: timeouts can occur after delivery. Inspect the Discord
channel before rerunning a failed job; a manual rerun can duplicate a post.
Notification failure does not undo or modify the published game release.

## Member notification choices

Discord's native **Channels & Roles → Customize** page offers the optional
question **Which release notifications would you like? (Optional)**. Members may
choose Stable releases, Preview testing, both, or neither and change this later.
The answers assign only the matching alert roles; no staff/developer roles are
offered. The new-member preview verified both assignments and hidden private
channels. Onboarding is enabled with the public channels available by default.
Both alert roles are mentionable for webhook delivery; staff may also mention
them manually. Discord members' own mute and role-notification suppression
settings can still prevent push notifications.

The **feedback-and-ideas** forum is under Community, with Gameplay, Launcher,
Graphics, Multiplayer, and Accessibility tags. Its posting guidelines request
one clear idea per post and direct reproducible bugs to help-and-bug-reports.

## Offline validation

```sh
python -m unittest discover -s tests/repository -p test_discord_release.py -v
```

The workflow runs these regressions for relevant PRs and main pushes as well as
release events. Tests cover stable/preview routing, unrelated tags, drafts,
mismatched flags, bounded metadata-free notes, mention suppression, webhook URL
validation, redirects, and ambiguous delivery. They use mocks and never send a
Discord message. The script uses Python's standard library on either host.
