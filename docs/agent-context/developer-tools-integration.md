# Developer Tools integration notes

Current widget, video and host-policy contracts continue in [systems continuation](systems-continuation.md).
The sections below retain the Developer Tools branch history while the owning
guides also track newer main work. Older patch counts describe their revisions.


## build-test integration 1

Shared-font tests add real regular/semibold glyph coverage, fractional metrics,
monospace digit advances and UTF-8-safe ellipsis. Standalone input/font fixtures
run the same CMake embedding helper as the supported build, then link the shared
font C source. Existing final-target/MSAA and layout fixtures remain applicable.
Patch tail ends2620 (89 ordered entries). Package scripts retain both font OFLs
and stb's MIT license on Windows, macOS and Linux; source checks do not establish
native host packaging or rendering acceptance.

Sidebar refinement checks add height-independent panel sizing,1.5x DPI,
size/weight glyph-cache keys and geometry-icon coverage. The host-contract fixture
checks styled callback forwarding, invalid size/failure and output preservation;
CoreText coverage fixture adds requested-size semibold text (macOS only).
Patch order ends2610, with an88-patch tail. Live visual comparisons use a bounded
isolated Test Lab copy at the mockup's DPI-adjusted size and1280x720/1920x1080.

Sidebar regression coverage supersedes the old flat-row navigation expectations.
`tests/input/test_developer_tools.py` compiles real shared widgets against isolated
settings/input boundaries: pointer actions, search, validated numeric values,
binding draft/save/discard, focus and offline guards. Its developer_canvas fixture
checks rounded boxes, clipping, BGRA colors/alpha, glyph reuse and invalid geometry.
`tests/storage/test_unlock_content.py` requires three Extras rows and no warning.
`tests/graphics/test_native_hud.py` checks actual D3D11 final-target text and sidebar
blue accent RGB128/191/255 at1x/4x MSAA without leaking host UI into guest captures. These tests remain
in the existing runtime-patches CI test routes. Patch order now ends2600 (87-tail).
Native Mac/Linux build, font and visual acceptance remain separate unrun checks.

Latest notes: input regression toggles audio scaling at held3x, requires normal
sound with unchanged game speed, then restores scaled sound. Real XMV fixtures
exercise an independent audio toggle while video advances2x. Native Extras
fixture now requires three rows without the sidebar warning. Patch order ends2600.

Further notes regression: audio output fixtures verify live gameplay ratios,
unchanged movie queues and independent media-clock domains. Stream timing exercises
0.5x/2x completion through the public tick. Input checks flight hotkeys, network
reset and the registered audio callback; native Extras fixture opens its own
Enable/Disable submenu with warning and configurable shortcut. Lobby guest fixture
checks the actual41E30 outlined-name caller, float coordinates, slot ownership
and decorated-name measurement without modifying names. The submenu fixture
requires empty lifecycle slots. Launcher checks unbound flight defaults and both persisted binding slots.

Revised Developer Tools regression additions: original-XBE98E50 fixture proves
that the fractional native cinematic branch follows game delta while the real
clock branch ignores it; guest fixture checks scoped flag restoration/origin
rebasing. Shared launcher UI tests change only Developer Tools ON/OFF and verify
Save draft state, persistence and the F3 warning. Native host acceptance remains
separate from Windows fixture/build evidence.

Revised Developer Tools checks: input fixtures cover disabled defaults,
reassigned shortcut/warning and closed-overlay lobby notices; unlock-content
fixtures execute the actual Extras selector tick. Both UDP LAN and Online
bridge fixtures deliver TSFDEV1 frames with native capacity=1, validate session
keys, availability changes and roster removal. The real bridge also tests an
admitted guest without an XNADDR entry and retirement with stale self-discovery. The native name getter adapter
is exercised separately. Movie fixtures integrate rate changes without clock
jumps and both real audio adapters preserve their queued voice/stream on rate
changes. Windows launcher tests round-trip enablement and the shared binding.

Developer Tools follow-up fixtures check remapped flight and native look signs,
held-speed restoration/opt-in, hidden reticles, local damage/ammo and owned-body matrix preparation/renderer guards.
Original A02F9 inclusion and A1BF1 alternate-mesh branches prevent reversed
visibility-mask assumptions; original 1B66E0 tests retain shot accounting.
Developer Tools: `tests/input/test_developer_tools.py` compiles deterministic
input/settings and guest-camera/checkpoint fixtures. Its private-XBE case runs
original US25EC30 to prove authored checkpoint-index destination lookup, rather
than a recorded player position. Native timing fixtures cover speed changes
and pause across 60/120/180/240 schedules. Windows settings integration checks
overlay visibility and preserved legacy menu-bar JSON without attaching a bar.
CPU fixtures do not establish geometry bypass, freecam visibility, authored
Story restoration/ending movies, or native Mac/Linux gameplay; see STATUS.

