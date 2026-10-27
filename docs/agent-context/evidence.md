# Source evidence for Sol advisories

The maintained map is the starting context. When an advisor needs a specific
source fact, it can ask the local controller to retrieve it and then refine its
plan. This is an external knowledge layer: no model weights are changed and no
complete repository understanding is claimed.

## Exchange

1. LOCAL gathers task facts; the supervisor selects a permitted Sol consultation.
2. Sol receives the task, relevant guide summaries, actual local results and a
   typed evidence protocol. It returns advice, or a JSON `need_evidence` request.
3. The local broker validates the request and reads only eligible source. It
   attaches revision, file SHA-256, Git-index blob and bounded lines/matches.
4. Sol receives those excerpts and either gives a plan or makes one further
   bounded request. LOCAL then implements and verifies the resulting plan.

All model output and retrieved text remain untrusted evidence. Instructions
embedded in a code comment cannot authorize tools, publishing or permissions.
Sol runs no shell/MCP/browser tools, edits no files and does not initiate a
nested session. The controller owns each inference and local retrieval.

## Request examples

Paths use forward slashes and are relative to the named namespace:

```json
{"kind":"need_evidence","question":"Which thread releases this gate?","requests":[
  {"kind":"references","namespace":"runtime","symbol":"xbox_Nv2aFlipGateRelease","scope":"src/kernel"},
  {"kind":"patches","query":"xbox_Nv2aFlipGateRelease"}
]}
```

After a search identifies a location:

```json
{"kind":"need_evidence","question":"What condition permits release?","requests":[
  {"kind":"read","namespace":"runtime","path":"src/kernel/kernel_bridge.c","start":2669,"lines":24}
]}
```

Example line numbers are navigation hints, not stable identifiers. Use current
search results and optional `expectedSha256` on `read` to pin a previously seen
version. `search` accepts a literal `query` and optional `scope`; `references`
accepts an identifier `symbol`. Both return lexical matches, **not** a verified
call graph or proof that an edge executes. References match complete,
case-sensitive identifiers; free-text searches use case-insensitive literals.
`patches` returns bounded matching
hunks from indexed top-level patches, with effective order only when the index,
loader and selected patch hashes agree. Historical removals are labelled.

## Eligible source and provenance

`repository` includes Git-indexed source/docs under `src`, `tests`, `tools`,
`patches`, `docs`, root project guides, public GitHub workflows/forms and explicit
default/build metadata and entry scripts. Personal configuration is excluded
even if accidentally indexed; only named public defaults/SDK/pack metadata in
`config` are eligible.
`runtime` includes public runtime source, HLE/kernel/GPU, lifter/templates,
extra-symbol data and manual title hooks in `external/ts3FPrecomp`. Its Git HEAD
must match the upstream revision pinned in Git-indexed `tsfp.ps1`.

Private assets, logs, build output, generated guest C, third-party trees,
credentials, personal configuration, arbitrary absolute paths and untracked
experiments are excluded. Symlinks, junctions and hardlinked source files are
rejected. Secret-shaped literals are redacted before transmission. No fetch,
checkout, source execution, write or permission change is part of retrieval.

The root and upstream revisions identify baselines; a SHA-256 identifies the
actual bytes read. `workingTreeDiffers` compares a direct read with its index
blob; patched runtime files normally differ from pristine upstream. Search hits
leave that comparison unknown (`null`). A difference alone does not identify
whether it came from the intended patch stack or an accidental edit. Keep
patch-ledger and executable provenance separate. Revision/file changes during a
consultation invalidate its evidence, requiring a fresh local assessment.

## Bounds and stop conditions

- At most two retrieval rounds, three requests per round, 6.5 KB of cumulative
  retrieved evidence per consultation. Every outgoing advisory document stays
  within the existing total 20-KB context limit, including guides and evidence.
- Reads request at most 80 lines from files up to 2 MiB. Searches scan at most
  400 eligible files / 8 MiB and return at most eight matches. Results explicitly
  report truncation; an empty bounded search does not prove global absence.
- Every actual hosted follow-up consumes the task's Sol attempt/call budgets.
  The existing advisory deadline is shared across the whole consultation.
  An exact-one instruction permits no second hosted response for retrieval.
- Duplicate/invalid requests, missing source, stale evidence, native denials,
  cancellation and exhausted budgets do not cause indefinite retries. A bounded
  missing-evidence handoff is not a completed diagnosis. LOCAL can gather a
  missing runtime measurement using ordinary permitted tools or report a blocker.

Implementation: `tools/router/repository-evidence.mjs` and
`tools/router/advisory-evidence.mjs`; contracts:
`tests/repository/repository-evidence.test.mjs`. The machine supervisor/router adapter is
installed separately; repository instructions remain usable without it.
