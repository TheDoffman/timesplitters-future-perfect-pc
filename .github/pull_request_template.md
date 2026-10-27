## Change

Describe the user-visible behavior and the platform implemented in this PR.

<!-- Link issues this PR resolves with Closes #123 (or use GitHub's Development
sidebar). After merge, the workflow posts the Cause and fix and Validation
sections to those issues. Incidental issue mentions do not receive a comment. -->

## Cause and fix

Explain what caused the problem and how this change fixes it. Describe the
behavior before and after, including any remaining limits. For enhancements,
explain how the new behavior works. This section is posted to linked issues
after merge, so replace these instructions with the actual explanation.

## Validation

Record relevant tests and host results. Mark unavailable checks as pending;
Windows results do not establish Mac support. Update `docs/project/STATUS.md` for runtime work.

## Shared implementation and platform validation

Describe the shared implementation and any OS/graphics adapters. Link the
entries updated in `docs/project/MAC_PORT_BACKLOG.md` for outstanding adapter or validation
work. Explain genuine platform exceptions or explicitly PC-only scope. Shared
source and a successful test on one host do not establish support on the other.
For changes with no platform impact, explain why a backlog entry is unnecessary.

Backlog entries:

- [ ] Feature logic and settings definitions are shared; any platform exception
      has a reason, capability boundary and backlog entry.
- [ ] Mac status is explicit: queued, in progress, awaiting validation, verified,
      or not applicable with a reason. Host-specific validation gaps are explicit.
- [ ] Common behavior stays shared; existing Mac support is preserved.
- [ ] New behavioral tests are registered in the Windows/macOS workflow.
- [ ] Relevant shared tests and available affected-host checks passed; unrun
      checks are listed.
- [ ] Usage documentation and validation records reflect the actual scope.
- [ ] Owning `docs/agent-context/` guides reflect changed systems; patch index
      refreshed if needed; `tools/repo-context.py check --base <base>` passes.
- [ ] Personal settings/saves are preserved and game tests use isolated paths.

Do not include private game assets, generated game sources, binaries, or saves.
