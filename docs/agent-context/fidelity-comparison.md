# Sun and visibility repair

The optional Fidelity Comparison preset was removed at the user's request.
There is no comparison menu, startup mode, settings override or pc-fidelity.ini
reader. Existing sidecar files are ignored; saved display/audio/controls keep
their normal behavior. The independent sun/glow and allocation fixes remain.
Historical preset captures below establish the repair's original evidence,
not a feature still present in the current build. New checks use normal settings.

## Query availability contract

The shared Vulkan adapter now maps `vkGetQueryPoolResults`'s `VK_NOT_READY` to
pending0, retaining the logical scope for a later poll and preserving the caller's
answer. It still maps real errors and aggregate overflow to-1, returns zero as a
valid ready answer, and uses nonblocking64-bit retrieval. Timeline completion
does not authorize treating a pending result as a failed visibility scope.
The unchanged production adapter fails the deterministic native fixture at this
pending-result assertion; the corrected adapter retries to the final exact sum.
`visibility_vulkan.c` compiles the actual shared include against explicit API
spies on each host and also checks partial writes, queue pending, device failure,
zero/empty scopes, overflow, capacity exhaustion and serial-based retirement.
This fixes an adapter result-mapping bug. It does not repair MoltenVK's separate
callback ownership or establish that the historical game crash is fixed.
[API semantics](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetQueryPoolResults.html).

## Query lifetime crash investigation

