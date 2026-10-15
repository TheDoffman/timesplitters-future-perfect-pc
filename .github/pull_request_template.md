## Change

Describe the user-visible behavior and the platform implemented in this PR.

## Validation

Record relevant tests and host results. Mark unavailable checks as pending;
Windows results do not establish Mac support. Update `STATUS.md` for runtime work.

## Mac port backlog

New PC features can ship before Mac adaptation. Link the entry or entries added
or updated in `MAC_PORT_BACKLOG.md`, including source references, remaining Mac
work and acceptance checks. For documentation-only changes or changes with no
PC feature or Mac adaptation impact, explain why an entry is not applicable.

Backlog entries:

- [ ] New PC features and fixes needing Mac adaptation are recorded in the backlog.
- [ ] Mac status is explicit: queued, in progress, awaiting validation, verified,
      or not applicable with a reason. Queued work does not block PC completion.
- [ ] Common behavior stays shared; existing Mac support is preserved.
- [ ] Relevant shared tests and available affected-host checks passed; unrun
      checks are listed.
- [ ] Usage documentation and validation records reflect the actual scope.
- [ ] Personal settings/saves are preserved and game tests use isolated paths.

Do not include private game assets, generated game sources, binaries, or saves.
