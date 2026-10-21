# Automatic launcher compatibility checks

`Launcher compatibility checks` validates launcher changes on Windows x64 and
native Mac ARM64. It uses standard GitHub-hosted runners, which are free for this
public repository. No OpenAI API key, API credits or third-party AI service is
required. The prior paid review/proposal workflow has been replaced.

## What happens on a PR

1. Every PR and push to `main` runs a small scope check so the final `Launcher compatibility` status
   is always reported. Launcher source, adapters, tests, shared configuration and
   build/workflow changes select both hosts. Removed or renamed launcher files
   remain in scope. Unrelated game/documentation changes skip launcher builds.
2. The shared read-only `launcher-build.yml` workflow builds/tests both launchers,
   runs their common UI tests, verifies all release assets and exercises the Mac
   packaged install/corruption/repair/restart path. It also owns the Windows
   startup-preference, packaging and game-icon checks. Runtime CI no longer
   repeats launcher builds or these tests. Releases reuse this workflow.
3. `Launcher compatibility` passes only after the scope check and all selected
   builds/tests/package checks pass. Failure, cancellation, missing output or an
   unexpectedly skipped build cannot pass. Fork PRs use read-only permissions and
   GitHub's ordinary approval policy; no secrets are needed.
4. Failed checks link to build logs. Fix the shared source or the missing host
   adapter in the same PR, or ask Codex in the existing chat to make the repair.
   CI itself does not generate code, open repair PRs, merge or publish a release.

Common UI/model changes compile into both launchers. OS-specific calls still need
an implementation for the other host or an explicit capability/backlog entry.
Both hosts remove retired high-FPS preferences and migrate old timing values to
the fixed 60 FPS policy (MAC-054), while retaining unrelated settings.
The new native frame-rate selector is separate: the common model/page writes
`pc-frame-rate.ini` and passes `TSFP_NATIVE_FPS`, with original timing as default.
Shared UI regressions exercise selection, game-menu preference round trips,
concurrent edits, invalid input and transaction rollback on both host builds.
These tests do not establish high-refresh gameplay correctness or Mac rendering.

Passing checks establish tested behavior, not complete visual or system-integration
parity. Discord sessions, native dialogs, physical displays, signing/notarization
and clean-machine delivery may need additional host checks. Record gaps in
`MAC_PORT_BACKLOG.md` and measured results in `STATUS.md`.

## Installation and operation

Merge the shared launcher/dual-platform release stack after its checks pass.
Add **Launcher compatibility** from GitHub Actions to the default branch's
required checks after its first successful PR run, preserving existing Windows/Mac
checks and the up-to-date requirement. Keep human merging. The workflow can also
be dispatched manually to validate the selected branch; it never publishes.

No enable variable or API secret is used. Repository setup removed
`OPENAI_API_KEY` and `LAUNCHER_COMPATIBILITY_ENABLED`. The ignored local API key
created during the earlier setup is unused by CI and can be revoked independently.
Do not upload ChatGPT session credentials to GitHub as a substitute.

Standard-runner availability/pricing: [GitHub runner documentation](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).

## Subscription-based cloud repairs

Codex cloud uses ChatGPT sign-in and the plan's usage allowance. It does not
require an OpenAI API key, but its allowance is limited. Do not enable paid
overages or an API fallback for this repository's compatibility automation.

The repository's ChatGPT Codex Connector installation is restricted to this
repository. Code Review is configured for team PRs on every push; additional
credit use for reviews is off. A private cloud environment named
`timesplitters-future-perfect-pc` is published with shared-source/host-adapter
instructions and no API secrets. Its initial Linux/Python 3.12.14 setup at
`c238ab32229fb5da6b3e618112d704f9dc6c8082` passed the 7 compatibility,
16 release-policy and 3 shared-UI Python tests. These are portable contracts,
not the Windows/Mac C# UI or native package tests.

