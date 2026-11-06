# Systems guide continuation

[Main guide](systems.md).

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

The shared HLE hook copies the preclear map into the handheld texture. Native
GL uses src/native/gl_screen_copy.inc and preserves framebuffer/scissor state.
Guest geometry and controls remain unchanged.

Temporary UV matrices: [render epoch and arena lifetime](render-matrix-cache.md).

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

Statistics: D2CB7 scopes D1FD0 to carried ticks. Time totals keep Original
cadence; distance stays per-frame. D2079 blocks paused stale ticks.
Impact sparks: see [native spark timing](native-impact-sparks.md).

[Devices](kernel-devices.md).

Project-owned I/O contracts: [sharing](kernel-sharing.md),
[request lifetime](request-lifetime.md), [driver calls](driver-call.md),
[transfer/queues/events/targets](transfer-completion.md),
[symbolic links](symbolic-links.md), [path routing](guest-paths.md) and
[host storage](host-storage.md).

Vehicle gunner mouse uses shared `src/input/pc_mounted.h` for mode `0x400`,
resolving the occupied seat and its gun on every update. Driver/passenger seats
retain their existing route. Vehicle limits and locked states differ from mode
`0x80`; see [mounted and vehicle mouse contracts](mounted-mouse.md). Both Windows
and SDL adapters consume this helper. Exact mission validation remains separate.

[Tank keyboard/mouse scope and original aim route](mounted-mouse.md).

Kernel: [pool](kernel-pool.md), [time](kernel-time.md), [RTL](kernel-rtl.md), [numbers](guest-numbers.md), [crashes](crash-evidence.md).

Online uses the build interpreter: [paths](multiplayer-ui.md).

Assault objective brackets (US142D10) use a tagged proportional placement.
`PcUiWorldAnchor` takes the complete first outline draw's horizontal midpoint;
all following labels retain that anchor until the next placement boundary.
The common renderer and legacy GL fit uniformly around this projected target,
not screen centre. This preserves both alignment and glyph/box proportions.
Guest flush boundaries isolate markers and restore the parent HUD. The existing
wide/single-player guards preserve 4:3 and split-screen. `test_ui_layout.py`
checks actual viewport scaling, target position, reset and seven display shapes.

