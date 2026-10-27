# Subsystem entry points

See [systems-details](systems-details.md) for detailed adapter contracts and validation routes.

Story NPC replicas now construct at the host coordinates/heading, using the
host-resolved appearance variant rather than a second random choice. Reused
body generations rebuild local model/spatial state even when character kinds
match. Frozen poses publish once per actual simulation tick (formerly every
six ticks); duplicate calls within one tick still do not resend. Production
fixture coverage includes constructor placement and same-kind body replacement;
changed live visibility/pacing acceptance remains separate.

First-mission `pc_story_objectives.inc` publishes only numeric objective kinds
0/1 and status/flags, never kind2 executable VM records. Guests validate host,
match, sequence and authored identity before updating their Status data. The
original 227430 notification path runs once after the owning local player exits
script control. The original pause row factory uses 1.25 row scale for the
first-mission Story Options builder only, fitting added actions between header
and footer; other menus keep their original metrics.

Checkpoint support uses shared `pc_story_checkpoint.inc` and separate wrappers
for original25EF80 capture/25F010 restore. Reliable packets contain only the
authored checkpoint index and match/sequence; each peer calls its own engine
snapshot routines and retains local VM/object pointers. Added players use the
original25EAD0/25EC70 register ABI with their separately allocated bookmarks.
Restore scopes the current context to primary0 for the Local co-op world
snapshot, then restores the owning context. Checkpoint restart is host-only and
requires a matching captured mask on every receiving peer. The host pause row
uses the original checkpoint confirmation and stays disabled before capture.
Unit fixtures cover this contract; live checkpoint acceptance remains pending.

<!-- packet:start -->
Root tracked policy: `src/pc_input*`, `pc_controls*`, `pc_bindings.inc` for input;
`pc_display*`, `pc_settings*`, `pc_refresh*` for display/configuration;
`pc_menu_*`, `pc_ui_layout.h`, `pc_cutscene.h`, `pc_camera.h` for menus/camera;
`pc_debug_options.def` for the shared debug fields/keys/labels/capabilities;
`src/graphics/` for common shader/math policy; `src/native/` for host adapters.
For movement regressions, compare player-position displacement in game as well
as sampled stick values: the title attenuates diagonals after the binding
sampler. Windows mouse confinement is owned by `pc_controls.c`; native SDL
relative mode is owned by `src/native/input_sdl.c` and should recover from
focus changes only when gameplay capture was requested.
Patches connect these files to upstream; the generated guest files are not the
authoritative place to implement portable behavior.

Textures: root `src/texture_pack*`, `texture_mode*`, runtime `src/d3d/` and HLE
texture hooks; offline tooling `tools/texture-*`. Sanitized replacements now live
in `assets/textures` (DDS via Git LFS), audited by `tools/build_textures.py` for optional delivery;
private source art and original assets stay local. Networking: `src/pc_network*`,
`pc_online*`, runtime HLE/kernel patches and `tools/online.py`. Crash capture:
`src/pc_crash*`, `pc_hang_policy.h`, runtime crash hooks and launcher diagnostics.
Audio/movie timing is runtime `src/hle/hle_dsound*`, `hle_xmv*`, not launcher code.
`audio_output.h` owns `RECOMP_AUDIO_DEFAULT_GAIN` (0.5, about -6 dB).
XAudio applies it once to the master; SDL applies it once per stream. This
quiets the output at 100% without changing saved Music/Sound sliders, their
curves or relative mix. An explicit `RECOMP_AUDIO_GAIN` still overrides it.
Find the connecting patch by symbol/path using `tools/repo-context.py find`.
Consult `docs/shared-platform.md` for shared versus adapter responsibilities.
<!-- packet:end -->

The 2026-10-24 #74 material continuation adds 215 reviewed metal, masonry,
rock, plaster, carved, granular and wood replacements, bringing the optional
pack to 2,261 entries. Sanitized DDS/JSON and config/texture-pack.json change
together; source artwork, prompts and held drafts remain private. Square sources
are 1254 pixels, downsampled to 1024, 512 or 256; rectangular sources are
1774x887 or 887x1774, usually downsampled to 512/1024 on the long edge.
One1774x887 wood source is resampled to2048x1024 for a1024x512 original; this
is not native4K artwork. RGBA8 preserves
fine detail when DXT1 loses it. Narrow wood/metal sources are 2169/2170x725 or
2172x724 padded atlases. The private compiler extracts explicitly reviewed
regions with at most 1% aspect rounding, then emits exact original 4:1 or 8:1
outputs, up to 2048 pixels long. Full source dimensions, extraction coordinates
and resampling remain in private provenance; no native 4K claim.

The loader, key mapping, optional-delivery contract, host adapters and164original
character-icon exclusions remain unchanged. Asset batches use native DDS,
mip/aspect/alpha/hash, full-pack and decoded2x2 repeat checks. Original-scale
feature profiles supplement manual review; extrema can select different bevel
edges, so preserve initial measurements and follow-up evidence. These checks
cannot establish complete contour equivalence, scene adjacency or final lighting.

Hybrid production separates fixed macro layout from repainted microtexture.
Failed drafts stay held for specific UV, hardware, motif, tone or pattern defects.
Do not downsample defects away, infer approval from classification, or treat
whole-image brightness as proof of local fidelity. Native original references
are normally sufficient. Enlarged references and revised prompts have improved
some corrections, but those are asset-specific results, not a universal recipe.

Narrow source extraction only removes explicitly reviewed atlas padding within
the compiler's1%aspect tolerance; it cannot fix changed material geometry. Record
actual source dimensions, crop coordinates, resampling and output format. Keep
prior full artwork and prompt records for corrections. Confirm counts against
the original: mistaken prompt counts have occurred for grooves and grid cells.

Temporary outputs stay below256MiB. Retire only reproducible disposable reviews
or catalog-verified exported originals with resolved-parent and SHA checks;
preserve source art/provenance and never retry denied cleanup targets. One private
pack and small public LFS pointers avoid duplicate hydrated copies. Detailed
historical batch measurements, held reasons and storage audits remain in STATUS.md
and private hybrid-N validation records; coverage is summarized in docs/textures.md.
Scene testing remains deferred under the near-completion policy.

Recent batches preserve separate baked-lighting variants and record
original-scale joint, reflection and region-brightness samples alongside decoded
repeat review. Measured offsets, uncertain extrema and altered contrast remain
scene-validation concerns; these samples do not establish full contour parity.
Detailed batch298-302 acceptances, held reasons, measurements and storage receipts
remain in STATUS.md and private validation records. The latest batch adds three
1024-square RGBA8 masonry textures from1254-square sources. Runtime, optional
delivery and host-adapter contracts are unchanged.

Character effects: the shared US1260C0 gate in `pc_native_numeric.h` admits
positive fractional game time for player/NPC effect updates. Its children already
consume the float delta; dropping subticks delayed attached-effect timers.
Original mode and zero-time pause retain their behavior. See runtime guide and
the original-dispatcher fixture for tested versus stubbed nested coverage.