[Input tests](controls-options.md).
[Tab queue](../development/graphics-options.md): HUD/SMAA order and texture-present fixtures.
Auto-sizing fixtures test SDL policy; restart handoff is separate.


## build-test integration 2

Local opt-in Tools / match policy extends existing developer, FOV, Unlock and real
network transport suites. UDP checks cover LAN/Online envelopes, host key/address,
sequence/replay, expiry and session reset; FOV's native-menu fixture checks host
selector insertion and stale descriptor rejection. These are existing CI suites.
Shared launcher -Test covers restored opt-in persistence, Save state and saved shortcut preservation.


## launcher integration 1

Developer Tools retains the local opt-in checkbox and persisted
developer_tools_enabled boolean (default false). Changing the checkbox alone
enables Save; enabling displays assigned-shortcut guidance with "Use with caution."
Extras uses the same setting without a warning. Multiplayer host Tools permission
is an additional gate; it does not disable Settings/Diagnostics for opted-in users.
Shared model/UI tests cover opt-in persistence and shortcut preservation.

Flight shortcuts append developer_noclip/developer_freecam as actions35/36;
PC_ACTION_COUNT is37. Both default unbound and use the existing two-slot conflict,
move/unbind and persistence paths. The Tools shortcut remains action34/F1.

Developer Tools opt-in participates in the shared settings draft and checked
change notification, so changing only that switch enables Save. Shared UI
regression covers the assigned F3 warning and ON/OFF persistence.

Developer Tools: `GameSettings.DeveloperToolsEnabled` reads/writes the root
`developer_tools_enabled` boolean in controls JSON, default false. The shared
General page has an opt-in checkbox and assigned-shortcut warning. Its host
binding follows weapon slots in Actions/Labels, defaults F1 only when missing,
and participates in ordinary conflict/move/unbind/save behavior. Runtime and
launcher preserve the same key; native Keyboard & Mouse exposes it in Menu
actions. Enabling does not grant network gameplay cheats; lobbies show availability.


## runtime integration 1

Developer Tools match policy (after cf14ea41) is implemented in shared source;
existing guest-frame and2C78F0 Options builder wrappers are reused. No new runtime
patch or generated guest edits are needed. Legacy name/text wrappers now pass
through without a wrench; the native Extras builder retains its Tools opt-in entry without a warning.

Patch2620 follows2610. Runtime input/d3d CMake owners link one shared
`tsfp_developer_font` target, and the RHI canvas uses its bitmap callback.
Legacy GL uses the same implementation. The older optional styled host callback
remains available but is no longer the sidebar font source. Configure-time
embedding verifies font hashes and runs without Python/network downloads.
No original executable, wrapper signature or generated guest code is edited.

Patch2610 follows2600. It adds optional RhiHost.text_bitmap_styled and
rhi_text_bitmap_styled(size,weight) for final-size sidebar text. Windows uses
UTF-8 GDI Segoe UI grayscale coverage; SDL forwards to Cocoa/FreeType adapters.
Existing counter/watermark callbacks remain unchanged. The overlay canvas now
uses the styled callback. No guest wrapper or generated C change is required.

Patch2600 follows2590 and replaces per-row developer text textures with the shared
BGRA sidebar canvas. The overlay shader's size.z selects full RGBA for this canvas;
ordinary counter/watermark coverage compositing is unchanged. The canvas draws
only after scene resolve/post-processing and stays outside guest captures. RHI
Windows font coverage selects Segoe UI for32px sidebar glyphs. No guest wrappers
or HLE declarations change; no generated C edit/regeneration is required.

Patch2590 follows2580 and separates XMV audio speed from its video clock using
PC_MOVIE_AUDIO_SPEED. Shared Tools supplies pc_developer_audio_speed; non-Tools
builds retain their prior rate fallback. No guest wrappers/declarations change,
so generation is unchanged and an incremental native build is sufficient.

Further notes add patches2560/2570/2580 after2550. 2560 makes all gameplay
voices pitch-enabled and updates their existing ratio through a registered overlay
callback; movie voices keep their separate movie ratio. 2570 advances buffer and
stream scheduling through two independent scaled media clocks (QPC/tick origins),
including SDL fallback completion. 2580 wraps native353760 and existing41E30 for lobby name
capture and a guarded wrench draw; guest regeneration is required. Shared native
menu/body changes remain in source, not generated C.