Bot corpses: [contracts](systems-details.md#online-bot-corpses).

[Story version](systems-details.md#story-co-op-release-flag).

Physics: [poses](systems-details.md#guest-ragdolls-and-physical-npc-drops).

[Language startup](language-packs.md).

[2x texture workflow](original-upscale.md).

[RTL](guest-stack-trace.md), [host policy](execution-host.md).

Local player assignment: controls-options.md.


## Developer Tools sidebar presentation

The approved sidebar replaces the flat row view. `pc_developer_sidebar.inc` builds
one shared widget layout for painting and pointer/keyboard hit testing. Tools,
PC Settings and Diagnostics tabs have collapsible categories, global search,
editable numbers, +/- steps, speed slider/presets and dock-side controls. Key
bindings use an explicit draft/Save boundary; quick binding chips and mouse
settings save immediately through the existing atomic settings service. Invalid
numbers retain edit focus; Escape cancels. Ctrl+F searches, Ctrl+Tab changes tabs,
Tab/arrows move focus, Page Up/Down scroll. The fixed footer owns the warning
ending "Use with caution."; Extras retains Status, shortcut and Back (three rows).

`PcDeveloperLine` now carries value-only box/label/color/radius/vertical-clip
commands, capped at512. `src/graphics/pc_developer_canvas.h` rasterizes a bounded,
cached BGRA sidebar with shared geometry, clipping and alpha. Hosts supply only
font coverage and texture upload; Windows Segoe UI, Cocoa system font and Linux
sans-serif are used for32px sidebar glyph caches. Existing FPS/watermark fonts
are unchanged. The RHI draws the canvas on the final host target; legacy GL uses
the same canvas. No guest framebuffer or gameplay tool policy is duplicated.
Fullscreen status is queried through the Windows/SDL adapter. Dock and category
state are session-only; saves and unrelated preferences remain intact. Windows
visual acceptance and remaining native-host checks are recorded in STATUS.

Sidebar theme follow-up: accent, focused/selected controls and enabled switch
thumbs now use a blue palette (accent RGBA80BFFFFF). Geometry, input contracts,
warning colors and saved preferences are unchanged; the private HTML concept
uses the same palette. The final-target GPU fixture asserts the blue accent.

Sidebar refinement supersedes the height-based scale and fixed32px glyph path:
`pc_controls_developer_ui_scale` supplies host DPI (GetDpiForWindow / SDL display
scale), capped only to fit narrow windows. A452-unit panel keeps its size when
only height changes. Styled glyph coverage is cached by text, physical pixel
size and weight and copied1:1 into the canvas, with per-label alignment. Geometry
icons replace font-dependent symbols. Each category owns one continuous rounded
card underneath its header/rows; clipping retains its original corner geometry.
Compact bindings, centered presets, a joined number stepper and label-width tabs
follow the private reference. Status space is reserved only when needed. The
panel/shadow use straight alpha; native legacy GL composites the same canvas
with a state-preserving shader rather than an alpha-discarding framebuffer blit.
No gameplay or settings persistence contract changed.

The shared-font follow-up replaces per-OS sidebar text with an embedded native
font library. Selawik regular/semibold/bold and Cousine Bold are verified against
assets/ui/fonts/manifest.cmake and embedded by CMake; the unmodified stb rasterizer
parses only these trusted bytes. `pc_developer_font.c` measures UTF-8 advances,
kerning and ascent/descent using the same face/size as rasterization. Fractional
DPI sizes are preserved; canvas cache keys include size, weight and font face.
Text roles/weights are explicit at draw sites, numeric fields are monospaced,
and labels ellipsize at UTF-8 boundaries instead of cutting glyphs. Bind chips
use measured widths. The hold-speed header separates right-aligned shortcuts
from its semibold label; expanded rows have bracket chips and compact steppers.
Movement hints, units, punctuation and footer wording follow the reference;
real focus/offline/network availability still comes from the game state.
Counter/watermark host fonts and game text remain unchanged. No browser, runtime
font lookup or font files beside installed executables are required.

Sidebar control-detail follow-up: the shared canvas reset icon uses an open
upper-left circular arc with its arrowhead pointing left at the top endpoint
(counterclockwise), rather than up at the left endpoint (clockwise). Expanded
hold-speed steppers draw the same light-grey end-button fill as the main speed
stepper; their input/value contracts and platform adapters are unchanged.

### Video Settings inside the sidebar

Input Settings precedes Video Settings; both categories and their child groups
collapse independently. PC Settings no longer links Debug/Diagnostics. The Tools
speed presets are 0.50/1.00/2.00/4.00. `pc_video_sidebar.h` defines only copied
values, modes and request/result serials; the mailbox in `pc_developer.c` uses
the existing lock. `pc_video_sidebar_ui.inc` owns staged video fields, keyboard/
pointer selectors, typed FOV, slider, Apply/Revert and resolution Keep controls.
Input edits retain their existing immediate/save-binding policy.

`pc_display_camera` registers the game-thread video provider; the existing
`pc_developer_guest_frame` invokes it between guest frames. The provider lives
in `pc_video_sidebar.inc`, included by `pc_display_menu.c`, and uses the existing
display preview/save, frame-rate, graphics/post-AA, feed/counter, texture and
fullscreen APIs. Subtitle writes use the native profile and save flag. No guest
pointers or persistence/GPU calls run in sidebar painting. Resolution changes
need Keep within15 seconds; close or timeout restores the previous mode without
saving it. A failed settings batch rolls back completed writes and reports any
restoration failure. MSAA/supersampling retain restart requirements. Legacy GL
keeps unsupported effect settings and disables those editing controls.

Existing developer input and FOV fixtures cover category collapse, drafts,
selectors, FOV typing, mailbox serials, profile updates, save-failure rollback,
preview timeout/close/Keep and original guest Video Options. These suites are
already registered in runtime-patches CI. Native Mac/Linux acceptance is separate.

Aspect groups reuse pc_display_modes.inc; frame-rate labels/values share the
persistence-free pc_frame_rate_choices.h with the original Video Options.
Changing aspect selects the nearest matching render mode as a draft.

### Host-controlled Tools access

pc_developer_enabled reads the persisted local opt-in, default false. Extras
and launcher both edit it; Extras has no warning row. pc_developer_match_policy applies session policy and resets flight
state on revocation; getters/shortcuts and sidebar widgets share that boundary.
Offline tools still work normally. Cheats and time scaling retain independent
offline guards; local camera ownership can be used online only with host access.

pc_developer_match_menu.inc appends native choice121 to US2C78F0 through its
existing pc_network_bots_menu wrapper, before the outer2D5E70 menu finalizes
layout. Native71CF0 dispatches12390 on left/right changes. Only the current host
and owning descriptor can change the session preference. No guest configuration
padding or personal save is repurposed. The existing lobby-name hook wrappers
now pass through, preserving original text with no decoration.

pc_developer_lobby_guest polls both lobby and match rosters, passing only values
to pc_network_tools_policy. TSFTOOL1 uses the existing LAN/Online compatibility
envelope, host session key, authoritative host address and monotonically
sequenced updates. Wrong sender/key, old/duplicate sequence, malformed values
and missing host are ignored; clients expire the permission after3 seconds.
For locally opted-in users, Settings/Diagnostics remain usable regardless of Tools permission. UI state
and local saved JSON never grant match permission.

Existing input/guest/FOV/Unlock and real-socket transport fixtures cover local opt-in
access, restored warning-free Extras and removed icon entries, host-only choice callback, stale descriptor,
online feature restrictions, revocation, sequence/session/host checks and LAN /
Online framing. Shared launcher tests check opt-in Save state, persistence and preserved shortcut
with assigned-key launcher guidance. Native host acceptance remains separate.

The Bot Set adapter carries the real host mode through nested row construction;
only the original builders see Arcade mode9. The Tools row is appended with the
real host mode restored, then the enclosing adapter resumes its temporary mode.
The native menu fixture covers this nested call and restores both contexts.

Match Tools selector placement: the existing79D60 wrapper inserts choice121
before native Music at US caller2C8010, while compact22100 typography/spacing
is active. The2C78F0 wrapper scopes menu ownership and the real host mode; it
no longer appends after Go. The native Music builder then resumes its own
selector context. No generated-code or patch edits are needed.

Visibility switches HUD & UI and Version watermark now live in
Settings > Video Settings > View & HUD, with no Tools HUD category. These remain
immediate session settings (not staged video drafts), usable while the host
disables Tools. Host policy polling preserves explicit HUD/watermark choices;
revoking an active free camera still restores its automatically hidden HUD.
The input fixture checks placement and live updates under denied host policy.

View & HUD visibility labels are HUD & UI and Version watermark (no Show prefix);
this wording-only refinement leaves immediate session semantics unchanged.
