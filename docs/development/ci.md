# CI and branch protection

The live `Base` ruleset currently requires these three GitHub Actions checks
on every pull request (verified 2026-10-25):

- **Patch transitions (windows-latest)**
- **Patch transitions (macos-latest)**
- **Launcher compatibility**

**Issue form tests** and **Issue resolution tests** run only when their source,
templates, or workflow files change. They are not required branch checks.
Event automation runs in separate workflows:

- **Automatic issue labels** runs when issues are opened, edited or reopened.
- **Issue form versions** validates and proposes reference updates after release
  changes, relevant pushes to `main`, or a manual dispatch.
- **Issue resolution notes** comments on linked issues only after a PR into
  `main` is merged, using trusted default-branch code.
- **Known issues Discussion** keeps the pinned known-issues post synchronized
  with open bug reports and maintainer workaround notes. It also runs after
  automatic labeling. See [maintainer instructions](../players/known-issues.md).

`PR validation` is a candidate single required check. It uses the same runtime
scope and original Windows/Mac workers, selects launcher builds from the same
revision, and fails if any selected worker fails or is unexpectedly skipped.
The old required checks continue reporting during the rollout. After the new
gate has passed on a PR, replace the three existing required contexts with
`PR validation` in the `Base` ruleset. Only then remove the old reporting jobs
and the separate PR trigger in `launcher-compatibility.yml`. This staged change
avoids a window where protected PRs wait for a check that does not exist.

The runtime plan always tests suite selection. It runs affected patch, native,
release and launcher tests when relevant files change. A documentation edit
still gets a completed gate without full game or launcher builds. No
private game files or personal saves are needed in CI.

The `Base` branch ruleset targets `main` with active enforcement. It requires a
pull request, the three checks listed above, and an up-to-date branch.
Deletion and force pushes remain blocked. There are no bypass actors or required
reviewer approvals, so a solo maintainer can merge once checks pass.

If `main` advances, use **Update branch** or merge `main` into the PR branch and
wait for fresh checks. Repository-level **Allow auto-merge** makes the option
available; it does not automatically select every PR for merging. Choose
**Enable auto-merge** on the individual PRs you want to merge after checks pass.

Published version references are updated through a PR on
`automation/issue-form-versions`, rather than a direct push to `main`. Updating
that branch preserves its previous tip and the current default branch as
ancestors; it never force-pushes or replaces unrelated edits. The workflow needs
`contents: write`, `pull-requests: write` and the repository's **Allow GitHub
Actions to create and approve pull requests** setting. It does not approve PRs.

GitHub may require a maintainer to select **Approve workflows to run** on a
token-created PR before its checks start. After approval, the same required
checks and up-to-date rule apply. Enable auto-merge on the version PR if desired.
No personal access token, new secret or branch-protection bypass is needed.

If renaming a required job or changing its matrix, update the ruleset's check
names as part of the same rollout so PRs are not left waiting for a missing check.