Patches2540/2550 follow2530. 2540 appends a host-only binding to runtime controls;
it is skipped by Xbox pad synthesis. 2550 integrates movie time at current
Developer Tools speed and enables movie-only XAudio frequency changes; SDL
uses the same rate contract. 2560 supersedes the original ordinary-voice pitch flags.
2550 also wraps native98E50 to use its fractional cinematic position path,
scoping the borrowed flag after99990 clamps debug seeking and rebasing its
real-clock origin for a smooth Tools-disable transition. Regenerate guest C.
Patch2530 follows2520: local damage/ammo, detached world masks and viewport draw hooks.
Patch2520 follows final Linux adapters: Developer Tools input/UI, CCAB0 wrapper,
HUD draw scopes and HLE-owned renderer callbacks. See systems.md; regenerate guest code.

[Linux](linux-game.md).
Counter2490: overlay sizing.
[Kill feed contracts](../development/arcade-kill-feed.md).
Story rewards: additive2465 wraps356800 for the single frontend context;


## systems integration 1

## Current Developer Tools access policy

The overlay uses the persisted developer_tools_enabled local opt-in (default
false), controlled by Extras > Developer Tools or the launcher. Extras contains
Status, assigned shortcut and Back with no warning row or confirmation. The
sidebar warning remains. Native roster names have no wrench decoration. Once
locally enabled, Settings and Diagnostics stay available online. Only the host can set the native match
Options > Developer Tools Off/On selector (default Off). The shared network
adapter authenticates its session-key/IP-scoped, sequenced TSFTOOL1 heartbeat;
guests fail closed while joining, on expiry, or when the host/session changes.
When enabled, local camera/movement controls are available; Time & audio
and Player & progression remain disabled online. Revocation resets active local
flight state, and non-UI shortcuts follow the same policy.
Visibility toggles moved to Settings > View & HUD; host Tools policy preserves them.
See the Systems continuation for current contracts. The wrench
paragraphs below describe superseded implementation history.

Extras has no warning: slot17 remains empty and the submenu contains Status,
shortcut and Back. Developer Tools appends Audio follows game speed (default On) at row13;
its session state is independent of the selected/manual/held game speed.
pc_developer_audio_speed applies the normal opt-in/offline/playback guards and
returns1 when audio scaling is Off. Gameplay callbacks and XMV audio use this
getter; movie/video clocks continue using pc_developer_speed.

## Developer Tools overlay

Developer Tools is opt-in (`developer_tools_enabled`, absent=false, in controls
JSON). Extras opens a native Developer Tools submenu with Enable/Disable selector,
the assigned shortcut and Back. The warning lives in the sidebar footer. Its owned descriptor uses
the cached per-menu tick (menu+C, copied by native78E50). Descriptor+8/+12
remain empty lifecycle slots; 75B70 and the Back row own navigation. A90-unit
frame and short Status label preserve native selector layout. Host action
`developer_tools` is appended after weapon slots (PC_DEVELOPER_ACTION); default
F1 migrates only when absent and unclaimed. Native Keyboard & Mouse includes it
under Menu actions; the Windows/Cocoa editors and overlay share PC_ACTION_COUNT=37.
Actions35/36 are configurable no-clip/freecam toggles, default unbound; rising
edges run only while the overlay is closed and normal input owns capture.
Input capture must bypass the shortcut during rebinding. Disabled tools reset
active cheats/flight/HUD; F2/F12 remain available. Native cinematic98E50 uses
its fractional game-delta branch while Tools is enabled offline; the borrowed
6D60F0 flag is restored and the real-clock origin rebased before returning,
so disabling Tools preserves position. Original pause/end checks stay active.
The native real-clock branch otherwise ignores scaled game delta. XMV time integrates the
requested speed and both audio adapters change the existing movie voice rate,
preserving queued PCM. All other voices follow effective game speed in-place;
separate QPC/tick media clocks keep buffer/stream deadlines aligned, including SDL
fallback playback. Delayed worker timestamps cannot rewind the media clock. Freecam's own-body draw temporarily bypasses native
1265B0 near-eye fade bit2 and explicitly selects opaque blend0/alpha127; blendFF
inherits fade state. It marks the model header dirty on entry and restoration,
then restores model draw style/effect flags afterward.
`pc_developer_lobby_guest.inc` uses native 353760 roster names and peer IPs;
compatibility-framed TSFDEV1 metadata announces availability, never cheat state.
Only admitted roster IPs with the current host lobby session key enter the
expiring cache. Native role/host index774040 identifies that session. Hosts use
their owned registered key; guests resolve the joined host key. Neither sender
nor receiver requires a per-guest XNADDR discovery entry. Retiring a host key
clears cached notices, including when stale self-discovery still exists.
LAN and Online bridge envelopes share validation; names are copied from the
native roster. The timing boundary polls host/guest lobby states11/12, even
when local Tools is disabled. Native353760 return198FD6 captures each name slot;
41E30 return199583 appends a vector wrench after the measured native name,
only for marked slots in lobby states11/12. The painter restores its row index
to EBX before this call; match it to the captured slot and measure the actual
display label, including native decorated copies. The original name stays unchanged.
The former named footer notice remains implemented but disabled for future reuse.

