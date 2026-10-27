# Automatic known issues and workarounds

GitHub Actions edits the original post in the existing pinned
[Known issues and workarounds Discussion](https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions/143).
When the list changes, the same post is updated in place. No new Discussions or
snapshot comments are created. Identical runs do not write. The title, pinned
placement, category and locked state stay unchanged. Old snapshot comments
are historical; maintainers can collapse them as outdated without deleting them.

Open issues labeled **bug** or **bugged** appear, with links to the full reports.
The existing bug form and labeler use **bug**; no new label is needed.
Closing, deleting, transferring out or removing the bug label removes the entry.
Reopening a still-labeled bug brings it back. A closed issue is not a promise
that its fix is already in a published release.

To document a workaround, a repository owner, member or collaborator adds a
comment on the issue:

```markdown
### Workaround
Use the keyboard to select the option while mouse support is being fixed.
```

The most recently updated qualifying human maintainer comment wins. Edits update
the post; deleting it falls back to the next qualifying comment, or displays
"No workaround documented yet." To withdraw older advice, edit the latest note
to explicitly say no workaround is currently available. Ordinary conversation,
reporter guesses, bot comments and headings inside code fences are excluded.
The post links to the source note; long notes are shortened to 600 characters.

The generated post refreshes on issue, label and issue-comment changes. The
automatic labeler also calls the sync after finishing, because changes made with
`GITHUB_TOKEN` do not trigger another issue workflow. A daily reconciliation and
**Actions → Known issues Discussion → Run workflow** cover missed events and
bot-created changes. Identical lists do not write or bump the Discussion.
Concurrent runs serialize and always read the current issue list, not old event
payloads. A failed or oversized read leaves the previous post in place.

The Discussion body is generated: keep workaround notes on their issues.
Comments by other people or bots are preserved; the workflow never deletes them.
The target number, exact title and accepted labels live in
`.github/known-issues.json`. A missing target or changed title fails visibly
instead of creating duplicates or editing another post. Update the configuration
if the target is intentionally changed. Forks do not publish to this post.

Automatic runs use trusted default-branch code. The default Actions token has
only `contents: read` for checkout. Publishing uses the repository secret
`KNOWN_ISSUES_TOKEN`, explicitly passed by the automatic labeler's reusable
workflow call. This is a fine-grained personal token restricted to this repository
with **Discussions: Read and write** and **Issues: Read-only**. Edits are attributed
to the token owner's account. The default `GITHUB_TOKEN` was rejected for edit
mutations in live tests; it is never used as a fallback.

Create and save the token directly in GitHub; never put it in source, logs or
chat. Renew it before its chosen expiration and replace the same repository
secret. A missing, expired or unauthorized editing token fails the run without
posting a snapshot or changing the previous list.

The workflow never checks out or executes issue text. Tests run inside the existing **Issue form tests**
check, and the publishing workflow does not run on pull requests.
Manual dispatch uses the maintainer-selected branch so a fix can be validated
with the configured editing token before merge. GitHub's error type and message
are reported when an update is rejected.

Read-only local preview (public repository reads only; no token needed):

```sh
python tools/ci/known-issues.py --repository TheDoffman/timesplitters-future-perfect-pc --dry-run
python -m unittest discover -s tests -p test_known_issues.py -v
```