Automatic reviews, cloud coding and automatic repair dispatch are separate
capabilities. The current web event-task dry run can read this repository and
filter PR activity, but cannot directly select or launch the published coding
environment. No event repair task is enabled yet. On 2026-10-19, the isolated
[PR #192 dispatch test](https://github.com/TheDoffman/timesplitters-future-perfect-pc/pull/192#issuecomment-6280238841)
reached the GitHub bot, which required a separate integration environment.
Its setup link redirected to the ChatGPT home page in both available browsers.
The published current-generation cloud environment does not satisfy that
legacy integration requirement. Do not enable a watcher that repeatedly posts
repair requests until a successful cloud task and branch update verify the path.

The published environment remains usable for manually started cloud coding
tasks. The GitHub review test on PR #192 completed without major findings at
`a521cad8cc1108d2111cf2885950fe3d9c6f9b57`; that verifies reviews, not coding
dispatch. The local worker below supplies a separate repair route.

A repair watcher must restrict work to open, non-draft, same-repository PRs by
the maintainer targeting `main`. It must wait for current-head CI, use evidence
from failed checks, and deduplicate work
by PR/head revision. Repairs start from the exact PR head, preserve its edits,
and use a reviewable branch. Allow at most one repair cycle per original head;
stop for concurrent changes, missing access, exhausted plan
usage or unresolved failures. Quiet runs need no status comments.

Cloud repairs must obey `AGENTS.md`, preserve settings and pins, keep common
policy shared and update adapters/backlog evidence. Do not weaken checks,
force-push, merge, publish releases, alter protections or transfer private game
data. Inspect both hosts' required checks on the resulting revision before
reporting a repair successful. Visual and gameplay limits remain explicit.

See OpenAI's [GitHub integration](https://learn.chatgpt.com/docs/third-party/github),
[cloud environments](https://learn.chatgpt.com/docs/environments/cloud-environments)
and [scheduled tasks](https://learn.chatgpt.com/docs/automations) documentation.

## Dedicated Mac worker

`tools/compatibility-worker.py` is a trusted local controller for this repository.
It polls every 15 minutes while the selected Mac is awake, online and logged in.
It uses that Mac's GitHub CLI account and existing ChatGPT-authenticated Codex
CLI. It does not copy credentials from another device or load `.env.local`.
There is no API fallback, purchased-usage setup, auto-merge or automatic release.
ChatGPT plan allowances still apply. Install on **one Mac only**.

The initial trigger is a failed current-head `Launcher compatibility checks` or
`Runtime patch transitions` run. Both workflows must finish first. Draft/fork
PRs, other authors, other base branches, cancelled/partial runs and previously
attempted PR revisions are skipped without invoking a model. Automatic Code
Review remains separate; a review comment alone does not trigger this worker.

Each repair has a 30-minute limit. Codex receives an expendable source snapshot
without `.git`, personal data or generated game files. Its shell has no network
access or approval escalation, and app/plugin integrations are disabled. The
controller accepts only bounded ordinary source/test/doc/patch changes; workflow,
credential, dependency pin, agent policy and worker changes need manual handling.
The controller creates `codex/compat-repair-<PR>-<head>` and opens a repair PR
**against the original PR branch**. It leaves that source branch unchanged.
Review both hosts' CI results before merging the proposed fix into its parent PR.
No source repair is certified merely because Codex returned successfully.

Local state prevents duplicate model calls, including after crashes. A published
repair branch/PR also prevents a second Mac from repeating that repair. Run only
one installed worker: simultaneous independent first attempts are not coordinated
across devices. Any repair error pauses further repairs until inspected. Resuming
does not retry an already consumed source revision; commit the next intentional
source change or handle that repair manually. This bounds quota failures and loops.

From a trusted checkout of this branch on the selected Mac:

```sh
python3 tools/compatibility-worker.py check
python3 tools/compatibility-worker.py dry-run
python3 tools/compatibility-worker.py install
```

If discovery needs help, add `--codex /absolute/path/to/codex` and
`--gh /absolute/path/to/gh` before the command. `check` verifies the GitHub account,
ChatGPT authentication and required CLI capabilities without a model call.
`dry-run` queries GitHub but does not edit source, publish, install a service or
invoke Codex. Sign in with `gh auth login` and `codex login` on the chosen Mac
when needed; do not use `codex login --with-api-key`.

Installation copies the reviewed controller to
`~/Library/Application Support/TSFP Compatibility Worker/worker.py` and installs
the per-user LaunchAgent `com.timesplitters.compatibility-worker`. That folder
holds its small source-only Git cache, bounded last-agent log and state. It stays
outside Desktop/iCloud and the user's active project checkout. Temporary repair
snapshots are removed after each attempt. No game build or data transfer is needed.
Updating the checkout does not silently replace the installed controller.

```sh
python3 "$HOME/Library/Application Support/TSFP Compatibility Worker/worker.py" status
python3 "$HOME/Library/Application Support/TSFP Compatibility Worker/worker.py" pause
python3 "$HOME/Library/Application Support/TSFP Compatibility Worker/worker.py" resume
python3 "$HOME/Library/Application Support/TSFP Compatibility Worker/worker.py" uninstall
```

`status` shows the last poll and repair outcome; `last_error` identifies failures.
`pause` preserves the service but stops repair attempts. `uninstall` unloads the
service and retains source/state/logs for recovery. Review and uninstall before
installing an updated controller. Moving to another Mac requires uninstalling on
the old Mac and signing in/installing separately on the new one. Account sharing
does not synchronize the worker or its local state.

The portable regression suite exercises a complete local Git repair/publication
simulation with mocked GitHub and Codex, plus concurrency, stale CI, source-head
races, authentication, symlink/path restrictions and bounded process execution.
It runs on Windows and macOS CI. It does not prove a model can resolve every
failure, native UI parity, physical input or game compatibility.

## Development checks

Run `python -m unittest discover -s tests -p test_launcher_compatibility.py -v`
and `python -m unittest discover -s tests -p test_launcher_release.py -v`.
The worker's asset-free checks are
`python -m unittest discover -s tests -p test_compatibility_worker.py -v`.
Both host jobs also run shared UI tests and the host launcher regression suite.
Scope tests exercise real Git rename/deletion diffs and completion tests cover
failure, cancellation, skipped builds and missing scope results.

## Runtime CI selection and parallel checks

`Runtime patch transitions` selects work once, then runs patch-upgrade and native
runtime jobs independently on both Windows and macOS. Both jobs check out the
pinned upstream revision; native tests apply the full patch stack themselves.
Parallel execution reduces the serial wait, but adds runner setup overhead and
does not promise fewer total runner minutes for a full runtime change.

| Changed inputs | Selected work |
| --- | --- |
| Runtime patches | Patch upgrades, native tests and Mac GPU contracts |
| Runtime source or unknown runtime inputs | Native tests and Mac GPU contracts |
| Known offline texture/Python tools | Native tests, without GPU setup |
| Launcher source, builds or packaging/icon tests | Dedicated Windows/Mac launcher workflow |
| Release validation tools | Release contracts on Windows and Mac |
| Runtime CI policy, dependency pins or patch application machinery | All runtime suites |
| Documentation | Lightweight scope, handoff and worker checks |

Selections combine for mixed changes. Shared inputs can deliberately select both
runtime and launcher workflows. Unknown runtime inputs retain GPU coverage;
only explicitly recognized offline tools omit it. Issue automation keeps its
existing separate checks.

The required names `Patch transitions (windows-latest)` and
`Patch transitions (macos-latest)` stay unchanged. These final jobs retain worker
regressions on their real hosts and require every selected runtime matrix to
succeed. An intentional scope skip is allowed; missing output, failure,
cancellation or an unexpected skip fails both gates. They do not need changes to
branch protection. `Launcher compatibility` separately gates both launcher builds.

Manual dispatch selects all suites owned by that workflow: dispatch runtime CI
for full runtime validation and launcher compatibility for both launcher builds.
These asset-free checks do not launch the game or verify visual/gameplay parity.