`src/interface/pc_developer.c` and `pc_developer_sidebar.inc` own F1 sidebar navigation, nested PC Settings,
flight state and value-only developer requests. Windows/SDL adapters own host
input/capture/fullscreen/log-folder operations. The old Windows bar is hidden;
its saved JSON is preserved. F2/F12 dialogs remain recovery routes.
`pc_developer_guest.inc` handles camera work at original CCAB0 and Story requests
at the timing boundary. Noclip publishes feet/eye/history through D2DE0;
both flight modes rebuild the final view after scripted cameras. Freecam
starts at the player eye and consumes look without moving character angles. Flight uses
configured movement and Interact/Crouch bindings, player yaw/pitch conventions,
mouse sensitivity/inversion and Shift boost. No-clip permits native weapon input;
free-cam alone blocks character input. Gameplay changes are offline-only.
Manual speed is separate from held [/] targets; an opt-in permits them outside
the overlay, release/focus loss restores manual speed. God mode rejects owned
local BB6C0 damage; 1B66E0 preserves shot accounting with zero ammo consumption,
1BC650 restores spent grenade reserve. A0130 retains native clip flags while
including the owned actor/attachments in the detached view. Before HUD/AO,
`pc_developer_draw_body` submits the owned world body through native A2740.
It clears only this view's prepared bit so A1B80 rebuilds colour bone matrices:
native first-person preparation can leave the bit set with a null colour pointer.
Native animation, lighting, materials and depth testing remain in use. 1B9120
omits the owned first-person viewport draw. Patch2530 follows2520.
Game speed scales game intervals and carried ticks, retaining raw input,
presentation and ISR clocks. HUD draw suppression retains original HUD logic.
Windows bounded Story/menu acceptance and remaining native-host limits are in STATUS.

[Settings input/layout](controls-options.md).

## Current migration checks

Historical fixture inputs exclude Tools without its Linux/display prerequisites.
Complete upgrades still install all patches. EOF wrapper matching uses -C1 for
2520/2530/2550/2580 and rejects modified wrapper code; appended unrelated comments
survive unwind/reapply. The patch-updates fixture exercises both outcomes.

## Retained runtime contracts after main sync

[Further contracts and validation notes](runtime-continuation.md).

Sidebar Video Settings uses a shared-source guest-frame provider registered by
`pc_display_camera`, called through `pc_developer_guest_frame`. It adds no
generated guest edits or runtime patch; value-only mailbox contracts and tests
are described in the Systems continuation.

Match Tools selector placement: the existing79D60 wrapper inserts choice121
before native Music at US caller2C8010, while compact22100 typography/spacing
is active. The2C78F0 wrapper scopes menu ownership and the real host mode; it
no longer appends after Go. The native Music builder then resumes its own
selector context. No generated-code or patch edits are needed.

Developer Tools migration integration: historical fixtures omit the Tools family
when its Linux/display prerequisites are absent; complete upgrades still install
the entire current stack. Additive manual-file wrappers2520/2530/2550/2580 use
existing -C1 matching so appended local comments survive reverse application.
Patched code must still match exactly; the small EOF fixture rejects a changed
wrapper body without writing any files. Runtime patch bytes remain unchanged.

## Native free-camera input sampling

Additive input patch2630 follows2620 and routes native keyboard sampling through
the existing guarded `pc_controls_key_down`, instead of the raw SDL key reader.
The existing `pc_controls_accept_pad` gate now applies to both host samplers,
including explicit pad assignments and automatic P1 fallback. The native build
includes the shared controls declarations. No guest ABI/wrapper or generated
code changes are needed. See Systems for free-camera input ownership.


## November 6 main synchronization

The rebase retains upstream multiplayer exit declarations, weapon FOV and input
icon adapters. The Video Options fixture uses upstream network availability and
adds the existing Tools stubs alongside native icon/profile helpers. Shared Tools
behavior, Retina pointer mapping and native free-camera input guards are retained.
Fresh patch application and repeated application, native lobby wrappers, settings
mailbox and sidebar input remain the integration validation routes.
