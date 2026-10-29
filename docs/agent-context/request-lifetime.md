# Request callback lifetime prerequisites

Stage0221 maintains original request/callback lifetime support in shared source.
It extends `guest_io_owner.c/.h` and `guest_callback_call.c/.h`, adds
`guest_io_scope.c/.h` and `guest_io_devices.c/.h`, and selects the two new sources
once through `patches/compiler/0221-independent-request-lifetime.patch`.
The patch follows0220 and precedes2090. Historical migration inputs omit0221
with0219/0220; their complete upgrades assert all providers are restored.

This is groundwork for replacing inherited driver dispatch. It does not reroute
IofCallDriver/CompleteRequest, synchronous builders, queues, cancellation or disc
backends. Existing single-argument callback behavior is retained. No external
implementation bodies were copied. Full toolkit replacement remains unfinished.

## Ownership and callbacks

Initial dispatch owns one operation pin and one callback pin. Forwarding an
ACTIVE/PENDING operation adds just a callback pin. Host-only noncopyable tickets
start private and publish immediately before guest entry. Initial private rollback
restores READY and removes both pins; forward rollback removes its callback pin
and preserves the parent phase, including intervening completion. Published calls
cannot roll back. A guest nonlocal exit drops the callback pin and leaves pending
work owned; abandoned completion retains an explicitly UNCERTAIN operation.

The object-scope lifetime hook must unwind frames before native guest longjmp
abandons their C storage. No metadata lock spans guest callbacks. Existing
normal-return wrappers compose the same owner API. One attachment transfers only
while READY and retires outside the request lock before mapping/control disposal.
Disposal captures everything needed before cleanup and never touches a freed owner.
Callers still need a valid client/internal ref and stable serialized host context.

Device collections retain one lease per distinct body in bounded1..127 persistent
host entries. Caller holds a request client ref during init/pin and serializes
entry mutation. No abandoned C-stack metadata may back pending ownership.
The retired callback may free clean entries; uncertain releases must be accounted
or quarantined, never blindly retried. These helpers do not create guest authority.

The bounded stdcall helper passes0..4 guest words, captures returned EAX and
restores the project's GPR/SEH/TIB/XMM/MMX/x87/DF/indirect-call cells on normal
return. A host-only prepared plan resolves allowlisted entry/full stack mapping
without modifying guest state. Invoke consumes it, validates unchanged ESP/layout,
and admits ownership before guest writes/entry. Admission is a normal-returning,
metadata-only hook preserving guest state and stable mappings. Result changes
only on balanced return. Native continuation deliberately bypasses restoration.

## ABI boundary and validation

Stage0221 installs the ABI-independent lifetime/callback prerequisites. Stage0222
now selects the [original call composer and target decoder](driver-call.md) in
the library, after removing the unsupported+20 device tag. Its stack codec keeps
the remaining20 bytes opaque. Actual kernel bridge/completion/queue integration
remains unfinished; neither a library selection nor a byte round-trip test proves
the detailed per-operation Xbox parameter layout.

`tests/compiler/test_io_lifetime.py` runs strict compiled baselines and executable
fault controls for actual native guest jumps, inline completion, private rollback,
forwarding/shared operation pins, completion during private admission, prepared
callbacks, argument/state/result guards and pending device retention/quarantine.
It is selected explicitly on both native CI hosts. Existing owner, pool/registry,
Windows concurrency, actual kernel-header and single-argument callback suites
remain required. The source-including device fixture intentionally interposes
release failure and must not also link a second copy of that source.

Build/archive selection, preserved production behavior, isolated Windows gameplay
and native Mac execution are separate acceptance claims. Measured outcomes and
remaining platform gaps belong in STATUS and MAC-118.
