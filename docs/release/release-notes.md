# Write release notes for players

Every new game and launcher release needs reviewed notes explaining the final
changes in everyday language. Start from [the template](notes/TEMPLATE.md).
Commit the completed notes with the version change before building/publishing:

- Game: `docs/release/notes/v<game-version>.md`.
- Launcher: `docs/release/notes/launcher-v<launcher-version>.md`.

Use the exact tag, including any preview suffix. Game `-NotesFile` can select
another reviewed file with the same format. The launcher workflow reads the
versioned file from its exact source commit. It needs no AI service or API key.

## Explain the problem and the result

Under `## What changed`, give each player-facing change a short `###` heading:

```markdown
## What changed

### Doors open correctly for everyone in an online match
**Before:** A guest could see a door stay closed after the host or a bot opened it.
**Now:** Guests see the same opening and closing animation as the host.

### See what the launcher is doing during an update
**Before:** An update could appear stuck while the launcher was unpacking files.
**Now:** The launcher shows which update step is running and its progress.
**How to use it:** Start the game update as usual; progress appears automatically.
```

These examples describe changes included in 0.7.1/1.7.1; they are writing examples,
not a claim that a future version contains them again.

- Explain what players noticed and when it happened. Then say what works now.
  For a new feature, describe the previous limitation and the new capability;
  do not invent a bug to fit the format.
- Explain a cause only when established, using ordinary words. Keep file names,
  internal symbols, implementation reports and test counts in project records.
- Give menu paths or steps when players must enable or change something. Say
  which platforms are affected and describe any remaining limits beside the change.
- Group related fixes; avoid duplicate entries for intermediate attempts. Describe
  the final behavior in the released source, including changes that superseded
  earlier PRs. A title such as “Launcher updates” is not an explanation.
- Compare the exact source range with the previous published release. Cover all
  player-facing changes and leave out version bumps, CI and housekeeping unless
  they change the player's experience. Launcher notes cover launcher behavior;
  a mixed game/launcher PR must not imply the launcher ships a game fix.
- Include known issues, system requirements, save/multiplayer compatibility and
  migration steps where relevant. Distinguish verified behavior from untested
  hardware or full-campaign acceptance. Never promise that every issue is fixed.

## Review before publication

Read the rendered Markdown as someone who has not seen the PRs. Each entry must
answer “What was wrong or missing?” and “What will I notice after updating?”
Check every claim against the released source and validation evidence. Human
review is required: the automated check validates structure, not factual accuracy,
coverage or the quality of the writing.

Both publishers reject missing notes, title-only lists, empty Before/Now fields,
template placeholders and forged channel/source metadata. The game checks before
building or reusing a package. Launcher publication checks the versioned notes
before creating a tag or draft; publishing workflow dispatch also checks before
building. Validation-only launcher builds do not need notes for an unused version.
Related PRs and the full source comparison remain below the reviewed explanation
as references. Existing published release assets and tags stay immutable.
