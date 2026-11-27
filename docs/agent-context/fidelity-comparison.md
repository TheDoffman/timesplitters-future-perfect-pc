# Sun and visibility repair

The optional Fidelity Comparison preset was removed at the user's request.
There is no comparison menu, startup mode, settings override or pc-fidelity.ini
reader. Existing sidecar files are ignored; saved display/audio/controls keep
their normal behavior. The independent sun/glow and allocation fixes remain.
Historical preset captures below establish the repair's original evidence,
not a feature still present in the current build. New checks use normal settings.

## Guest draw query scopes (mansion lighting, November26)

A title visibility query must count guest geometry only. Device preparation can
run feedback/snapshot draws with stale fullscreen state; counting those made
What Lies Below's Temporal Uplink light return1,024,170 pixels instead of170.
The original light code converted that to intensity45734 instead of14 and
corona halfsize8865 instead of7. High intensity bits also contaminated RGB.
This was host query contamination, not authored attenuation or FOV metadata.

`visibility_draw_scope.h` splits one logical query into at most64 reusable native
segments, beginning/ending only around submitted guest geometry. It sums exact
samples asynchronously; empty scopes are zero, capacity/overflow/native failure
is an error, and pending/error preserves the caller's output. The existing
visibility cache and target/MSAA normalization remain authoritative. Reset releases
all segments before device destruction. No brightness/size clamp is introduced.
`visibility_rhi_device.inc` supplies RHI callbacks; shared late patch3064 brackets
all four D3D8 draw submissions after feedback/shader/state preparation. Legacy
`gl_visibility.inc` and renderer_gl.c use the same policy around actual GL draws.

`test_visibility_queries.py` compiles the actual RHI adapter with host feedback
spies: old scope, missing begin and missing end controls must fail. It covers
multi-draw preparation, asynchronous readiness, cached/empty answers, capacity
and cleanup. `rhi_visibility.h` also submits host triangles before/after one
counted triangle at samples1/4 and requires4096*samples, not12288*samples.
Native Mac Clang and Linux GCC/Clang pass the portable scope/removal/GL adapter
fixtures. These CPU checks do not certify native Mac/Linux gameplay. Live Windows
mission captures and final build acceptance are recorded in STATUS. Corona
placement separately uses [full-view scopes](hud-sizing.md#world-light-coronas).

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
writes. The original acceptance used pinned Vulkan headers; subsequent Windows Vulkan
GPU scope checks are recorded above. Native Mac gameplay remains pending. Windows immediate and deferred140s mission runs both return
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