Arcade HUD panels: `pc_display_ui.inc` gives both shared-panel callers
US12EC8B (RANK) and12EA9B (SCORE) the left anchor. Their original compositor
retains panel widths, spacing and sliding/fading; mismatched anchors otherwise
overlap them on widescreen, including Original60. Each full panel flushes before
placement changes and restores its enclosing placement. Other panel callers,
4:3 and split-screen retain their prior path. Shared Windows/native Mac code;
the UI fixture tests both callers and guards, while Mac scenes remain unverified.

Reload input (#337): `pc_native_numeric.h` admits the original `1C2E80` reload/
weapon-mode dispatcher at compare site `1C2F09` whenever native game time advances.
Input edges follow raw whole ticks; pause can offset those from game whole ticks,
so the old gate dropped a valid press. Eligibility, binding lookup, animation and
ammo checks remain original; zero-time pause and Original60 remain unchanged.
The generic CMP hook is documented in runtime.md. No host-specific adapter.
Keyboard reload buffering additionally uses `pc_reload_input.h`, owned by
`PcInput`, and read-only US state snapshots in `pc_reload_guest.h`. Win32 and
SDL publish press/release events under their existing input locks, including
collapsed down/up events and fresh Fire clicks between samples. The shared
binding sampler applies action roles from configured keys to keyboard magnitudes
before merging physical-pad contributions. A short Reload request lasts at
least 34 ms and expires after 150 ms if the weapon never enters reload state 7.
Already-held Fire is suppressed until release, reload completion or rejection;
a fresh Fire press can interrupt. Monotonic press order preserves Fire after
Reload within one poll, including events with identical clock timestamps.
Context expires after 100 ms without an eligible
local-player camera update; menu/capture/focus/controller transitions clear it.
This does not queue rejected reloads indefinitely or alter ammo/animation logic.


`pc_model_transform_guest.inc` implements the US151E80 affine point fast path.
It checks disjoint/wrap-safe buffers and magnitudes at most2^33, then uses local
binary32 intermediates with explicit rounding, preserving final GPR/SSE state.
Overlap/nonfinite/large inputs use the unchanged generated body. Entry callbacks
precede all reads; no state persists across calls, no host adapter is needed.
`pc_model_transform_guest.c` includes the generated type header and builds once
for either host. Fallback NaN result payloads depend on host compiler allocation;
rejection itself preserves every guest bit and floating-point exception flag.

## Crash breadcrumbs and capture health

See [crash breadcrumbs and capture health](systems-details.md).

## Texture-upload timing diagnostic

See [texture-upload timing diagnostic](systems-details.md).

## State and data boundaries

`pc_story_menu.inc` inserts a Local/Online chooser only at the supported-US
main-menu Story row (return `0x002D0C8E`, ID 0, inherited callback). It uses the
existing menu dispatcher and guest layout/input helpers. Local opens the original
`0x005238E0` descriptor; Back restores the captured main menu. Four verified INT3
bytes `0x1201C..0x1201F` dispatch the chooser. `pc_online_connect_mode` selects
Arcade or Story for the companion; its mode becomes active only on bridge success
and resets on disconnect. The unchanged `pc_online_connect` remains Arcade.
Story Online currently opens the shared network lobby after directory connection;
Story-specific lobby contents/loading/replication remain pending. Native builds
share the chooser and Local route; Online reports unavailable pending MAC-013.
Callback fixtures and the optional checksum-verified US entry contract live in
`tests/test_quit_menu.py`; they do not prove runtime navigation or co-op gameplay.

For opt-in Story lobby diagnostics, `pc_display_menu_mouse_footer` honors
`TSFP_STORY_TRACE` only for a connected Windows Story session in lobby phase 11.
It reports numeric menu descriptor callbacks/footer fields once per descriptor;
it does not change lobby behavior. The measured host descriptor is `0x004E99C4`,
with builder `0x001A8DA0` and tick `0x001A8D30`. Its existing Options and character
actions still enter the ordinary multiplayer menus. Story lobby replacement is
pending; a connected bridge is not evidence of Story loading or replication.

The shared `graphics/present_metrics.h` recorder accepts optional
`TSFP_FRAME_PROFILE_DELAY_MS` (0..600000, default0; malformed values use0).
Only CSV rows are delayed from the first successful Present. Counters and the
last-success timestamp keep advancing, preserving skipped/failed gaps without
turning the startup delay into a false hitch. The 20,000-row cap is unchanged.
No delay environment lookup occurs without a profile path. Common RHI and native
GL share this behavior; native Mac execution remains pending (MAC-003).

`pc_auto_look_camera` keeps two fresh `PcGuestRegion` slots for its consecutive
current-player, player-record and control-setting accesses before the guest call.
It retains the global's region while visiting high-RAM player data, so settings
in the first region reuse validation. Bounded misses use `pc_guest_query_span`;
the complete 0x1504-byte player record remains validated. Windows checks its
pages in one bounded batch; Mach/full-region results may cover additional fields.
Read permissions cannot grant writes.
Snapshots are never consulted after guest execution or
retained for the next invocation. The original bit-3 save/restore and controller,
local-slot and menu rules are unchanged. `test_auto_look.py` checks host behavior
plus real multi-page records, split/read-only maps, expiry and uncached/persistent
mutations. Controller and menu paths retain their original preference behavior.

Online bot presentation (#222) uses `pc_network_bot_motion.h` from the
`pc_network_bots_guest_frame` hook. The transport still consumes each validated
host snapshot once. The guest caches it and updates the displayed body, spatial
bounds and matching animation frame times every frame over a 50 ms span, retargeting
from the current display pose when arrivals vary. Discrete vitals, scores, shots,
absence and spawn transitions retain host values; no guest AI or extrapolation
is enabled. A gap over 250 ms snaps to the next received host state. Match,
host, actor-pool and inactive-state changes clear presentation history. The
Windows QPC adapter supplies monotonic seconds independently of guest game time.
Human controller replication and the host's three-tick send interval are unchanged.
Native Mac fallback remains original until its network adapter exists (MAC-052).
Only clip-block `+8` (model `DC`/`134`) is interpolated within the same clip/cycle;
the other animation channels and crossfade controls stay atomic. US `5CEF0`
can promote the secondary clip during zero-advance evaluation, so mixing fade
duration/elapsed with another snapshot's clip identity can select an invalid clip.

The bot score-network adapter admits configured AI at US return site67868,
using EDI's actor slot in the original round score-limit scan. The stock
controller-flag filter otherwise skips AI despite valid scores. Limit/team
calculation and end-of-round authority remain original. Invalid slots, humans,
disabled networking and native Mac retain fallback (MAC-052). The supported-US
round routine fixture checks threshold decisions; the Online runner can require
both peers' completed Results records.

Camera and perspective corrections use `pc_display_memory.h` with a stack-local
region after the original guest routine returns; no guest calls, allocations or
protection changes may occur while it is reused. Bounds, overflow, read-only and
guard/no-access rejection remain enforced. `pc_display_perspective_projection`
does not re-query its output: supported US `151CF0` writes all 16 floats without
guest callbacks before returning. Weapon/preview correction therefore needs no
VM query; decals still validate their camera and source matrix afresh, independent
of the output's guest-stack region. `pc_guest_span.h` supplies a fresh Windows
resident-page query for known-size accesses spanning at most four OS pages.
Every page must be valid/readable; writes require all pages writable. Failed,
cold, guarded or larger spans fall back to the full-region query. Camera/source pairs can
reuse that snapshot; different pages need fresh queries. Darwin retains its Mach
region query. Unbounded string scans still use the full-region API. This avoids
Windows scanning a large mapped allocation to find its end on each display guard.
The general `pc_display_menu.c::guest_writable` also delegates to this adapter,
with a fresh zeroed snapshot on every call, including calls around guest callbacks.
HUD, viewports, menus and network display helpers retain their requested bounds;
spans beyond four pages fall back to full-region validation. Darwin uses real
Mach protection for these calls instead of the compatibility stub's synthetic
4 KiB writable extent. This does not cache matrices, reduce updates or change
projection arithmetic. `projection_output.c` checks the real
US builder's output extent, arithmetic, canaries and stack; generated C stays local.

Discord's `pc_display_menu.c::discord_guest_read` also uses a fresh local bounded
span for each requested read, including individual string bytes. No permission
snapshot survives the callback. Decoder fields, one-second sampling cadence,
event-name cache and IPC submission remain unchanged. Windows resident reads
avoid full-allocation queries; cold/large reads retain fallback and Mac retains
Mach validation. `test_discord_presence.py::test_guest_read_memory` executes the
production callback with real protection changes, exact copied bytes, rejected
reads leaving output intact, fallback and retained/full-query negative controls.

The native SDL input self-test ends its synthetic terminal context before
testing independent mouse capture/fire behavior. The fractional-terminal case
otherwise leaves `camera_input.terminal_active` set after its mock guest is freed,
suppressing the next case's Fire button. This is test isolation inside
`tsfp_native_input_test`; ordinary frame/input behavior is unchanged.
Its Confirm checks use the fixture's two active keyboard slots (E and Space),
then verify that its retained third entry (Return) stays inactive. The keyboard
profile parser deliberately caps bindings at two; the test must not require a
third binding or change the shared parser to satisfy its fixture.

`config/*.default.*` is tracked default configuration; `config/pc-controls.json`
and `config/pc-display.ini` are personal. `private/game` is the single extracted
game-data copy; `private/saves` is personal; `private/test-fixtures/unlocked` is
the canonical local fixture and must remain unchanged. Test runners copy it into
bounded disposable test roots. `private/texture-remaster/pack` is the one active
6-GiB-capped art-production pack; source art and original game assets stay ignored.
Sanitized compiled DDS/JSON replacements are tracked in `assets/textures` and
published as optional packs; `tools/texture-repository.py` stages them with LFS,
`tools/build_textures.py` audits them. Leave pointers unhydrated for unrelated
source work. Optional pack publication must reject missing LFS content. Ordinary
game packages omit textures; existing installed packs and runtime fallback remain.

For installed layouts, use the launcher platform adapter instead of assuming
checkout names. Never include private data, generated guest code, credentials,
environment dumps or personal configuration in durable context. Store paths,
symbols, semantics and sanitized measured conclusions instead.

## Prompt text layout read scopes

`pc_prompt_guest.inc` scans successive chunks through `pc_prompts_read_region`;
`pc_prompts_read_span` validates the complete argument frame before dereferencing
it. The bounded thread-local `pc_text_read_scope.h` cache uses fresh single-page
Windows queries with full-region fallback; Darwin retains its Mach query.
It is valid only during synchronous text layout, with no mapping/protection
changes or event dispatch. Nested scopes retain intervals; the next outer scope
clears them. Scratch writes still use a fresh protection query.
Cached intervals cover only the OS-validated page/region. Descending reads reuse
them only when the requested minimum fits; crossing argument frames query again.
Strings continue at the next validated chunk, preserving cross-page text. No
interval extends beyond its bounds or survives the outer layout scope.
The US outlined-label wrapper (`sub_00041E30`) retains this same scope over
its four outline draws and foreground pass, then expires it at return.
Scope wrapping, label construction/drawing and nested prompt measurement use the shared
hooks in `pc_tutorial_ui.inc`, `pc_display_ui.inc` and `pc_prompt_guest.inc`.
`pc_display_menu.c` includes `pc_prompts.h` before the .inc callers to avoid
implicit declarations. Do not extend caches across menu ticks or frames.
Tests cover region boundaries/expiry and actual prompt-hook scratch ABI/fallbacks.
Issue #105 also requires the runtime late-frame pacing patch; caching alone
measured 30 FPS. See STATUS.md for measured acceptance and Mac limits.

## Physical keyboard text entry (#136)

`src/pc_text_input.h` defines the shared bounded per-dialog text queue and edit
commands. Windows `pc_controls.c` supplies WM_CHAR/WM_UNICHAR; native Mac
`input_sdl.c` supplies SDL text events and starts/stops text input on its main
thread. Both adapters suppress keyboard menu bindings while a text owner is active
and discard queued text on focus loss or owner changes.

`src/pc_menu_text.inc`, included by `pc_menu_mouse_guest.inc`, recognizes live
character-row callbacks (`0x0007DB20`) rather than one dialog descriptor. Create
Profile embeds this keyboard in descriptor `0x004E8D54`; testing only standalone
`0x004D8D78` misses it. The callback-row scan reuses one writable OS region only within
that synchronous traversal (no guest calls), checking every row's bounds and
querying again on a region change. It expires before dispatch, including nested
menus; no row identity or permission survives a tick. Large non-keyboard menus
therefore avoid a VM query per row. `test_menu_text.py` checks descending links,
query counts, permission expiry, partial/invalid rows and cycles; eager-query and
stale-region mutations must fail. Windows/Mac use their existing VM adapters.
Original insertion/edit/Done routines retain selection replacement, limits and
save behavior. Escape queues the owning menu's
Back action, preserving embedded-menu callbacks. Numeric descriptor `0x004D94D4`
is excluded. Supported input follows the original Latin-1 tables; paste and
arbitrary Unicode are outside this change. See MAC-059 and the #136 STATUS entry
for physical Windows confirmation and pending Mac/dialog-matrix validation.

## Automatic display size

`pc_display_current()` is the selected/persisted mode: `0,0` retains console
resolution, `1,1` means Auto, and other valid dimensions are manual. New missing
settings default to Auto; existing settings are preserved. Render/camera/UI code
must use `pc_display_effective()` or the shape/scale helpers, never interpret
the Auto sentinel as physical pixels. The shared catalog appends Auto without
renumbering existing ratio groups. Guest Display settings and Cocoa expose it.

On Windows Auto reads the fullscreen window's nearest monitor desktop mode,
or its non-minimized client pixels in windowed mode. The SDL main thread
publishes drawable pixels via `pc_display_host_size`; the render thread reads
an atomic snapshot. Frame completion checks at most four times per second;
GPU allocation failure retains the last effective size and retries later.
Unsupported/empty sizes retain the last valid output (guest output before a window
exists); current supported limits remain landscape 640x480 through 7680x4320,
no wider than 4:1. Manual selection disables this tracking. Desktop resolution
and refresh rate are never changed.

## Crane viewport edge clipping (#221)

`pc_ui_vignette_clip` in `pc_ui_layout.h` converts the crane/remote-view
vignette's full 0,0..639,479 clip to host-exclusive 640,480 bounds only for
STRETCH tag `0x00137010`. Shared RHI `d3d8_GetScissor` and legacy GL
`native_apply_scissor` apply this policy before output scaling. Stored guest
clips, other tags and partial rectangles remain unchanged. The title helper
scope supplies the tag only for complete single-player non-4:3 views.

The pending Story startup attempts original D50E0 once for each context still
inactive at the first evaluator, rather than requiring the whole roster inactive.
A remote spawn may arrive before a guest reaches this boundary. Active contexts
are preserved, original ownership checks remain in D50E0, and the pending token
is consumed once so subsequent deaths never trigger initialization. Mixed-state
regression passed; the changed automatic-ready paired route reached [2,2] on
both peers with matched movement and reviewed weapons/HUD views. Host original
spawn correctly denied its remote context; guest initialized that own context.

A wrapped-row replacement experiment found that callback198E20 draws the roster
as well as Arcade details. Replacing the entire callback hid the roster despite
showing localized briefing. That experiment was reverted (saved ignored under
logs/story-briefing-experiment). Preserve198E20; replace only its lower details
callee1986A0 or an equally scoped drawing boundary. Header flags4/style11 also
failed to expose year/contrast and were reverted. Briefing remains unfinished.

The lower-details replacement now scopes original198E20 through a synthetic body
callback and intercepts only its1986A0 call. Existing40F00 word wrapping returns
a guest string, drawn by41DC0(x,y,text,color), with original font/fade services.
The roster still draws through original198E20. Paragraph placement remains a
runtime visual check; no source fixture can certify the final bounds.

The narrow briefing renderer retains both rosters live, but initial visual review
showed paragraph clipping. It now measures wrapped height through3FCF0, reduces
shared font scale only as needed within the lower panel and restores all font
state after drawing. The fitting correction awaits its changed visual check.

The fitted Scotland briefing was visually reviewed complete on both peers while
preserving roster/controls. Header readability uses the original briefing-menu
opacity branch during the synchronous heading draw, restoring descriptor after.
Revised notes now require the year (superseding the earlier optional-year
clarification); the later metadata renderer supplies it.
The heading opacity correction awaits runtime review alongside four-player work.

Four real instances now verified admission and matching host settings in one
Story lobby. Name opacity correction and complete fitted Scotland/TimeToSplit
briefings reviewed with roster intact. The year requirement was subsequently
restored in the revised notes. This does
not validate four-player level load or world/interaction/next-level authority.

## Mounted and CCTV gun mouse aim (#182)

`src/pc_mounted.h` handles guest player mode `0x80` before ordinary player
camera updates in both `pc_controls.c` and `native/input_sdl.c`. The US guest
`sub_0025D520`/`sub_00258A20` aim an AGUN through player entity data `+0xC14`;
ordinary player yaw/pitch do not turn it. The shared helper validates its AGUN
tag and reverse owner link, uses local input port zero (not guest player ID),
and updates gun pitch/yaw with authored limits, sensitivity, inversion, FOV and
optional gun speed. Mouse displacement is consumed once without frame-time
multiplication. Invalid ownership, pause, focus loss and stale motion consume
input without falling through to hidden player angles. Terminal mode `0x2000`
and its left-click mapping retain `pc_terminal_mouse` behavior. Controller
input remains in the guest routines. The separate vehicle-seat mode `0x8000`
is not routed by this helper. `tests/test_mounted_mouse.py` runs the guarded C
fixture on Windows/macOS; physical Mac validation is tracked under MAC-001.
The mounted helper receives the adapter's explicit native-timing selection.
Native mode accepts finite positive game-float time at `4E794C`; Original60
retains its whole-time gate at `4E7944`. Both consume/rebase mouse batches with
`pc_input_consume_motion`, preventing fresh packets from expiring against a
previous batch timestamp. Paused/invalid native time consumes without aiming.
The helper still handles only AGUN mode `0x80`, not vehicle-seat mode `0x8000`.
Host input self-tests route synthetic motion through `pc_controls_apply_mouse`
in Windows and SDL, with zero-whole positive time, paused stale whole time,
live return to Original60 and return to on-foot controls. Windows execution
passes; native SDL self-test execution remains a host validation requirement.

Both adapters now create a local `PcGuestRegion` after input replay/focus gating
and share it through terminal, mounted and ordinary camera `_in_region` helpers.
`pc_guest_memory_in_span` reuses bounds/permissions only during that host mouse
operation, which invokes no guest callbacks or allocation. Every subsequent
update queries afresh. `pc_camera_mouse_region` seeds the snapshot at the lower
of the player and FOV field using their known extents (0x118 and 4 bytes).
Misses use bounded resident-page queries on Windows, or the existing full-region
fallback/Mach query. Only an actual returned span may cover another field;
separate pages/maps get individual checks. Region changes and read-to-write checks retain OS validation;
read access cannot grant writes. Standalone helper entry points create their own
snapshot. This removes duplicate player queries without retaining memory facts
across frames or changing motion, capture, terminal clicks or timing policy.

## Player FOV through Zeppelin air lifts (#339)

`gameplay_fov_scale` in `src/pc_display_menu.c` recognizes grounded (2), falling
(4) and air-lift (0x800) movement. Original US `sub_00225C70` selects 0x800 in
Zeppelin's four lift cylinders and returns to 4 outside them. Both player-camera
callers retain the selected FOV; the projection and CPU frustum scale together.
The scene, camera-owner, story and letterbox guards still apply, and the guest
weapon lens is not rewritten. The shared hook needs no platform adapter.
`test_cutscene_letterbox.py` covers lift/ascent/fall/landing, excluded cameras and
a regression mutation. With the supported local XBE it also executes the original
lift/state routines and all four region records through the production camera
hook at default/90-degree FOV and three aspect ratios. The separate
`test_native_timing.py` player-force fixture checks original exit/landing motion.

## Unified Video Options and native presets (PR #285)

`pc_display_menu.c` stages resolution (including Auto), aspect, vertical FOV,
texture style, FPS and subtitles behind one dirty-state Apply. Auto uses the
`1,1` saved sentinel and is labeled Auto, never displayed as a pixel size.
Controls follows Video Options; conditional frontend Cheats is last. Audio
retains its frontend/pause-specific alignment and common highlights are rounded.
`pc_frame_rate_settings.h` persists the beta presets; live requests enter
`pc_frame_rate_live.inc` and commit at the guest clock boundary.

The merge retains main's nested text-read scope and fresh scratch-write checks
rather than the older per-hook stack-query optimization. The real-protection
fixture in `tests/pc_prompts.c` retains boundary/ABI checks; query reduction is
measured inside the cache by `tests/text_read_scope.c`, because reads now execute
in a separately compiled translation unit. Mounted mouse aim remains ahead of
ordinary camera input in both adapters; fractional terminal handling is retained.
Native high-refresh validation is MAC-060, distinct from crane MAC-058 and
text input MAC-059.

## Issue #74 menu texture additions

The branch pack now contains 2,046 accepted replacements. The same keyed DDS
files serve Windows and the common native Mac renderer; loader contracts are
unchanged. Menu coverage is 574/750 keys, with 164 original character icons, eight
intentional functional originals and four unresolved replacements.
`config/texture-originals.json` records excluded icon keys; the worklist rejects
installed replacements for those keys and hybrid planning retains the original
lane. Character model materials remain in scope. Source resampling and original-detail
recovery are distinguished from newly generated art in `docs/main-menu-textures.md`.
Offline acceptance does not certify mapped scene appearance or Mac GPU behavior.

The original-icon policy also includes historical public DDS/JSON hashes.
`build_textures.ensure_pack` rejects reintroduced icon keys and emits these
identities in `BUILD.json` under `textures.retired` for both game packagers.
The shared launcher retires only matching installed pairs transactionally;
custom artwork stays protected. See the launcher guide for recovery behavior.

## Native movement boost resource

The US walking routine `sub_00258F50` gates a movement boost on mode byte
`0x7DE456 == 0x11`, selected player `0x7DE498`, input bit `0x800`, and positive
entity+0x1D0 resource. The fixed subtraction at `0x259060` is now scoped through
`pc_native_boost_drain` in `pc_native_numeric.h`. Native frames split one active
original-update charge by `max(0, min(1, raw_delta, game_delta))`: slow motion
retains the original zero-whole-tick gate's reduced consumption; fast/catch-up
updates still consume at most one charge. Original mode and other sites retain
exact subtraction. No constant table, boost speed, threshold or stored clock
is changed. The complete US routine fixture uses a controlled flat collision
floor; it does not establish real-map geometry, input acceleration or network
transport parity. The game rule-name table at 0x4D1F90 + index*0x20, decoded
through the existing Discord string-table path, identifies index 17 as Escort.
A bounded frontend lookup confirms the name; live boost activation remains
unverified.

Escort recovery takes a separate route: active-match `sub_00068830` calls
`sub_000681C0`, then `sub_000654E0`. Its original game-clock delay is more than
five seconds; eligible match updates include zero-whole-game-tick frames.
`pc_native_boost_recover` therefore scales only addition site `0x6552D` by
`max(0,min(1,raw_delta))`. The original dispatcher gates pause; its selected-actor
write and mirror, threshold overshoot and health-dependent clamp are preserved.
The full three-routine fixture controls external services and validates those
boundaries separately from the walking drain's active-game-update budget.

## Experimental water coefficient precision

`pc_surface_coefficients` in `pc_surface_bias.h` preserves the current guest
uploader's clamped/truncated byte anchors, then scales acceleration/displacement
and exponentiates damping without repacking fractional values. The existing
bias+float+fractional opt-in gates enable it. `pc_native_surface_guest.h` copies
original parameters into the HLE state before temporary guest scaling; whole
steps retain original uploads. The HLE path copies bank 0 stages 3/5/7 and bank
1 stage 6 into ordered commands only for shader 0x4BBF10, caching duplicates and
clearing on scope exit. Defaults and original/catch-up behavior stay unchanged.
The constant-uploader fixture documents the current lifter's truncation; this
change does not claim original Xbox MXCSR rounding fidelity or full wave parity.

The opt-in precision policy now selects full-float format 0x4B for registered
feedback intermediates. Final byte textures, guest headers, rate gating and
normal-play defaults are unchanged. Common-RHI promotion/demotion preserves
live GPU contents and capture bytes; the transaction still flushes before copy,
rebinds only on success and retains the old resource on failure. This removes
measured half-float accumulation error but does not prove original wave parity.

Optional game packages use `build_textures.optional_identity` to retain only the
small `textures.retired` migration, without any DDS/JSON payload or LFS access.
This preserves the original-icon fix using the existing shared launcher consumer;
ordinary missing texture files never cause removal of an installed pack.

Merge with main (2026-10-22): the common button/lookup dispatcher retains Story
chooser and lobby interception alongside Audio, subtitle-row and settings
callbacks. Both sets of callbacks remain in the shared translation unit.

Revised Story notes require year, selected difficulty and population in the
lobby header. `story_lobby_status` uses the mission record's localized year ID
and actual occupied controller rows, with scoped small-font state below the
original title/thumbnail. It never substitutes profile completion for selected
difficulty. Trophy icons and changed runtime layout still need validation.

Story preserves authored active-character membership when the Arcade network
tick reconciles its player-only actor pool. Original3559A0 otherwise removes
every nonplayer from list74C37C/7A3580 via15BCC0 at355E9F. Death/script removal
callers remain unchanged. This preserves NPC membership, not NPC replication or
verified world authority; its live first-level effect still needs measurement.

The direct-load Story adapter now selects original stage1 and calls65940
without the frontend factory's unconditional stage0 reset, then records stage1
on each peer. Stage0 constructs an intro configuration;
changing its scene/flags afterward was insufficient gameplay initialization.
This corrects factory selection, while synchronized intro playback remains open.

Fresh four-instance Windows captures show Time to Split with localized 2401,
Normal and 4/4 Players, while keeping title/thumbnail/roster/briefing and host
footer. This validates header placement; trophy rendering remains unfinished.

Initial pending Story context spawns discard cached Arcade map pads before
calling original D50E0. Original CF060 can then choose authored Story starts.
Only still-inactive contexts are affected once per pending load; active remote
contexts and later deaths retain their existing handling. Runtime placement
requires fresh first-level captures and cross-peer positions.

Story character membership is now preserved during pending load as well as
active Story scenes. Restricting the prune guard to network phase15 allowed
newly initialized NPCs to be removed during loading. A separate eight-event
trace records phase/scene/pending/count; it does not alter character state.

Story GO now asks original65940 for stage0 and retains its cinematic scene,
movie ID and flags, including the original no-intro fallthrough. Received intro
selection is derived from original mission/movie metadata, while retaining the
host gameplay difficulty. Only host65C70 advances after the intro; its pending
config uses the original reliable network start handshake for all peers. The
manual context-spawn adapter runs only for gameplay stage1. These source changes
still require a changed four-peer runtime and duplicate-character visual check.

The first live intro reached original scene24 on all peers but remained
network phase10; the stock ready routine rejects cinematics, eventually causing
a peer to leave. Host/guest captures show the authored cinematic, not accepted
co-op readiness. The new cinematic-ready adapter runs only original transport
startup while skipping its gameplay actor loop, restoring all cinematic state
afterward. Long intro stability and the changed handoff still need validation.

## Frame-rate preset range

`pc_frame_rate_settings.h` and launcher `GameSettings` expose Original60 plus
120/180/240 Beta. Legacy144 sidecars load as120 without rewriting; direct144
environment timing remains for regression comparison. `pc_native_rate.h` accepts
180/240, and `pc_frame_rate_live.inc` publishes their immutable strings at the
next guest update. Save/apply rollback and the 60-Hz console clock are unchanged.
The overlay already labels every native target above60 Beta and displays actual
measured FPS rather than rounding it to the preset interval.

## Direct keyboard inventory slots

`pc_weapon_slots.def` appends host-only actions after the 24 Xbox controls;
`pc_weapon_slots.h` owns press edges and expiring requests. `pc_bindings.inc`
loads/saves them through the normal binding editor and migrates only missing
slots. Stock White=1 and Black=2 move to free Z/X keys; existing custom and
explicitly unbound actions remain intact. No migration writes until Save.
Both Win32 and SDL gate requests on focus, mouse capture, menus and text input.

The additive keyboard-weapon-slots patch wraps US `sub_001BF140`, the original
player weapon-input routine. `pc_weapon_slots_guest.inc` runs on that guest
thread after ordinary input, using the ordered table at `0x4FC888`, the original
variant filter `sub_001BAA70`, usability `sub_001BA410` and selection
`sub_001BA130`, then refreshes the weapon HUD with `sub_00142820`. It deduplicates
weapon groups, checks local input slot zero rather than network player ID, and
never grants inventory. Empty slots do nothing. Settings expose exactly ten bindable
slots, defaulting to 1–9/0. The separate 70-entry internal table bound covers
weapon IDs and variants; it does not add settings rows.

## Themed live keyboard and mouse editor

Controller selection preserves the launcher's keyboard binding map. Shared
`input_bindings.c` recognizes an explicitly selected pad with keyboard sources
and no pad sources, then merges pad-only defaults from that selected slot at
sampling time. Custom pad mappings (including unbound actions), entirely unbound
profiles and saved keyboard keys remain intact. Focus/modal gating precedes the
fallback; keyboard mode retains its existing first-free-pad sharing policy.

`pc_keyboard_menu.inc` is included by `pc_display_menu.c`, adding Keyboard and
Mouse to both main Settings and paused Options. It uses the original menu
builders, callbacks, highlights and footer navigation. The controller-style
bindings pages draw a keyboard/mouse diagram with primary/alternate columns;
`pc_keyboard_menu_model.h` partitions all 34 actions across short pages.
`PC_BINDING_SLOTS` is two in the shared model and native dialogs; old keyboard
profiles use their first two sources without modifying disk until saved.
Controller profiles keep their existing source capacity.

For gameplay stick axes, the shared binding sampler uses
`pc_input_radial_axis_magnitude` from `pc_input.c` on both left and right
sticks. It subtracts the configured deadzone once from the stick's radius,
then applies one gain to both components. Keyboard WASD still uses the existing
diagonal magnitude cap; stick halves bound to buttons retain their original
per-source threshold. This prevents a physical diagonal from losing speed in
movement or look while preserving custom bindings and both host adapters.

`pc_binding_capture.h` handles opening-input release, capture, final-input
release, Escape cancellation, focus loss and timeout. `pc_bindings.inc` owns
its lock/state; Windows and SDL supply raw physical keys and wheel pulses, latching press
events so quick taps are retained between polling ticks.
While capturing, controller polling and menu pointer dispatch are suppressed,
so E/F/Y and mouse clicks bind without activating menu commands. Assignments
use the existing atomic settings save/conflict API. Mouse sensitivity previews
live, debounces writes, flushes on exit, and restores previous values on error.

Capture starts once in the new page update. Native descriptor +12 is a per-frame
callback, not a destructor; installing capture cleanup there cancels it every
frame. The callback fixture in `tests/fov_settings.c` exercises both native
per-frame callbacks and requires a selectable capture instruction row.

## Host storage choices in Create Profile

`pc_storage_menu.inc` injects mounted host drive rows at native storage-button
site 001944C3, directly in Select Storage Device. Rows sort by drive letter on Windows and case-insensitive volume name on Mac;
Rows show only volume names and drive letters, with no game/save role suffix.
Native menu sizing is widened before its
title at 0019440B. Labels are ASCII-safe for the guest font. Windows lists mounted
drive letters; Mac lists the startup volume and /Volumes mounts. Neither lists
unmounted partitions. The native 00192770 update and 00191470 icon adapter keep
host indices away from the console device table. Selection delegates to original
00192E60 with console index zero, preserving the save/overwrite state machine.
After a drive change, synchronous 00024EC0/00190F70 enumeration rebuilds the native
profile list (74F9FC) before duplicate-name validation against the new location.

`pc_storage.inc` supplies actual volume metadata, cached five seconds for original
storage-text replacements. `pc_storage_routes.inc` validates a writable save
folder, prepares UserData/45410066 for the native free-space query, and atomically
saves save-location.txt beside RECOMP_INPUT_CONFIG. Changes immediately redirect
future UDATA opens through the registered kernel adapter; failed persistence
rolls that route back. Existing handles, caches and partition devices stay put.
Startup also honors the preference; profiles are neither moved nor deleted.
An unavailable configured location fails explicitly instead of silently
creating a new profile elsewhere. TSFP_SAVE_LOCATION_LOCK protects isolated test
sessions from changing real drive preferences. Windows resolves junctions and
volume identities; drive metadata retains executable-volume identity internally.
The Mac metadata adapter includes unistd.h before calling getattrlist; sys/attr.h
provides attribute structures but not that function declaration. The production
pc_prompts.c compile contract checks this include order in Mac CI.


Story/main merge (2026-10-23): Story chooser callbacks now use verified unused
US INT3 bytes 0x1237b..0x1237e, distinct from keyboard callbacks 0x1201c..0x1201f
and drive callbacks 0x12379..0x1237a. The shared button hook retains both Story
interception and drive-dialog handling. The common display fixture executes
both Story pose preservation and host storage tests after resolving duplicates.
Main's bot presentation helper remains alongside the separate Story replica path.
No new OS adapter is introduced; native Online remains limited by MAC-013.

## Experimental coupled water operator

`pc_surface_capture.h` defines the copied original-feedback snapshot and validates
the guest resource's input/first-pass/output indices and texture records.
`pc_surface_capture_guest.h` validates mapped memory, calls the original feedback
once, and submits eligible Scotland samples. `pc_surface_capture_hle.inc` reads
the existing RHI images at that point in the deferred command stream. It writes
at most 16 records to the explicitly requested `TSFP_SURFACE_CAPTURE` file,
normalizes BGRA to RGBA, and stops on unsupported identity/storage or I/O failure.
No image state is replaced. The older fractional/float experiments disable the
probe. Each TSWATER1 record contains 23 little-endian metadata words plus three
128-square RGBA8 images (input, first pass, second pass). GPU readback stalls
make these diagnostic runs unsuitable for performance claims. Common RHI is
supported by the source; legacy GL intentionally performs no capture.

`graphics/surface_coupled.h` computes a fractional power of the coupled affine
velocity/displacement step, anchored to the existing one-tick coefficients.
`graphics/surface_coupled_shader.h` supplies one shader body with vector-type
adapters. `graphics/surface_transform_shader.h` supplies separable transforms with an
explicit axis extent. `graphics/surface_basis.h` owns clamped DCT-II and wrapped
real Hartley weights and the corresponding diagonal-stencil eigenvalues, bounded
to 128 texels per axis. The captured game path uses 128x128 **wrap**, so the
earlier clamped DCT result is only a diagnostic. Texture-load/vector adapters
remain separate from shared spectral policy. The current
common-RHI fixture reuses basis/scratch resources and reconstructs the field
after each fractional update. The production owner/scheduler is described below.
Restoring coefficients belong to spatial eigenmodes; per-texel matrix powers
cannot replace the spatial transform. Preserve authored disturbances, smoothing
and byte storage. Historical float-only candidates and their rejected storage
comparisons remain in `docs/native-high-refresh-regressions.md`.
`graphics/surface_state_shader.h` encodes red scratch, green signed velocity plus
0.5, blue height and alpha height. Effect22 samples alpha cardinal differences;
constant alpha erases ripple slopes. The native final-view adapter packs this
height channel too.
`pc_surface_coupled_forced_step` stores the fractional response to a held forcing
input in each row's fourth component; `TSFP_SURFACE_FORCED_SHADER` applies that
input in the same spectral basis. The rounding fixture refreshes the measured
first-pass residual at authored boundaries. `surface_gradient_shader.h` supplies
the unclipped linear gradient: subtracting this from actual byte red retains
both clipping and rounding. Subtracting the already-clipped float shader output
loses the clipping contribution and fails strong initial impulses.
`surface_feedback_residual` also includes the second pass's signed acceleration
clamp after adding the fourth diagonal neighbor. `surface_limit_boundary`
applies final velocity bounds and the height cap before damping at authored
boundaries; it does not clip the internal state after every fractional piece.
`surface_rounding_shader.h` canonicalizes nearby values with original shader
arithmetic evaluated at that boundary, only within 32 float epsilons. It does
not predict byte thresholds: a hardware probe found an RTX transition away
from the ideal half-byte value. The earlier threshold-distance condition missed
that transition. Larger disagreements remain untouched and detectable; fractional
frames never use this authored-boundary correction.
The fixture requires byte initial/persistent state. No runtime scheduler/resource
owner calls this policy yet. The fixture covers mild/strong initial fields,
near-limit signed velocities/heights and
mixed-rate steps, including nonzero-phase switches; actual disturbance shaders,
full scene saturation coverage and live game scheduling remain acceptance work.
Original1956A0 dispatches disturbances1953B0, noise1952D0, smoothing1950B0,
feedback195110, then output195280. Noise effect19/2BE30 uses pixel4BBD30 and
vertex4BBBF8: immutable A8L8 noise adds into green (ONE/ONE, RGB writes) before
smoothing. Fixtures retain these shaders at authored boundaries with controlled
UV offsets; queued splash geometry and runtime RNG/event delivery need live
acceptance.
`graphics/surface_interval.h` splits exact host-side intervals into at most four
pieces (the existing maximum three-tick catch-up can cross four partial pieces).
Each piece marks authored start/end boundaries; zero elapsed time produces none.
It consumes `PcNativeStep` metadata without rounding float deltas or resetting
phase on rate changes. The GPU fixture uses this producer/splitter, including
a pause during motion. It is not yet a resource owner or an enabled game hook;
new resources and guest-local time overrides require explicit synchronization.
`graphics/surface_mode.h` packs each spatial mode's immutable eigenvalue and
fixed-point data into two float4s. `surface_evolve_shader.h` advances that mode
using current state, held residual and a delta/scale uniform. Unlike the earlier
fixture's per-delta coefficient-image cache, varying elapsed time requires only
a 16-byte buffer update and no coefficient-image allocation/upload. Real,
repeated and complex roots share this representation; CPU validation covers all
three and authored GPU trajectories use the complex branch.
`graphics/surface_gpu.h` now owns the coupled kernel's RHI images/views, shaders,
buffer and raster/depth/blend states. Create it on the render thread with fixed
coefficients/extent; begin from full single-mip float state/residual views, then
advance by each positive fractional piece. Zero time preserves output. Reset
invalidates the interval and the output accessor until a new begin; destroy before the device. Inputs
with incompatible storage/shape or hazardous internal aliases are rejected.
Creation failure unwinds every owned handle. It saves/restores output targets
and all viewports, including an empty list, and unbinds temporary textures and
its step buffer. It may leave other draw state changed under the RHI contract.
The owner executes the GPU fixture trajectories and the opt-in live GPU probe.
`pc_surface_gpu_begin_bytes` initializes from completed original input/first-pass
sampled byte views, retaining first-pass clipping/rounding and second-pass
acceleration saturation. It requires the authored center (128/255) and rejects
aliases or incompatible storage before drawing. Its unpack/residual shaders and
temporary residual image are owner-managed; ordinary float input remains supported.
Original disturbances/smoothing/byte boundaries remain caller-owned.
`graphics/surface_gpu_cache.h` provides a zero-initialized, render-thread registry
bounded to the constructor's five resources. It keys owners by resource ID,
generation, coefficients and four retained RHI image identities. Unchanged draws
reuse their owner without allocation; replacement is transactional and a full
registry returns NULL without evicting live entries. Clear before scene/device
reset and advance generation for in-place reinitialization. Invalid or failed
requests return NULL, never a stale owner. The opt-in native render hook now uses this registry.
The ten 128-square float4 images contain 2.5 MiB of pixels per owner; this is not
a total VRAM or in-game performance measurement.
`TSFP_SURFACE_GPU_CAPTURE=1` runs the shared kernel on actual GPU views and
writes fractional/full-tick fields to a `.gpu` sidecar. Its per-sample allocation
and readback are diagnostic; they do not replace visible water or alter guest
RNG/events. The persistent gameplay owner is separate. See the coupled-operator
record in `docs/native-high-refresh-regressions.md` for capture details/limits.

## Background replacement texture reads

`texture_pack_stream.h` owns a shared single-reader queue for common RHI and
legacy GL. The worker reads/validates DDS data without touching texture owners or
GPU resources. Submit/cancel/pump run on the renderer thread; release and content
changes cancel pending owners before reuse. A full 64-slot queue retries when the
texture is next bound. Two completed payloads and one read bound CPU staging to
72 MiB; the existing 512-MiB GPU budget and DDS/alpha rules still apply.

Original views remain usable during loading. Each Present pumps at most one
nonempty payload into GPU resources; F10 switches cached views immediately. The
GL adapter restores the previous texture binding after upload. Shutdown joins
the reader; normal cancellation never waits for disk. `TSFP_TEXTURE_STREAMING=0`
selects the synchronous diagnostic fallback. Per-texture messages now require
`TSFP_TEXTURE_PACK_TRACE=1`; explicit original dumping remains synchronous.

Classic now defers new replacement requests in both adapters (`pack_checked=3`).
Binding in Enhanced starts the deferred request; until completion the original
remains visible. Returning to Classic retains cached replacements and permits
already submitted requests to finish. Hash/content invalidation, cancellation
and explicit original dumps still run. Deferred bindings in Classic perform no
replacement I/O, GPU allocation or repeated trace writes. A `deferred` trace
records the initial hash only; late activation has zero hash/source-byte fields.

The common renderer pumps from present_resolve, shared by Swap and Present.
The initial Present-only prototype never delivered replacements in this title;
its apparent pacing gains are invalid evidence. Actual DDS upload records and
rendered Enhanced scenes are required alongside queue unit tests.

The in-game Video Options Apply path calls the explicit tsfp_texture_set_4k
setter. Unlike the host toggle, that setter does not emit a mode log line.
Reopening Video Options reads the effective mode; a pending row label alone is
not evidence that the change was applied. Texture selection is session-only.

Integration #308/#311: upload timing follows the queued request by value. Only
the renderer opens/writes/closes the bounded CSV; the worker stamps read times
in its exclusively owned record and publishes it under the queue lock. Completed
records measure hash, worker read/validation and renderer image/view creation.
Total duration includes queue/frame scheduling waits, not GPU completion.
Cancellation discards the record with the payload; a full queue logs its failed
attempt and starts a new record on retry. Disabled timing takes no clocks/files.

`pc_surface_native.h` schedules the persistent cosmetic field from exact native
clock metadata. It begins from post-event/smoothing byte input on authored updates,
then evolves the remainder and intervening fractional frames. This places the
cosmetic field one authored tick behind the original completed feedback, retaining
original event/RNG ordering. It does not add input latency to gameplay. Duplicate
frames do not advance; missing intervals, changed scopes and new identities wait
for an authored update. Pause preserves phase. Original60 suspends the clocks and invalidates field
readiness while retaining bounded GPU owners; constructor/device reset releases
them. Returning to native waits for an authored boundary before reusing an owner. `pc_surface_native_hle.inc` resolves copied full texture identities,
uses the five-entry retained-image cache, packs alpha-height through
`pc_surface_gpu_pack`, and borrows that view only during the original final shader.
No per-frame readback or queue flush belongs to this path. Older fractional/float/
live experiments are mutually exclusive. Native Mac common-RHI execution remains
unverified and legacy GL uses original output. Bounded Windows Scotland captures at 120/144 produce fresh final images on
partial frames; sustained pacing, broader visual acceptance and Mac remain open.

`pc_surface_gpu_cache_suspend` invalidates every output without freeing images,
shaders or retained identities. The deferred `hle_d3d8_surface_native_suspend`
clears the borrowed view and clocks, avoiding shader/resource recreation across
an Original60 round trip. Original60 still performs no coupled updates. Full
scene/device cleanup remains distinct. The creation log identifies actual
new owners; unchanged identity reuse emits no allocation notice.

Native water defaults on for common-RHI 120/144 beta presets; Original60 and
legacy GL remain original. `pc_surface_native_policy` accepts an explicit
`TSFP_SURFACE_NATIVE_TEST=0` recovery override and excludes old experiments.
The HLE adapter latches allocation/shader failure until constructor/device reset,
so fallback does not retry expensive setup each frame. Cleanup is registered
before first allocation; empty resets do not alter RHI texture bindings.

## Character status-effect model growth

`pc_native_character_guest.h` normalizes only scale-constructor call120BBF:
subdivide the one-raw-tick factor at current game speed. Original rotation,
composition, cleanup and whole/catch-up arithmetic remain intact. The original
helper fixture covers60..240, slow time, pause, jitter and live switches.

`TSFP_CHARACTER_TRACE` selects126029->120AB0: matrices/deltas/identity/removal,
validated mappings, cross-frame/scene rejection,8192rows/8MiB cap. Mapping caches
never span guest execution. Disabled/failed/capped traces keep original callee.
CPU evidence only; rendering/pacing and physical Mac remain unverified.

## Temporal Uplink map display

The shared HLE screen-copy hook saves Future Perfect's preclear map image
and supplies its map region to the handheld texture later in the frame.
Native GL implements the same texture-region transfer in
`src/native/gl_screen_copy.inc`, preserving framebuffer bindings and scissor
state. The map geometry and gameplay controls remain guest-owned; this adapter
only transfers the image that the game already drew.

## Forced and damped material scrolling

`pc_native_material.h` splits127500 velocity/travel in raw game time:
12765Bforce,1276FD/127705drag,12773B/127791UV. Weak drag uses stable limits;
Original/whole/catch-up/unsupported bases keep old math. Force/clamp, wrapping,
resources/events, layout and pause stay original. Varying-force/rendered/Mac
parity remain open; see surface/cache tests and STATUS.

## Scripted object clocks

TREE1D88B0 admits fractional time at its gate; EAX is overwritten before use.
Original phase/pose, events, terminal clamp, layout and whole updates remain.
TRAP1D8AB0 now couples fractional kicks/travel and carried-tick contact lifetime.
Original whole/catch-up and pause remain exact. See [script clocks](native-script-clocks.md)
for the map, hook composition, rejected candidates and live/Mac acceptance limits.

Statistics: D2CB7 scopes/restores D1FD0 float delta to carried ticks. Time totals
keep Original addition cadence; distance stays per-frame. D2079 blocks stale
ticks during pause. Shared helper adds no state/adapter; Original is unchanged.

Story/main integration (2026-10-26): preserved Story adapters with current main
timing/render/input contracts; 15 menu/Story/guest fixture passes,139 Online/
context/native timing passes, optimized Windows build/native checks completed.
The transition runner retains both scenario families and input publication.
Fresh four-peer acceptance and native Mac Online remain separate.

First-mission level reload uses pc_story_restart.inc on the original reliable Story channel. Only the host may initiate checkpoint0; guests validate host identity, current match and next reload epoch. Every peer calls original653B0 and resets NPC/loadout/initial-spawn state. The epoch changes pointer-free NPC/loadout identity to reject old-run packets. Original pause confirmation is retained; only the host receives the Restart Level row. Checkpoint restoration remains unsupported; no raw checkpoint buffers are transmitted. Native/non-Story fallback stays original.

A changed four-peer startup caught remote human teams2/4 after earlier normalization. The new D7570 return adapter now reapplies the existing validated body/context normalization immediately after the direct slot+1 initializer, preserving original return/register state. The frame fallback still covers construction before backlinks exist; NPC, Local and Arcade assignments remain original.

Story damage authority now runs before optional hit tracing in the shared damage wrapper. A guest replica never executes its own health/death/scripts, including incoming host events; a locally owned human instead sends the original35C870 reliable hit request. Replica identity validates match/entity/spawn independently of pose availability. Host damage remains original and drives snapshots. A matched Windows crash resolved to DC550 after guest-side NPC damage/death and a wild callback; the new guard removes that second simulation rather than suppressing the allocator failure.

Changed Windows four-peer acceptance (2026-10-26) passed opening, shared human
teams, owned movement/ammo, host-only level reload with fresh handoff, and
original host receipt/application of guest NPC hits through death. All runners
exited0 after the damage-authority correction. Full mission completion,
checkpoint/Game Over handling and native Mac/Internet validation remain separate.

First-mission camera/fade/pose and random-effect authority contracts are in
[systems-details.md](systems-details.md); pause layout uses original menu origin
with an11-unit host-only adjustment after original finalization.