The retained mod/audio game crash PC is in MoltenVK's finishQueries completion
block. The primary Mac SDK VERSIONS.txt identifies MoltenVK source
`db66022459ffb663aa2b50f6b018bc2e124f5edf`. That source tracks query-pool pointers
without retaining them for the Metal completion callback. Upstream fixes that
ownership in [c0af3a56](https://github.com/KhronosGroup/MoltenVK/commit/c0af3a5658ba03b6006fdeb14efd1b21b2ad91b2).
This matches the recorded stack pattern; it does not establish the historical
game crash's cause or a tested game repair. The SDK pin/installed driver and
normal renderer behavior are unchanged while the candidate is investigated.

`tests/graphics/vulkan_query_lifetime.c` is an asset-free native Vulkan1.2
diagnostic: record an occlusion-pool reset, submit with a timeline signal, wait
for that signal, then destroy the pool. Repeated ordinary allocator reuse makes
an illegal late driver callback observable. It never touches freed application
memory and does not add a queue-idle wait before each retirement. A final device
idle drains cleanup. No shaders, game/title addresses, surfaces or saves are used.

`tools/testing/test-vulkan-query-lifetime.py --output NEW_DIRECTORY` builds and
observes up to8 independent processes with bounded iterations/deadlines. Native
exit0 alone cannot pass: exact completed counts, scope and timeline evidence are
required. Crash, timeout, malformed/partial output and unsupported device are
distinct failed observations. New-only output preserves existing files, and a
2GiB staging floor is checked before SDK setup/compilation/output creation.
Mac uses the verified shared SDK (`--setup-sdk` or `--sdk-source-root`); Linux
uses system Vulkan. An explicit candidate library requires its independently
recorded SHA256 and an isolated generated ICD; it does not replace an SDK file
or normal game driver selection.

The separate `mod-query-lifetime.yml` diagnostic workflow runs the pinned Mac
SDK without game assets and retains all observations. Its explicit
`--record-only` means a completed diagnostic job can contain failed native
probes: inspect report.json, never equate the workflow result with a fixed crash.
Normal graphics/runtime regression jobs remain separate. Patched-driver A/B,
physical Mac game replay and final release acceptance remain required.

## Visibility repair (Windows runtime acceptance passed)

Patch3001 preserves original SDK Begin3D4250 and End3D42F0 bodies as `_gen`
wrappers. pc_display_menu.c includes visibility_guest.inc; visibility_queue.inc
keeps Begin/End in the same immediate/deferred stream as their draws, with a
copied End payload and no early flush or extra-frame replay. End retains the
original index argument, HRESULT and allocation result. HLE GetResult still
runs the original first and respects RECOMP_PB_EXEC; only an incomplete result
may use the host answer. Both original17E90 calls pass null timestamps (17F01,
17F6F). Timestamp callers retain the original/fallback path; no GPU time is
invented. Guest output is validated, and pending/error cannot overwrite it. Bounded
diagnostics retain the first positive/zero samples plus at most32 periodic
results (one per300 polls), without fabricating GPU timestamps.

visibility_queries.h owns the0x4000-ID cache over32 native objects. Completed
answers survive native-object reuse; zero is a valid ready answer. Begin reaps
abandoned completed work; ID reuse invalidates old data even if new Begin fails.
Failed original End retires its native object without publishing an answer.
Reset releases objects before device destruction. Scale/MSAA normalization uses
actual target backing scale and draw-surface samples, preserving guest pixel
area at higher resolution. Query scopes are the original title's world probes;
arbitrary multi-target SDK scopes and GPU timestamps are not certified.

RHI exposes asynchronous exact-sample queries. D3D11 uses OCCLUSION/GetData;
Vulkan enables precise occlusion when supported and brackets each draw inside
its rendering instance, sums segments, and retains pools until their submission
serial completes. Unsupported capacity/features fail without fabricating counts.
The GL adapter uses SAMPLES_PASSED under the existing context scope. Both hosts
share the cache policy. RHI and GL reset paths release their query objects.

Asset-free WARP fixtures pass wall0, visible samples, alpha cutouts, multiple
draws, reuse and MSAA1/4. A native manager fixture covers pending/error/zero,
cache/ID reuse, abandoned work, capacity, normalization and teardown. Guest
callback tests preserve original stack/HRESULT and reject timestamp/invalid
writes. Vulkan source compiles with pinned headers; real Vulkan/GL/Mac execution
remains pending. Windows immediate and deferred140s mission runs both return
50 samples with the sun visible and0 behind the cave roof. An additional
stationary run waits for tutorial panels to finish and shows the authored
bright white/yellow sun and glow. All seven preference files remain unchanged.

The temporarily gray sun centre is an original tutorial shadow, not a colour
conversion defect: owned D3DCAP replay has374 draws, no failed/malformed/unknown
calls; draw278 produces white255, while tutorial-shadow draw349 dims it to127.
The unobscured first-person capture retains the bright sun. Do not add gamma,
bloom, blend or texture changes to compensate for that authored overlay.

Local evidence is under logs/fidelity-sun: final-immediate, final-deferred,
final-unobscured (frame012 is unobscured first person), replay and ui-{enable,
restore,verify}. UI result byte comparisons are authoritative; enable's frame
list includes older files from a timed-out attempt and is not UI image evidence.
The final query-enabled EXE SHA256 is
2d318b0b642cfaccff6ebd1f0dea82ab8667f8804f6926ebc38373f5dbb75cb7.
These are controlled opening-mission checks, not whole-game or original-hardware
certification. Assets and gameplay were not altered.

Validation update:49 combined Windows native contracts pass with2 native-Mac
skips. The production GL adapter is compiled against API spies (context entry,
pending/zero counts and area scaling); this is not a GL GPU execution result.
The original-body registration check runs the real C-source scanner against
both SDK wrappers. `_gen` declarations must each be separately discoverable;
otherwise the generator omits the retained body and the final link fails.
Before preset removal, stable patch transitions passed3 tests and repository context tests passed21.
The Windows Release game and debug fixture link successfully. Whole-function
512KiB translation partitions avoid MSVC C1002 without changing guest bodies.

Removal validation: Windows Release game and debug fixture build successfully.
54 targeted checks pass with2 native-Mac skips (visibility, original menu/FOV,
graphics, textures, audio and repository context). Patch removal and repeated
application are verified by the official helper; no runtime files change on
the repeated check. No preset symbols or config reader remain.

Local preview0.6.3-sun-fix.1 packages the post-removal Windows build with
launcher1.6.2, source/licenses and matching symbols; all1529 file hashes pass.
Its isolated140s packaged mission run retains seven preference files and uses
1280x720/FOV73 despite an obsolete comparison flag. Wider-FOV roof-edge geometry
keeps the sun visible here; do not claim this run repeats full-roof occlusion.
The package has no game data; the local playable stage links the existing copy.

Main integration with5426485f preserves the newer crosshair, weapon-FOV and
tutorial-layout work. Integrated Windows Release/debug build passes;42 targeted
visibility/FOV/RHI/repository/compiler checks pass with2 native-Mac skips.
