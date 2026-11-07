# Runtime guide continuation

[Main guide](runtime.md).

## Replacement texture frame completion

`patches/graphics/textures/0360-texture-upload-streaming.patch` calls `tsfp_texture_frame_complete` from common
`present_resolve`, guarded by TSFP_TEXTURE_PACK. This covers the title
`dev_Swap` path, the older `dev_Present` path and the public frame helper. Legacy GL calls
the same shared queue pump from its own Present adapter. This completes
background CPU reads on the renderer thread with one replacement upload per
frame. It does not change guest texture dimensions/data, update clocks or frame
rate selection. Source lives in `texture_pack_stream.h` and the existing loader
adapters; no generated guest edits or new renderer thread owns GPU resources.

Replacement completion/cancellation increments a binding revision. The common
renderer checks that revision alongside mode before draws, so textures left
bound across frames see arrivals and queue-full retries without rebinding every
draw. The streaming patch must follow 0350-texture-toggle.patch, which creates this
binding helper. The texture runtime fixture compiles that actual patched helper.

Keep this patch LF on disk before refreshing its index. A CRLF working copy
produced a different hash/target spelling from the normalized Git blob; the
Linux CI context check exposed it. Runtime behavior was unaffected.

Future Perfect reaches host_Swap from its HLE shadow frame boundary. A pump in
dev_Present alone does not run for that path. Keep the completion call in the
common resolve function, after the skipped-Swap guard; tests execute both actual
entry bodies and verify one completion per presented frame, none for a skip.

Standalone translator consumers gate live-register header/declarations on the supported protocol; see [register contracts](gpr-refs.md).

Original x87 emission now joins SIMD in the additive translator stack; see
[independent emitter contracts](independent.md#original-x87-emission). Storage
and FP environment remain upstream; game acceptance is tracked separately.

The additive `independent-x87-fixture.patch` exports the inherited classification
C fixture and includes production SET_LO16; it does not alter generated code.

The control-transfer emitter now follows x87 in the additive stack; call ABI,
local dispatch and table-boundary contracts are in [independent.md](independent.md).


Ordinal81 repair/test: [next-packet ABI](kernel-objects.md#guest-next-packet-abi-prerequisite).


Request allocation/ownership through0218: [contracts](kernel-objects.md#original-request-storage-and-initialization).

## Profile early-access hooks

See [Unlock Content](unlock-content.md) for the shared menu, profile extension,
original predicate hooks and isolated validation routes.


Original sharing bridges and ordinal80 cleanup: [contracts and checks](kernel-sharing.md).


Original request lifetime/callback prerequisites: [contracts and ABI boundary](request-lifetime.md).


Stage0222: [original driver-call composition and opaque stack payload](driver-call.md).


Stage0223: [private request construction and completion delivery](transfer-completion.md).

Stage0224: [host-owned FIFO request scheduling](transfer-completion.md#host-owned-request-queue).

Stage0225: [StartIo callback composition and acceptance](transfer-completion.md#startio-callback-composition); maintained maps include the new provider.

Stage0226: [exclusive waiting membership and device queue mirrors](transfer-completion.md#device-queue-mirrors).

Stage0227: [persistent device queue lifetime and scoped scheduler APIs](transfer-completion.md#persistent-device-queue-lifetime).

Stage0228: [read-only caller-owned request adoption and range lifetime](transfer-completion.md#caller-owned-request-adoption).

Stages0229/0231: [events and targets](transfer-completion.md). Stage0232: [symbolic links](symbolic-links.md).

Stages0233/0234: [routing](guest-paths.md), [original host storage](host-storage.md).

`2080-tank-driver-controls.patch` wraps 1730E0 through shared
`pc_tank_guest.inc`. The local keyboard/mouse scope selects signed track input
and restores actor links after the original call; controller behavior retains
the original camera-relative branch. See [scope guards](mounted-mouse.md).

Stage0235: [original pool imports and native surface retirement](kernel-pool.md).

Stage0236: [original absolute-time codec and guest calendar imports](kernel-time.md).

## Native Online integration (2026-10-30)

`network/2140-native-online.patch` follows the historical 2090 layout migration
in `patch_records`. It links shared `pc_network.c` and the Cocoa process adapter
into the native title, enables Objective-C in that directory, and defines
`TSFP_NATIVE_NETWORK` for the guest dispatch, lobby and bot hooks. SDL3 and Cocoa
remain host dependencies. Unsupported fixture builds retain original guest calls.
The patch stack must be applied before generating the shared compatibility identity.

Stage0237: [original counted strings and memory prefixes](kernel-strings.md).

Stage0238: [original critical sections and whole native RTL retirement](kernel-rtl.md).

Stage0239: [original checked narrow guest formatting](guest-printf.md).

Stage0240: [original numeric conversions](guest-numbers.md).

Crash model traversal instrumentation at US guest `0x583E0` is introduced by
`patches/diagnostics/2140-model-tree-trace.patch`. The manual wrapper preserves
original execution and records bounded model/node/depth history through the shared
capture adapters. It is diagnostic instrumentation, not a fix for #410. See
[crash evidence](crash-evidence.md) for capture scope and platform limits.

Allocation-history hook details: [crash evidence](crash-evidence.md). The shared
allocator observer is diagnostic-only; backing allocation ownership and guest
model suballocation lifetime remain distinct concepts.

Patches2290/2291 scope Assault marker US142D10 and fit its proportional UI around
the first outline draw's projected midpoint. Shared PcUiWorldAnchor retains that
anchor for labels and resets at placement boundaries; viewport/scissor share it.
Both common backends and legacy GL consume the helper. Original world projection,
visibility and objective logic remain guest-owned (Refs #338).

## Console language and included packs

[Language policy, US startup gate, original-routine tests and host validation](language-packs.md).


Stage0241: [original extended product and ABI checks](guest-integer-product.md).

RTL: [arithmetic/memory](guest-memory-ops.md), [strings](guest-stack-trace.md); [host policy](execution-host.md).

2350: controls-options.md.
