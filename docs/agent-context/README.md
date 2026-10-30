# Repository map for agents

Read this first, then **one** relevant guide below. This is a navigation and
working-memory layer, not a replacement for root `AGENTS.md`, source, or tests.
It describes the tracked baseline; uncommitted patches and installed binaries
may differ. Check branch, diff and build provenance before drawing conclusions.

File placement and naming: [repository layout](../development/repository-layout.md).
Player/developer documentation: [guide index](../README.md).

Hosted GPT/Codex contributors use these guides with their usual model and tools.
The separately installed LOCAL SMART controller is optional. Its Sol advisory
roles, budgets and evidence protocol apply to that integration; ordinary hosted
agents can implement/build/test directly without Qwen, Ollama or a router.

<!-- packet:start -->
This repo wraps a pinned Xbox static-recompilation runtime. Authoritative edits
live in tracked `src/`, `tools/`, `config/` defaults and `patches/**/*.patch`.
`external/ts3FPrecomp` is the ignored, patched runtime checkout, NOT a second
authoritative source tree. Its generated guest C is under
`titles/futureperfect/src/recomp/gen/`, NOT `build/windows/generated`.
The pristine base is tracked in `vendor/ts3FPrecomp/source`; the offline
`tools/build/prepare-runtime.py` importer creates the ignored checkout at the same
upstream pin. Keep changes in shared patches, not the pristine snapshot.
Use runtime-relative paths when inspecting upstream: `src/hle/`, `src/d3d/`,
`src/kernel/`, `titles/futureperfect/src/recomp_manual.c`.

Find a symbol/path with `python tools/repo-context.py find FlipGate` (Windows:
use `.venv/Scripts/python.exe`). The tracked patch index lists targets and the
loader's effective order; `find` also searches current patch text and tracked
source. Inspect the resulting runtime function and its callers, not every patch.
Later patches supersede earlier behavior. `patches/legacy/` is unwind-only.
The final fixed-60 patch retains dependencies on historical presentation patches;
do not delete them because their feature was retired. Default is original
60 FPS / 60 Hz; launcher native 120/180/240 beta choices need compatible game builds,
not the retired interpolation engine. Never edit generated C or invent addresses.

Windows: `powershell -NoProfile -File tsfp.ps1 -Action Build`; native Mac:
`tools/build/build-native-mac.py`. Use `tools/testing/testing_profile.py` (Windows) or
`tools/testing/test-native-mac.py` (Mac) for bounded isolated game tests. Never use
personal saves/settings. Build success, menu startup and verified gameplay are
different claims. Inspect the FIRST real compiler/linker error before changing flags.

For a substantial investigation keep a short ignored `logs/agent-investigation.md`:
task + revision, exact paths/symbols, proven facts, disproven hypotheses with
commands/results, next discriminating check and blockers. Update before a Sol
handoff or compaction. Do not repeat a search unless new evidence changes it.
Sol receives bounded guide excerpts and task evidence; it cannot see the entire
repo or local runtime automatically. Advice is a hypothesis until locally tested.
<!-- packet:end -->

| Task | Read next |
| --- | --- |
| Runtime, patches, HLE, guest symbols, renderer, FPS | [Runtime](runtime.md) |
| Controls, display, audio, networking, textures, crash handling | [Systems](systems.md) |
| Launcher UI, install/update, versions, platform paths | [Launcher](launcher.md) |
| Build/generate, test selection, isolation, CI | [Build and test](build-test.md) |
| Debugging, repeat searches, compaction, Sol handoff | [Investigation](investigation.md) |
| Sol requests for missing code, revisions, limits | [Evidence retrieval](evidence.md) |

See [the #105 run review](issue-105-review.md) for the evidence behind this map.
`manifest.json` maps changed source to guide ownership and selects advisory
excerpts. `patch-index.json` is generated from Git-indexed patch files using the
real loader. Refresh with `python tools/repo-context.py refresh`; validate with
`python tools/repo-context.py check`. See [maintenance](investigation.md#maintenance).
