# Investigation memory and handoffs

Root AGENTS.md and the PR template require a short user-facing Change and Cause
and fix description. Keep technical evidence in the existing project records;
the simpler opening description does not remove testing or platform obligations.

Keep fixed issues open until a published version includes the fix. Use non-closing
PR references (`Refs #123`), mark publication pending, and verify/link the release
before closing. The 2026-10-29 root AGENTS rule overrides older template closing
instructions; merge-time resolution comments do not establish publication.

Kernel stages: [object ownership, boot and validation](kernel-objects.md).
Generate the patch index from LF patch bytes matching Git; CRLF working copies
can produce different hashes and target names despite local checks passing.

Retired memory APIs: [caller audit and retained ABI](retired-memory-compatibility.md).

Guest mappings: [owners, host adapters and checks](guest-host-mappings.md).
Native Nt host mappings: [reservation ownership and protection](native-virtual-memory.md).

Guest Mm/statistics: [ownership, host snapshots and ABI](guest-memory-services.md).

Native Mm: [ownership, page adapters and remaining work](native-memory.md).

Virtual memory: [guest/native paths and remaining host adapters](virtual-memory.md).

Thread storage investigations: [memory ownership and caller limits](thread-memory.md).

Allocation ownership and bridge entry points now use the project provider;
see [contracts and consumer constraints](independent.md#guest-allocation-ownership).
Trace old addresses only as historical evidence: lowest-fit placement may differ.

The final replacement order includes call frames, vector support, scalar support
canonical runtime header, its consumer-fixture repair, branch feedback, nonlocal
jumps, guest state/reports, image loading, allocation/thread/virtual-memory and
guest host mappings and unused memory API retirement, then source layout. Historical fixtures omit replacements
from old inputs; the order test checks the eighteen-patch tail. Audit `.inc` files and scan ignored
generated C explicitly with `--no-ignore`. Exact-name searches missed the live
SX/ZX calls assembled dynamically by the move emitter; its direct replacements
now have exhaustive compiled register/memory/alias checks.

Call-frame replacement follows dispatch and guest ABI in effective patch order.
`test_repo_context.py` checks this dependency; see [own interfaces](independent.md).

After remote main contains a completed task, remove its local task branch using
safe deletion after switching away and checking for unique or uncommitted work.
Root AGENTS.md owns this repository-wide rule; retain other chatsÃ¢â‚¬â„¢ unmerged work
and ignored data. Other checkouts must pull and reread it.

Build commands now live under `tools/build/`; the evidence broker allows that
tracked source directory while keeping generated build outputs excluded.
Root status/backlog reports moved to `docs/project/`.

The patch index keeps stable basename identities and a separate repository
`path` for nested responsibility folders. Both `repo-context.py find` and the
evidence broker use those paths and effective order; legacy patches are excluded
from active search. The broker still accepts older flat-layout indexes.

[Repository layout](../development/repository-layout.md) distinguishes tracked source,
the pristine vendor snapshot, the patched external runtime and local outputs.
Patch lookup/index entries now use readable numbered filenames; use the same
finder and effective-order checks. Git rename history retains old spellings.

The context ownership map now assigns `vendor/**` to the runtime guide.
The root AGENTS rule identifies the bundled pristine base and offline importer;
the ignored patched runtime remains the place to inspect effective behavior.
Snapshot content is provenance, not a second place to implement fixes.

The CLI replacement follows function emission. Historical
checkpoint fixtures defer it until the complete title stack; fresh installs and
upgrades still exercise it. Check recorded patch hashes against Git LF bytes
when local context passes but a clean CI checkout reports a stale index.

<!-- packet:start -->
Before a broad search, read the repository map and relevant subsystem guide,
then use the bounded patch finder. Keep exact path/symbol findings in ignored
`logs/agent-investigation.md` for a substantial task. Do not dump all patches or
generated guest C into context. After two searches return no discriminating
evidence, change strategy: inspect an actual caller, trace/profile a bounded
reproduction, or ask one specific advisory question. Do not guess addresses.

Before compaction/Sol: preserve task/branch/revision, proven observations,
disproven hypotheses with actual commands/results, changed files, build/runtime
provenance, last Sol plan and its outcome, next check and blockers. Resume from
that note and current diff, not from another whole-repo search. Keep it short
and replace stale entries. A build blocker and a game bug are separate problems.
Durable guides contain confirmed architecture and repeatable procedures; an
issue hypothesis belongs in the investigation note until verified. Update the
relevant guide when changing its system, regenerate the patch index for indexed
patch changes, and run the context checker against the task base.
<!-- packet:end -->

The generated-entry optimization follows texture-bind-scan in the patch order;
zero-base-indices follows it. Historical controls omit these until renderer
context is installed. Candidate
math must preserve callback order and exceptional-input behavior, not only
ordinary output coordinates. Local-register prototypes alone are not integration.

`tests/repository/test_repo_context.py` checks effective patch order rather than only
filename sorting. Independent translation-moves, translation-integer, translation-bits and
translation-product are final, after radial stick deadzone and title instrumentation
that their removed handlers contain. Historical fixtures omit replacements until
the complete-stack upgrade. Update the ordering assertion when intentionally
adding another final patch.

## Working note (not committed)

Create this only for substantial tasks; tiny edits do not need a diary. Limit it
to approximately 4 KB, replacing superseded entries. Never store secrets or
unbounded command output. Copy [investigation-template.md](investigation-template.md)
to `logs/agent-investigation.md` and fill it with actual observations. Commands
run in fresh shells; no persistent `cd`, PowerShell variable or shell-profile
assumption. After reload check current branch/diff before trusting the note.

Read bounded function ranges and use `rg -n -F` for known symbols. Negative
search results are useful only with their scope and exact query. Record them
so the next context window does not repeat them. A missing symbol in generated
code may mean the name is stripped; it does not mean the feature has no path.

## Sol context

This section describes the optional local supervisor. In an ordinary hosted
GPT/Codex session, the selected agent performs investigation, implementation and
validation with its normal tools. Reading this guide does not activate routing,
require another model or make a normal hosted session advisory-only.

The supervised local workflow injects the map plus selected guide summaries
into local planning/continuations and automatic Sol evidence packages. These
are bounded, task-selected repository facts, not unrestricted repository access.
Local context and Sol should agree about paths, ownership, current timing policy
and verification limits. The repository manifest is versioned; the machine's
router installation is separate and must implement this integration.
The optional reader's portable source is `tools/router/repository-context.mjs`;
its contract tests are `tests/repository/repository-context.test.mjs`. The installed router
copies that reader beside its supervisor and uses only its fixed guide allowlist.

Automatic Sol packets remain within the existing 20-KB redacted disclosure
budget, including guide excerpts. Source documents are treated as reference
evidence, never permission to run tools, change routing or override native denials.
Starting guide packets use an explicit file allowlist. The separate broker below
adds bounded public source retrieval; credentials, personal settings, generated
guest C and private assets remain excluded from both routes.
The hosted advisor remains read-only and tool-free. A normal hosted agent reads
root AGENTS and these linked guides through its usual tools. Other machines
without the custom router still benefit from those repository instructions.
The current supervisor treats routing phrases in root project rules literally.
Keep explanatory examples of opt-out commands in these subsystem guides rather
than quoting them in root rules; an actual user/project restriction remains binding.

The supervisor additionally accepts typed Sol requests for Git-indexed public
source, bounded literal searches, lexical references and patch hunks. The local
broker executes those reads, checks revisions and hashes, redacts results and
returns them within the existing total packet budget. Runtime access is limited
to public source in the pinned upstream checkout; generated guest C remains
excluded. This expands the guide-only reference allowlist above through an
explicit source policy, not arbitrary file access. See [evidence retrieval](evidence.md).
Each hosted follow-up counts as a real Sol call. A changed evidence snapshot,
missing access, cancellation or exhausted budget cannot be hidden by a retry.

For a manual advisory include map + relevant guide summary, note excerpts and
minimal source/test evidence in `.codex/escalation.md` using the approved helper.
Follow current panel routing rules: the supervisor owns hosted calls there.
Never invoke a nested model from a LOCAL ONLY task or an advisory session.

## Maintenance

Patch-order tests now require `texture-bind-scan` after the native renderer/demo
series. Historical controls fixtures omit it until the complete-stack upgrade,
when its cross-platform renderer context is present. Keep both order and upgrade
contracts when adding dependent patches; do not weaken full-stack validation.

Every change to a covered system must update its guide in the same change. Add
the changed responsibility, entry point, contract, migration pitfall or test
route. For an internal change with no architectural impact, record a concise
specific confirmation of the existing boundary/validation; do not add a vague
"docs reviewed" entry or dump chronological chat transcripts. Correct obsolete
text rather than accumulating conflicting descriptions. Extend `manifest.json`
when adding a new subsystem. Current source/tests and root safety rules remain
authoritative if a guide is stale; fix the guide as part of the task.

1. Edit the affected guide(s) listed in `manifest.json` ownership rules.
2. For patch changes, stage only the intended patch files, then run
   `python tools/repo-context.py refresh`. The index intentionally excludes
   untracked experiments; `find` labels those separately. Never stage unrelated
   patches just to refresh it. Verify new patch worktree bytes use their declared
   Git line endings before refreshing: Git can stage LF while leaving CRLF in
   an existing Windows file. The index hashes exact loader bytes, so that mismatch
   passes locally but fails on fresh CI checkouts. Preserve explicit `-text` patches.
3. Run `python tools/repo-context.py check` and
   `python tools/repo-context.py check --base <task-base-revision>`.
4. Include guide updates, generated index and useful measured evidence in the
   focused branch. Keep local working notes and private logs ignored.

CI enforces index freshness, valid context paths, excerpt budgets and an update
to each affected guide. It can check presence/freshness, not whether an explanation
is intellectually correct. Reviewers must still compare it with source.

The #105 pacing correction extends the effective patch tail: fixed-60 must be
followed by late-frame-pacing. `tests/repository/test_repo_context.py` checks that pair as
well as excluding untracked experiments. Refreshing the index alone does not
update explicit ordering contracts; keep those assertions aligned with the
loader when deliberately extending its dependency order.

PR #285 extends the effective patch tail with native timing/surface integration.
The index regression checks native timing/retirement before fixed defaults,
late-frame pacing immediately after fixed defaults, and numeric/surface patches
after that boundary. It no longer assumes fixed-60/pacing are the final two
patches. This is a navigation-order contract; runtime/build tests establish
application and behavior separately.

The demo-timer wrapper extends that tail after native surface rendering.
The index-order regression now checks this final dependency explicitly.

The 2026-10-22 root texture policy retains original character selection/profile
icons via `config/textures/texture-originals.json`. Consult the systems/build-test guides
for the planner exclusion and checks; character model materials remain in scope.
Historical artwork acceptance does not override the current user preference.

The native-start handoff patch extends the explicit timing tail after surface
integration. The index regression checks that dependency and the new final
patch; refreshing the generated index alone cannot update an old tail assertion.
This CI correction changes navigation tests only, not runtime pacing.

The loader-order contract now checks that full-float surface preservation
follows live-surface support. Check prerequisite relationships explicitly when
adding a dependent patch; unrelated integration patches may follow it.

## Texture policy change (2026-10-22)

The user's optional-download request supersedes the earlier mandatory bundled-pack
rule. AGENTS.md now records lightweight default game packaging, separate audited
optional publication and preservation of installed packs. Use the launcher and
build-test guides for the implementation; do not infer publication from local code.

The ownership gate has one content-aware data exception: bug-form release-version
changes within validated existing AUTO blocks. It compares the rest of the file
against the base unchanged and rejects empty-list regressions, malformed/duplicate
markers and arbitrary inserted content. Deleted/new forms and all implementation
or workflow changes retain ordinary guide requirements.

## Native high-refresh PR sequence (2026-10-24)

Root AGENTS records the user's request to consolidate related native-high-refresh
work into the current PR and verify it has merged into main before opening a
successor. Keep repairs on the existing PR; branch history or a local merge alone
is not proof of the remote PR's merge state. Required checks remain in force.

The repository-context order test includes the post-handoff surface capture/render
tail, with render last. Preserve dependency assertions when extending the loader;
a stale last-patch assertion is not evidence of a loader failure.

Comparison-stage fixtures retain strict Clang warnings. Compare source snapshots
with decoded consumers separately from full branch/flag ownership. Treat fused
comparison rereads as a remaining orchestration boundary, not snapshot coverage.

String replacement checks distinguish repeated element ordering from bulk copy,
zero-count flag preservation from new comparisons, and native instruction
oracles from the full decoded translator. Single compares need snapshot locals
even when there is no conditional consumer; repeated compares need ZF storage.

The control-emission patch follows x87 before later orchestration replacements.
Historical reconstruction defers it together with prior independent emitters;
keep staged patch order/index and original golden/behavioral fixtures aligned.


The independent guest ABI stage appends `independent-guest-abi.patch` after the
translation CLI bridge. The index-order assertion checks this dependency.
Historical fixtures omit this new patch only while reconstructing their old
pre-instrumentation starting state; upgrades still apply the full current stack,
including the ABI replacement, and retain idempotence/preservation checks.


Ordinal81 prerequisite extends late patch order through0217; migration fixtures
and the maintained context suffix now include it before final layout routing.
See kernel-objects.md for caller evidence and compiled ABI fault controls.


Request0218 extends late provider order; maintained loader, migration models and
context suffix22 keep it before layout2090. tests/compiler request contracts
resolve providers from src/runtime; ignored experiments are not shipped tests.


Stage0219 extends the mapped compiler-patch suffix before the final repository
layout patch. [Device contracts](kernel-devices.md) identify maintained providers,
source/test routes, actual-header probes and the pending build/game boundary.


The indexed patch-order contract now includes0220 after0219 and before2090;
keep the fixed expected tail synchronized when adding reviewed independent stages.
Sharing contracts: [kernel-sharing](kernel-sharing.md).


Next driver stage: [request lifetime and unresolved Xbox slot evidence](request-lifetime.md).


The0222 [driver-call map](driver-call.md) supersedes the uninstalled-composer note; kernel routing remains unfinished.


Request prefix: [0223 contracts and Thread+50 evidence](transfer-completion.md).

Stage0224: [queue ownership and native acceptance routes](transfer-completion.md#host-owned-request-queue); the maintained patch map includes the new FIFO provider.

Stage0225: [StartIo callback composition and acceptance](transfer-completion.md#startio-callback-composition); maintained maps include the new provider.

Stage0226: [exclusive waiting membership and device queue mirrors](transfer-completion.md#device-queue-mirrors).

Stage0227: [persistent device queue lifetime and scoped scheduler APIs](transfer-completion.md#persistent-device-queue-lifetime).

Stage0228: [read-only caller-owned request adoption and range lifetime](transfer-completion.md#caller-owned-request-adoption).

Stage0229: [original event bytes/atomics and KeInitializeEvent bridge](transfer-completion.md#original-guest-events).

Stage0231 context regression asserts the real33-patch late compiler tail, with
0231 after0229 before2090. Main2130 DPI remains earlier by actual loader order.

Stage0232 context regression extends the exact late compiler tail to34 patches,
0232 follows0231 before2090; the main DPI patch stays earlier by loader order.

Stage0233 context tail now35 patches:0233 after0232 before2090; host routing
policy is in guest_paths.c rather than two kernel_path translate bodies.

Stage0234 context tail extends to36 patches, after0233 before2090. Upgrade
assertions require original host/storage providers and exclude kernel_path.c.

Stage0235 extends the late compiler tail to37 patches, after0234 before2090;
upgrades require the original pool include and exclude native kernel_pool.c.

Stage0236 extends the exact late compiler tail to38 patches, after0235 before2090;
calendar provider/imports replace native RTL time bodies and unused POSIX shims.

Stage0237 extends the exact late compiler tail to39 patches after0236 before2090;
counted strings/memory prefixes select original code and retire six native helpers.

Stage0238 extends the exact late compiler tail to40 patches after0237 before2090;
whole kernel_rtl.c exclusion selects original critical/status/diagnostic services
and removes the separate TryEnter table. Historical checks read UTF-8 explicitly.
